#!/usr/bin/env python3
"""RID-01 RX1024 R3: receive-only, privacy-filtered four-source A/B/A logger, host revision v3.

Run with Python 3 in Kali only after the reviewed RX1024 R3 upload. Requires the existing
arduino-cli; no Python packages are needed. Close other serial monitors first.
The helper sends no serial commands, compiles/uploads nothing, and changes no
firmware or GNSS/BLE/reference settings. Opening a USB serial monitor can still
affect device connection state; inspect normal startup and the readiness gate.
The CLI's stdin pipe stays open without receiving any bytes; terminal keyboard
input is never forwarded to the device. Ctrl+C ends this host capture normally.

Keep the R111S powered/GPS-acquired, positions and settings unchanged, and the
RID-01 in PRIVATE mode. For the standard A/B/A, after READY_FOR_IDLE_BASELINE,
time 2 minutes idle manually;
use the touchscreen to START, time 4 minutes from the recorded START event,
then STOP/HOME. After POST_STOP_IDLE_CONFIRMED, time 2 minutes idle and Ctrl+C.
There are no automatic phase transitions or acceptance decisions. A 30-minute
ceiling prevents an abandoned capture from running indefinitely.

Only exact scalar schemas and explicitly mapped control events are printed
or saved. Unknown, overlong, non-ASCII and unterminated lines are discarded;
raw NMEA, coordinates, target identifiers and monitor stderr are not saved.
Timestamps are host receipt times relative to monitor launch, not UART fault
or device interrupt timestamps. GNSS/UART counters are boot-scoped; DIAG values
describe reporting intervals, so a transition interval can mix scan states.
GNSS "long" is the parser's overlong-line counter, not a UART-buffer-size or
overflow measurement. UART errors count notifications, not lost bytes.

Readiness requires two recent RX1024_R3 UART summaries with requested=1024,
set=1024 and uart_init=OK. Any bounded FAIL or mismatched configuration is
retained and permanently invalidates this capture, even if later reports
look correct. Old R2 UART lines are rejected and cannot establish readiness.
The expected source/application hashes printed by the host describe the
reviewed images; they are not firmware readback or device attestation.

Keep the R111S and three intended ESP32 SYSTEM beacons powered throughout the
agreed observation protocol (the default is 2/4/2 minutes). This logger
also reconstructs complete existing
one-second target summaries and final scan totals. Only bounded slot labels,
states, packet counts, ages and aggregate counters are retained; aircraft
names, identifiers, source addresses, coordinates and detailed snapshots are
never forwarded. No firmware, beacon interval or receiver setting is changed.

Target blocks are emitted only after their end marker and consistency checks.
All lines in a reconstructed block use its host completion timestamp, not a
device timestamp. Missing, malformed or interrupted blocks produce a fixed
notice and no partial slot data. A complete block describes allocated slots;
STALE/LOST slots remain tracked, and four slots do not prove the intended four
physical sources, continuous reception, or four fresh geographic positions.
The final RID count measures transport-qualified service-data fields before
allocation/decoding, not authenticated or fully validated aircraft messages.

The footer counts accepted/discarded input lines (including target-block
components), not emitted output lines; complete/incomplete block counts are
reported separately. V2 adds only bounded structural rejection context: fixed
expected/observed field codes, parser position, slot/count context, byte length
and ASCII/control counts. No rejected raw text, identifiers, coordinates or
hashes of rejected text are retained. These diagnostics do not identify a
proven cause of console loss or relax the complete-block acceptance rules.
Input lengths exclude LF and include any CR; NA denotes an unavailable metric.

V3 adds a parallel exact allowlist for existing SNAPSHOT, BACK TO LIVE, view-tab
and target-selection messages. [UI-ACTION] records contain only fixed enums and
T01-T04 slots, with host receipt timestamps. The firmware messages precede screen
redraw; they do not timestamp touch contact or prove screen completion. SNAPSHOT
is logged after its data copy but before the screen-state assignment/redraw.
Ignored/unavailable actions are identified separately. No screen timing, phase
transition or acceptance is inferred automatically. For a screen-control test,
follow the agreed timing protocol; MANUAL_TIMING is guidance only.
Every UI line still passes through the unchanged v2 filter after this parallel
observation. A UI line inside an open target/final block can therefore still
produce the same incomplete-block notice. Accepted/discarded input counts keep
the v2 meaning; ui_action_records is a separate count of new observation records.
No rejected raw text or detailed snapshot data is forwarded.
Previous logs are preserved: every run uses a distinct
UTC-microsecond name opened exclusively, with mode 0600.
"""

from __future__ import annotations

import datetime as dt
from collections import deque
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import subprocess
import sys
import time


PORT = "/dev/ttyACM0"
HELPER_VERSION = "RX1024_R3_4SRC_v3"
EXPECTED_CANDIDATE = "RX1024_R3"
EXPECTED_RX_BYTES = 1024
EXPECTED_SOURCE_SHA256 = "bdad0c79b6d08b4a622348e10c2c0234cff2005033edd6404e2d536d2a13f607"
EXPECTED_APPLICATION_SHA256 = "f0f2721406bbffe3b601a7f2f31b389716f5aca8bc71b4733784bd6a846d3b07"
READ_BYTES = 4096
MAX_LINE_BYTES = 512
MAX_CAPTURE_SECONDS = 1800.0
RECENT_SECONDS = 15.0
HEALTH_SECONDS = 30.0
UINT32_MAX = 4294967295
UINT = r"[0-9]{1,10}"
GNSS_FIELDS = ("bytes", "lines", "nmea_ok", "checksum_bad", "malformed", "long", "GGA", "RMC")
DIAG_FIELDS = ("gap_peak_ms", "pending_peak", "pending_ge128", "calls")
UART_FIELDS = ("buf_full", "fifo_ovf", "frame", "parity", "break", "other")
CONFIG_FIELDS = ("rx_requested", "rx_set")
CONFIG_SUFFIX = (r" candidate=(?P<candidate>RX1024_R3) " +
                 " ".join(name + "=(?P<" + name + ">" + UINT + ")" for name in CONFIG_FIELDS) +
                 r" uart_init=(?P<uart_init>OK|FAIL)")
RXCFG_RE = re.compile(r"\[GNSS-D3B2-RXCFG\]" + CONFIG_SUFFIX +
                      r"(?: reason=(?P<reason>RX_SIZE|UART_INIT))?")


def numbers(fields: tuple[str, ...]) -> str:
    return " ".join(name + "=(?P<" + name + ">" + UINT + ")" for name in fields)


SCHEMAS = (
    ("GNSS", "[GNSS-D3B1]", GNSS_FIELDS,
     re.compile(r"\[GNSS-D3B1\] " + numbers(GNSS_FIELDS)
                + r" FIX=(?P<FIX>FRESH|STALE|NO_FIX) SATS=(?P<SATS>[0-9]{1,2})")),
    ("DIAG", "[GNSS-D3B2-DIAG]", DIAG_FIELDS,
     re.compile(r"\[GNSS-D3B2-DIAG\] scan=(?P<scan>ACTIVE|IDLE) " + numbers(DIAG_FIELDS))),
    ("UART", "[GNSS-D3B2-UARTERR]", UART_FIELDS,
     re.compile(r"\[GNSS-D3B2-UARTERR\] scan=(?P<scan>ACTIVE|IDLE) " + numbers(UART_FIELDS) + CONFIG_SUFFIX)),
)
ROLLOVER = re.compile(r"\[SCAN\] Continuous RID window rollover -> #(?P<window>" + UINT + ")")
EVENTS = {
    "[UI] START SCAN -> TARGETS / backend ACTIVE.": "SCAN_START",
    "[UI] STOP / HOME requested.": "SCAN_STOP_REQUEST",
    "=== RID CAPTURE STOPPED BY USER ===": "SCAN_STOPPED",
    "[UI] STOP / HOME: receiver already idle.": "STOP_ALREADY_IDLE",
    "[UI] BEGIN -> OUTPUT MODE / backend PRIVATE / scanner IDLE.": "BEGIN_PRIVATE_IDLE",
    "[UI] Output mode -> PRIVATE.": "MODE_PRIVATE",
    "[UI] Output mode -> OPEN.": "MODE_OPEN",
    "[UI ERROR] START SCAN failed; remaining on OUTPUT MODE.": "START_FAILED",
    "[UI ERROR] STOP request failed.": "STOP_FAILED",
    "[ERROR] Asynchronous RID scan window failed to start.": "SCAN_START_FAILED",
    "[ERROR] Continuous RID rollover failed; scan session stopped.": "ROLLOVER_FAILED",
    "[ERROR] BLE scanner is not initialized.": "BLE_NOT_INITIALIZED",
    "[UI ERROR] BLE scanner is not initialized.": "BLE_NOT_INITIALIZED",
    "[ACK] BLE START blocked.": "START_BLOCKED_ACK",
    "[UI] START ignored: RID session already active.": "START_IGNORED_ACTIVE",
    "=== RID CAPTURE ENDED BY CONTROLLER ===": "ENDED_BY_CONTROLLER",
    "[GNSS-D3B1] UART1 RX=GPIO44 / 115200 / receive-only": "GNSS_UART_INIT",
}


# Exact existing messages from wsProcessButton() in the reviewed R3 source.
# This is a parallel observer; it must not bypass the original line filter.
UI_ACTION_FIXED = {
    "[UI] SNAPSHOT -> BACK TO LIVE.": "action=BACK_TO_LIVE outcome=REQUEST_ACCEPTED",
    "[UI-SHELL] VIEW -> TARGETS": "action=VIEW_TARGETS outcome=REQUEST_ACCEPTED",
    "[UI-SHELL] VIEW -> RADAR": "action=VIEW_RADAR outcome=REQUEST_ACCEPTED",
    "[UI-SHELL] RADAR unavailable: ACQUIRING geographic prerequisites.":
        "action=VIEW_RADAR outcome=UNAVAILABLE",
    "[UI] SNAPSHOT ignored: no active RID session.":
        "action=SNAPSHOT outcome=IGNORED reason=NO_ACTIVE_SESSION",
}
UI_SNAPSHOT = re.compile(
    r"\[UI\] SNAPSHOT (?P<slot>T0[1-4]) (?P<snapshot>FROZEN|EMPTY) / (?P<mode>PRIVATE|OPEN)")
UI_SELECT = re.compile(r"\[UI-SHELL\] SELECT (?P<slot>T0[1-4])")


def parse_ui_action(raw: bytes):
    """Return a reserialized bounded action only; never return unknown text."""
    if len(raw) > MAX_LINE_BYTES:
        return None
    if raw.endswith(b"\r"):
        raw = raw[:-1]
    try:
        line = raw.decode("ascii")
    except UnicodeDecodeError:
        return None
    body = UI_ACTION_FIXED.get(line)
    if body is None:
        match = UI_SNAPSHOT.fullmatch(line)
        if match is not None:
            body = ("action=SNAPSHOT outcome=REQUEST_ACCEPTED slot=" + match["slot"]
                    + " snapshot=" + match["snapshot"] + " mode=" + match["mode"])
        else:
            match = UI_SELECT.fullmatch(line)
            if match is None:
                return None
            body = "action=SELECT_TARGET outcome=REQUEST_ACCEPTED slot=" + match["slot"]
    return ("[UI-ACTION] " + body
            + " timestamp_basis=host_receipt render_completion=NOT_OBSERVED")


def parse_line(raw: bytes):
    """Return only typed, bounded allowlisted data; never return unknown text."""
    if len(raw) > MAX_LINE_BYTES:
        return None
    if raw.endswith(b"\r"):
        raw = raw[:-1]
    try:
        line = raw.decode("ascii")
    except UnicodeDecodeError:
        return None
    for kind, prefix, fields, pattern in SCHEMAS:
        match = pattern.fullmatch(line)
        if not match:
            continue
        values = match.groupdict()
        for name in fields + (CONFIG_FIELDS if kind == "UART" else ()):
            values[name] = int(values[name])
            if values[name] > UINT32_MAX:
                return None
        if kind == "GNSS":
            values["SATS"] = int(values["SATS"])
            body = " ".join(f"{name}={values[name]}" for name in fields)
            body += f" FIX={values['FIX']} SATS={values['SATS']}"
        else:
            body = f"scan={values['scan']} " + " ".join(f"{name}={values[name]}" for name in fields)
        if kind == "UART":
            body += (f" candidate={EXPECTED_CANDIDATE} rx_requested={values['rx_requested']}"
                     f" rx_set={values['rx_set']} uart_init={values['uart_init']}")
        return kind, values, prefix + " " + body
    match = RXCFG_RE.fullmatch(line)
    if match:
        values = match.groupdict()
        for name in CONFIG_FIELDS:
            values[name] = int(values[name])
            if values[name] > UINT32_MAX:
                return None
        # The reviewed firmware emits a reason only for a failed init.
        if ((values["uart_init"] == "OK" and values["reason"] is not None)
                or (values["uart_init"] == "FAIL" and values["reason"] is None)):
            return None
        body = (f"[GNSS-D3B2-RXCFG] candidate={EXPECTED_CANDIDATE}"
                f" rx_requested={values['rx_requested']} rx_set={values['rx_set']}"
                f" uart_init={values['uart_init']}")
        if values["reason"] is not None:
            body += f" reason={values['reason']}"
        return "RXCFG", values, body
    match = ROLLOVER.fullmatch(line)
    if match:
        window = int(match.group("window"))
        if window <= UINT32_MAX:
            return "EVENT", {"event": "ROLLOVER", "window": window}, f"[EVENT] ROLLOVER window_started={window}"
        return None
    event = EVENTS.get(line)
    if event is not None:
        return "EVENT", {"event": event}, "[EVENT] " + event
    return None


def matching_configuration(values):
    return (values.get("candidate") == EXPECTED_CANDIDATE
            and values.get("rx_requested") == EXPECTED_RX_BYTES
            and values.get("rx_set") == EXPECTED_RX_BYTES
            and values.get("uart_init") == "OK")


class Sink:
    def __init__(self, logfile, console, origin):
        self.logfile, self.console, self.origin = logfile, console, origin

    def stamp(self):
        return dt.datetime.now(dt.timezone.utc), max(0.0, time.monotonic() - self.origin)

    def emit(self, message, stamp=None):
        utc, elapsed = self.stamp() if stamp is None else stamp
        timestamp = utc.isoformat(timespec="milliseconds").replace("+00:00", "Z")
        line = f"{timestamp} elapsed_s={elapsed:.3f} {message}\n"
        self.logfile.write(line)
        self.logfile.flush()
        self.console.write(line)
        self.console.flush()


class Observation:
    def __init__(self, sink):
        self.sink = sink
        self.latest = {}
        self.previous = {}
        self.samples = {"GNSS": deque(maxlen=2), "UART": deque(maxlen=2)}
        self.ready_shown = False
        self.started = False
        self.operator_state = "UNKNOWN"
        self.stop_requested = False
        self.stopped_at = None
        self.post_idle_shown = False
        self.counter_warnings = 0
        self.config_invalid = False
        self.health_notices = set()

    def accept(self, record, stamp):
        kind, values, text = record
        elapsed = stamp[1]
        self.sink.emit(text, stamp)
        if kind in ("UART", "RXCFG") and not matching_configuration(values):
            if not self.config_invalid:
                self.sink.emit("[HOST] WARN configuration_invalid stream=" + kind
                               + " expected_candidate=RX1024_R3 expected_rx_requested=1024"
                               " expected_rx_set=1024 expected_uart_init=OK"
                               " run_invalid=1 stop_capture_and_review=1", stamp)
            self.config_invalid = True
        if kind in ("GNSS", "UART"):
            fields = GNSS_FIELDS if kind == "GNSS" else UART_FIELDS
            previous = self.previous.get(kind)
            if previous is not None:
                decreased = [name for name in fields if values[name] < previous[name]]
                if decreased:
                    self.counter_warnings += 1
                    self.sink.emit("[HOST] WARN counter_decrease stream=" + kind
                                   + " fields=" + ",".join(decreased)
                                   + " comparison_requires_review=1 run_invalid=1 stop_capture_and_review=1", stamp)
            self.previous[kind] = values.copy()
            self.samples[kind].append((elapsed, values.copy()))
        if kind in ("GNSS", "DIAG", "UART"):
            self.latest[kind] = (elapsed, values)
            self.health_notices.discard(("stale", kind))
        elif kind == "EVENT":
            event = values["event"]
            if event == "SCAN_START":
                self.started = True
                self.operator_state = "ACTIVE"
                self.stop_requested = False
                self.stopped_at = None
                self.post_idle_shown = False
            elif event == "SCAN_STOP_REQUEST":
                self.operator_state = "STOP_PENDING"
                self.stop_requested = True
            elif event == "SCAN_STOPPED":
                self.operator_state = "IDLE"
                if self.stop_requested:
                    self.stopped_at = elapsed
            elif event == "STOP_ALREADY_IDLE":
                self.operator_state = "IDLE"
            elif event == "MODE_OPEN":
                self.sink.emit("[HOST] WARN mode_open_observed protocol_requires_private=1", stamp)
            elif event.endswith("FAILED") or event in ("BLE_NOT_INITIALIZED", "ENDED_BY_CONTROLLER"):
                self.sink.emit("[HOST] WARN scan_control_event=" + event
                               + " comparison_requires_review=1", stamp)
        self.check_readiness(stamp)

    def check_readiness(self, stamp):
        now = stamp[1]
        if self.counter_warnings or self.config_invalid:
            return
        if not all(name in self.latest for name in ("GNSS", "DIAG", "UART")):
            return
        if any(len(self.samples[name]) < 2 for name in ("GNSS", "UART")):
            return
        observations = list(self.samples["GNSS"]) + list(self.samples["UART"]) + [self.latest["DIAG"]]
        if any(now - observed_at > RECENT_SECONDS for observed_at, _ in observations):
            return
        if any(values["FIX"] != "FRESH" for _, values in self.samples["GNSS"]):
            return
        first, last = self.samples["GNSS"][0][1], self.samples["GNSS"][1][1]
        if not (last["bytes"] > first["bytes"] and last["lines"] > first["lines"]):
            return
        if any(self.latest[name][1]["scan"] != "IDLE" for name in ("DIAG", "UART")):
            return
        if any(values["scan"] != "IDLE" for _, values in self.samples["UART"]):
            return
        if any(not matching_configuration(values) for _, values in self.samples["UART"]):
            return
        if self.operator_state in ("ACTIVE", "STOP_PENDING"):
            return
        if not self.ready_shown and not self.started:
            self.ready_shown = True
            self.sink.emit("[HOST] READY_FOR_IDLE_BASELINE begin_manual_2_minute_baseline=1"
                           " candidate=RX1024_R3 rx_requested=1024 rx_set=1024 uart_init=OK"
                           " gnss_progress_observed=1 idle_uart_reports=2"
                           " startup_and_setup_require_operator_check=1", stamp)
        if (self.stopped_at is not None and not self.post_idle_shown
                and all(self.latest[name][0] > self.stopped_at for name in ("GNSS", "DIAG", "UART"))):
            self.post_idle_shown = True
            self.sink.emit("[HOST] POST_STOP_IDLE_CONFIRMED begin_manual_2_minute_idle=1"
                           " candidate=RX1024_R3 rx_requested=1024 rx_set=1024 uart_init=OK"
                           " first_diag_interval_may_mix_states=1", stamp)

    def check_health(self, stamp):
        now = stamp[1]
        for kind in ("GNSS", "DIAG", "UART"):
            previous = self.latest.get(kind)
            if previous is None and now >= HEALTH_SECONDS:
                key = ("missing", kind)
                if key not in self.health_notices:
                    self.health_notices.add(key)
                    self.sink.emit("[HOST] NOTICE missing_stream=" + kind
                                   + " threshold_s=30 readiness_not_established=1", stamp)
            elif previous is not None and now - previous[0] > HEALTH_SECONDS:
                key = ("stale", kind)
                if key not in self.health_notices:
                    self.health_notices.add(key)
                    self.sink.emit("[HOST] WARN stale_stream=" + kind
                                   + f" age_s={now - previous[0]:.1f}"
                                   + " comparison_requires_review=1", stamp)



# Existing receiver console structures, reconstructed without retaining raw text.
# See printTargetSummary(), finalizeRIDScan() in the reviewed RX1024 R3 source.
TARGET_BLOCK_TIMEOUT_SECONDS = 5.0
TARGET_HEADER = "========== RID TARGET SUMMARY =========="
TARGET_END = "========================================"
FINAL_END = "=== END RID CAPTURE ==="
UA_TYPE_NAMES = (
    "NONE", "AEROPLANE", "HELICOPTER / MULTIROTOR", "GYROPLANE", "HYBRID LIFT",
    "ORNITHOPTER", "GLIDER", "KITE", "FREE BALLOON", "CAPTIVE BALLOON", "AIRSHIP",
    "FREE FALL / PARACHUTE", "ROCKET", "TETHERED POWERED AIRCRAFT", "GROUND OBSTACLE",
    "OTHER", "UNKNOWN",
)
TYPE_NAMES = {0: "BASIC ID", 1: "LOCATION / VECTOR", 2: "AUTHENTICATION", 3: "SELF ID",
              4: "SYSTEM", 5: "OPERATOR ID", 15: "MESSAGE PACK"}
TARGET_SCHEMAS = (
    ("MODE", re.compile(r"Output Mode: (?P<mode>PRIVATE|OPEN)")),
    ("TRACKED", re.compile(r"Targets Tracked: (?P<tracked>[0-4])")),
    ("ACTIVE", re.compile(r"Targets Active: (?P<active>[0-4])")),
    ("SLOT", re.compile(r"T0(?P<slot>[1-4])  (?P<state>ACTIVE|STALE|LOST|EMPTY)  (?:"
                        + "|".join(re.escape(x) for x in UA_TYPE_NAMES) + r")")),
    ("FLIGHT", re.compile(r"State: (?:UNDECLARED|GROUND|AIRBORNE|EMERGENCY|UNKNOWN)")),
    ("POSITION", re.compile(r"Position: (?:VALID|UNKNOWN)")),
    ("RSSI", re.compile(r"RSSI: (?P<rssi>-?[0-9]{1,3}) dBm")),
    ("PACKETS", re.compile(r"Packets: (?P<packets>" + UINT + r")")),
    ("AGE", re.compile(r"Last Seen: (?P<last_seen_ms>" + UINT + r") ms ago")),
    ("ELAPSED", re.compile(r"Capture elapsed ms: (?P<capture_elapsed_ms>" + UINT + r")")),
    ("WINDOWS", re.compile(r"Continuous BLE scan windows started: (?P<windows_started>" + UINT + r")")),
    ("ADVERTISEMENT", re.compile(r"Total BLE advertisement callbacks: (?P<advertisement_callbacks>" + UINT + r")")),
    ("RID", re.compile(r"Validated Remote ID packets: (?P<transport_rid_fields>" + UINT + r")")),
    ("ALLOCATION", re.compile(r"Target Allocation Failures: (?P<allocation_failures>" + UINT + r")")),
    ("TYPE", re.compile(r"Type (?P<type>[0-9]|1[0-5]) \((?P<type_name>BASIC ID|LOCATION / VECTOR|AUTHENTICATION|SELF ID|SYSTEM|OPERATOR ID|MESSAGE PACK|UNKNOWN / RESERVED)\): (?P<count>" + UINT + r")")),
)


def parse_target_line(raw):
    """Classify only bounded, fixed console tokens; unknown text never leaves here."""
    if raw is None or len(raw) > MAX_LINE_BYTES:
        return None
    if raw.endswith(b"\r"):
        raw = raw[:-1]
    try:
        line = raw.decode("ascii")
    except UnicodeDecodeError:
        return None
    markers = {"": "BLANK", TARGET_HEADER: "DASHBOARD_BEGIN", TARGET_END: "DASHBOARD_END",
               "Remote ID message types observed:": "TYPE_HEADER", FINAL_END: "FINAL_END"}
    if line in markers:
        return markers[line], {}
    for kind, pattern in TARGET_SCHEMAS:
        match = pattern.fullmatch(line)
        if match is None:
            continue
        values = match.groupdict()
        for name in tuple(values):
            if name not in ("mode", "state", "type_name"):
                values[name] = int(values[name])
                if not (0 <= values[name] <= UINT32_MAX) and name != "rssi":
                    return None
        if kind == "RSSI" and not -128 <= values["rssi"] <= 127:
            return None
        if kind == "TYPE":
            if values["type_name"] != TYPE_NAMES.get(values["type"], "UNKNOWN / RESERVED"):
                return None
            del values["type_name"]
            if values["count"] == 0:  # The receiver prints only nonzero types.
                return None
        return kind, values
    return None


def structural_input_context(raw, token=None, raw_length=None):
    """Fixed categories/counts only; never retain raw content or its hash."""
    context = {"input_present": int(raw is not None or raw_length is not None),
               "observed": "NONE", "prefix": "NONE", "raw_len": 0,
               "raw_len_capped": 0, "ascii": "NA", "control_bytes": "NA", "cr_bytes": "NA"}
    if raw is None and raw_length is None:
        return context
    size = len(raw) if raw_length is None else raw_length
    context["raw_len"] = min(size, 65535)
    context["raw_len_capped"] = int(size >= 65535)
    context["observed"] = "UNKNOWN" if token is None else token[0]
    context["prefix"] = "UNKNOWN"
    if raw is not None:
        context["ascii"] = int(all(byte < 128 for byte in raw))
        context["control_bytes"] = sum(byte < 32 or byte == 127 for byte in raw)
        context["cr_bytes"] = raw.count(b"\r")
        prefixes = (
            (b"Output Mode:", "MODE"), (b"Targets Tracked:", "TRACKED"),
            (b"Targets Active:", "ACTIVE"), (b"State:", "FLIGHT"),
            (b"Position:", "POSITION"), (b"RSSI:", "RSSI"),
            (b"Packets:", "PACKETS"), (b"Last Seen:", "AGE"),
            (b"Capture elapsed ms:", "ELAPSED"),
            (b"Continuous BLE scan windows started:", "WINDOWS"),
            (b"Total BLE advertisement callbacks:", "ADVERTISEMENT"),
            (b"Validated Remote ID packets:", "RID"),
            (b"Target Allocation Failures:", "ALLOCATION"), (b"Type ", "TYPE"),
            (b"[GNSS-D3B1]", "GNSS"), (b"[GNSS-D3B2-DIAG]", "DIAG"),
            (b"[GNSS-D3B2-UARTERR]", "UART"),
        )
        for prefix, category in prefixes:
            if raw.startswith(prefix):
                context["prefix"] = category
                break
        if re.match(br"T[0-9]{1,2} ", raw):
            context["prefix"] = "SLOT"
    if token is not None:
        context["prefix"] = token[0]
    return context


class TargetBlocks:
    """Bounded reconstruction of complete existing dashboard/final blocks."""
    DASHBOARD_ORDER = ("MODE", "TRACKED", "ACTIVE")
    SLOT_ORDER = ("FLIGHT", "POSITION", "RSSI", "PACKETS", "AGE")
    FINAL_ORDER = ("ELAPSED", "WINDOWS", "ADVERTISEMENT", "RID", "TRACKED", "ALLOCATION", "TYPE_HEADER")

    def __init__(self, sink):
        self.sink = sink
        self.block = None
        self.completed = {"dashboard": 0, "final": 0}
        self.incomplete = {"dashboard": 0, "final": 0}
        self.max_tracked = 0
        self.scan_active = False
        self.active_since = None
        self.latest_dashboard = None
        self.missing_notice = False
        self.reject_subreason = "unspecified"
        self.reject_values = {}

    def block_context(self):
        block = self.block
        if block["kind"] == "dashboard":
            if block["stage"] < len(self.DASHBOARD_ORDER):
                expected = self.DASHBOARD_ORDER[block["stage"]]
            elif block["row"] is not None:
                expected = self.SLOT_ORDER[block["row_stage"]]
            else:
                expected = "SLOT_OR_DASHBOARD_END"
        elif block["stage"] < len(self.FINAL_ORDER):
            expected = self.FINAL_ORDER[block["stage"]]
        else:
            expected = "TYPE_OR_FINAL_END"
        return {"expected": expected, "block_stage": block["stage"],
                "row_stage": block["row_stage"] if block["row"] is not None else 0,
                "slot": "NONE" if block["row"] is None else f"T0{block['row']['slot']}",
                "rows_complete": len(block["rows"]),
                "declared_tracked": block["values"].get("tracked", "NA"),
                "declared_active": block["values"].get("active", "NA")}

    def reject(self, subreason, **values):
        self.reject_subreason = subreason
        self.reject_values = values
        return False

    def abandon(self, reason, stamp, context=None, subreason=None, details=None):
        if self.block is not None:
            kind = self.block["kind"]
            self.incomplete[kind] += 1
            fields = self.block_context()
            fields.update(structural_input_context(None) if context is None else context)
            fields["subreason"] = reason if subreason is None else subreason
            fields.update({} if details is None else details)
            diagnostic = " ".join(f"{name}={value}" for name, value in fields.items())
            self.sink.emit("[HOST] NOTICE incomplete_target_block kind=" + kind
                           + " reason=" + reason + " partial_slot_data_retained=0"
                           " target_evidence_requires_review=1 " + diagnostic, stamp)
            self.block = None

    def begin(self, kind, stamp, context=None):
        self.abandon("new_block", stamp, context, "replacement_block_started")
        self.block = {"kind": kind, "started": stamp[1], "stage": 0, "values": {},
                      "rows": [], "row": None, "row_stage": 0, "types": {}}

    def observe_record(self, record, stamp, context=None):
        kind, values, _ = record
        if kind != "EVENT":
            return
        event = values["event"]
        context = structural_input_context(None) if context is None else context.copy()
        context["observed"] = "EVENT_" + event
        context["prefix"] = "EVENT"
        if event == "SCAN_START":
            self.abandon("new_scan", stamp, context, "new_scan_started")
            self.scan_active = True
            self.active_since = stamp[1]
            self.latest_dashboard = None
            self.missing_notice = False
        elif event == "SCAN_STOP_REQUEST":
            self.abandon("stop_request", stamp, context, "scan_stop_requested")
            self.scan_active = False
        elif event in ("SCAN_STOPPED", "ENDED_BY_CONTROLLER"):
            self.scan_active = False
            self.begin("final", stamp, context)

    def check_health(self, stamp):
        if self.block is not None and stamp[1] - self.block["started"] > TARGET_BLOCK_TIMEOUT_SECONDS:
            self.abandon("timeout", stamp, subreason="block_deadline_exceeded")
        reference = self.latest_dashboard if self.latest_dashboard is not None else self.active_since
        if (self.scan_active and reference is not None and stamp[1] - reference > HEALTH_SECONDS
                and not self.missing_notice):
            self.missing_notice = True
            self.sink.emit("[HOST] WARN missing_complete_target_summary threshold_s=30"
                           " target_evidence_incomplete=1", stamp)

    def feed(self, raw, stamp, raw_length=None):
        self.check_health(stamp)
        token = parse_target_line(raw)
        context = structural_input_context(raw, token, raw_length)
        if token is None:
            if raw is None or len(raw) > MAX_LINE_BYTES:
                subreason = "overlong_line"
            elif context["ascii"] == 0:
                subreason = "non_ascii_line"
            elif context["control_bytes"] > int(raw.endswith(b"\r")):
                subreason = "unsupported_control_byte"
            elif context["prefix"] != "UNKNOWN":
                subreason = "schema_or_range_mismatch"
            else:
                subreason = "unrecognized_line"
            self.abandon("invalid_input", stamp, context, subreason)
            return False
        kind, values = token
        if kind == "DASHBOARD_BEGIN":
            if not self.scan_active:
                return False
            self.begin("dashboard", stamp, context)
            return True
        if self.block is None:
            return False
        if kind == "BLANK":
            return True
        self.reject_subreason = "unspecified"
        self.reject_values = {}
        if self.block["kind"] == "dashboard":
            valid = self.dashboard_token(kind, values, stamp)
        else:
            valid = self.final_token(kind, values, stamp)
        if not valid:
            self.abandon("unexpected_or_inconsistent_field", stamp, context,
                         self.reject_subreason, self.reject_values)
        return valid

    def dashboard_token(self, kind, values, stamp):
        block = self.block
        if block["stage"] < len(self.DASHBOARD_ORDER):
            if kind != self.DASHBOARD_ORDER[block["stage"]]:
                return self.reject("dashboard_header_order")
            block["values"].update(values)
            block["stage"] += 1
            return True
        if block["row"] is not None:
            if kind != self.SLOT_ORDER[block["row_stage"]]:
                return self.reject("slot_field_order")
            if kind in ("PACKETS", "AGE"):
                block["row"].update(values)
            block["row_stage"] += 1
            if kind == "AGE":
                block["rows"].append(block["row"])
                block["row"] = None
            return True
        if kind == "SLOT":
            if len(block["rows"]) >= 4:
                return self.reject("slot_capacity_exceeded", observed_slot=f"T0{values['slot']}")
            if len(block["rows"]) >= block["values"]["tracked"]:
                return self.reject("slot_rows_exceed_tracked", observed_slot=f"T0{values['slot']}")
            if block["rows"] and values["slot"] <= block["rows"][-1]["slot"]:
                return self.reject("slot_duplicate_or_out_of_order", observed_slot=f"T0{values['slot']}")
            block["row"] = values.copy()
            block["row_stage"] = 0
            return True
        if kind != "DASHBOARD_END":
            return self.reject("expected_slot_or_dashboard_end")
        summary = block["values"]
        if summary["tracked"] != len(block["rows"]):
            return self.reject("tracked_row_count_mismatch")
        calculated_active = sum(row["state"] == "ACTIVE" for row in block["rows"])
        if summary["active"] != calculated_active:
            return self.reject("active_row_count_mismatch", calculated_active=calculated_active)
        for row in block["rows"]:
            age = row["last_seen_ms"]
            expected = "ACTIVE" if age <= 3000 else "STALE" if age <= 10000 else "LOST"
            if row["state"] != expected:
                return self.reject("slot_state_age_mismatch", offending_slot=f"T0{row['slot']}",
                                   slot_state=row["state"], slot_age_ms=age, expected_slot_state=expected)
        self.completed["dashboard"] += 1
        snapshot = self.completed["dashboard"]
        self.max_tracked = max(self.max_tracked, summary["tracked"])
        span_ms = max(0, round((stamp[1] - block["started"]) * 1000))
        self.sink.emit(f"[RID-TARGETS-BEGIN] snapshot={snapshot} tracked={summary['tracked']}"
                       f" active={summary['active']} mode={summary['mode']}"
                       f" complete=1 receipt_basis=block_end receipt_span_ms={span_ms}", stamp)
        for row in block["rows"]:
            self.sink.emit(f"[RID-SLOT] snapshot={snapshot} slot=T0{row['slot']}"
                           f" state={row['state']} packets={row['packets']} last_seen_ms={row['last_seen_ms']}", stamp)
        self.sink.emit(f"[RID-TARGETS-END] snapshot={snapshot} slot_rows={len(block['rows'])} complete=1", stamp)
        if summary["mode"] != "PRIVATE":
            self.sink.emit("[HOST] WARN target_summary_mode_open protocol_requires_private=1", stamp)
        self.latest_dashboard = stamp[1]
        self.missing_notice = False
        self.block = None
        return True

    def final_token(self, kind, values, stamp):
        block = self.block
        if block["stage"] < len(self.FINAL_ORDER):
            if kind != self.FINAL_ORDER[block["stage"]]:
                return self.reject("final_field_order")
            block["values"].update(values)
            block["stage"] += 1
            return True
        if kind == "TYPE":
            number = values["type"]
            if number in block["types"]:
                return self.reject("duplicate_message_type", observed_type=number)
            if block["types"] and number <= max(block["types"]):
                return self.reject("message_type_order", observed_type=number)
            block["types"][number] = values["count"]
            return True
        if kind != "FINAL_END":
            return self.reject("expected_message_type_or_final_end")
        self.completed["final"] += 1
        number = self.completed["final"]
        fields = ("capture_elapsed_ms", "windows_started", "advertisement_callbacks",
                  "transport_rid_fields", "tracked", "allocation_failures")
        body = " ".join(f"{field}={block['values'][field]}" for field in fields)
        self.sink.emit(f"[RID-FINAL-BEGIN] summary={number} complete=1 receipt_basis=block_end " + body, stamp)
        for message_type, count in block["types"].items():
            self.sink.emit(f"[RID-MESSAGE-TYPE] summary={number} type={message_type} count={count}", stamp)
        self.sink.emit(f"[RID-FINAL-END] summary={number} type_rows={len(block['types'])} complete=1", stamp)
        self.block = None
        return True

    def finish(self, context=None):
        self.abandon("capture_end", self.sink.stamp(), context, "capture_ended_before_block_end")


class LineFilter:
    def __init__(self, observation):
        self.observation = observation
        self.targets = TargetBlocks(observation.sink)
        self.pending = bytearray()
        self.overlong = False
        self.raw_length = 0
        self.accepted = 0
        self.discarded = 0
        self.ui_action_records = 0

    def append(self, part):
        self.raw_length = min(65535, self.raw_length + len(part))
        if self.overlong:
            return
        if len(self.pending) + len(part) > MAX_LINE_BYTES:
            self.pending.clear()
            self.overlong = True
        else:
            self.pending.extend(part)

    def feed(self, chunk, stamp):
        pieces = chunk.split(b"\n")
        for piece in pieces[:-1]:
            self.append(piece)
            raw = None if self.overlong else bytes(self.pending)
            ui_action = None if raw is None else parse_ui_action(raw)
            if ui_action is not None:
                self.ui_action_records += 1
                self.observation.sink.emit(ui_action, stamp)
            record = None if raw is None else parse_line(raw)
            if record is not None:
                self.accepted += 1
                self.observation.accept(record, stamp)
                self.targets.observe_record(record, stamp, structural_input_context(raw, raw_length=self.raw_length))
            elif self.targets.feed(raw, stamp, self.raw_length):
                self.accepted += 1
            else:
                self.discarded += 1
            self.pending.clear()
            self.overlong = False
            self.raw_length = 0
        self.append(pieces[-1])

    def finish(self):
        context = None
        if self.pending or self.overlong:
            self.discarded += 1
            raw = None if self.overlong else bytes(self.pending)
            context = structural_input_context(raw, raw_length=self.raw_length)
            context["observed"] = "UNTERMINATED"
        self.pending.clear()
        self.overlong = False
        self.raw_length = 0
        self.targets.finish(context)


def create_log(directory, now=None):
    directory.mkdir(parents=True, exist_ok=True)
    now = dt.datetime.now(dt.timezone.utc) if now is None else now
    filename = "RID01_D3B2_RX1024_R3_4SRC_ABA_V3_SANITIZED_" + now.strftime("%Y%m%dT%H%M%S_%fZ") + ".log"
    path = directory / filename
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    return path, os.fdopen(descriptor, "w", encoding="ascii", newline="\n")


def stop_monitor(child):
    if child is None:
        return
    # The CLI cancels its monitor when stdin reaches EOF. Keep this pipe open
    # throughout observation without ever writing to it; close only on cleanup.
    if child.stdin is not None:
        child.stdin.close()
    # Target the owned process group even if the CLI has already exited: a
    # monitor subprocess could otherwise retain the serial port or pipe.
    try:
        os.killpg(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    if child.poll() is None:
        try:
            child.wait(timeout=3)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.wait(timeout=2)
    if child.stdout is not None:
        child.stdout.close()


def capture(cli, logfile, console=sys.stdout, *, launch=subprocess.Popen):
    """The launch override exists for synthetic tests; main uses the real CLI."""
    origin = time.monotonic()
    sink = Sink(logfile, console, origin)
    observation = Observation(sink)
    filtered = LineFilter(observation)
    child = None
    selector = selectors.DefaultSelector()
    try:
        child = launch([cli, "monitor", "--port", PORT, "--config", "baudrate=115200"],
                       stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL, bufsize=0, start_new_session=True)
        sink.emit("[HOST] CAPTURE_STARTED timestamp_basis=host_receipt"
                  " elapsed_origin=monitor_launch phase_acceptance=not_assessed"
                  " helper_version=" + HELPER_VERSION)
        sink.emit("[HOST] EXPECTED_FIRMWARE candidate=" + EXPECTED_CANDIDATE
                  + " expected_source_sha256=" + EXPECTED_SOURCE_SHA256
                  + " expected_application_sha256=" + EXPECTED_APPLICATION_SHA256
                  + " identity_basis=reviewed_host_expectations_not_device_attestation")
        sink.emit("[HOST] MANUAL_TIMING idle_seconds=120 active_seconds=240"
                  " post_idle_seconds=120 max_capture_seconds=1800 stop_with=Ctrl+C"
                  " guidance_only=1 follow_agreed_protocol=1 phase_acceptance=not_assessed")
        sink.emit("[HOST] TARGET_EVIDENCE schema=RX1024_R3_4SRC_v2 complete_blocks_only=1"
                  " slot_identity_not_verified=1 tracked_includes_stale_lost=1"
                  " receipt_completeness_not_assessed=1 rejection_context=bounded_structure_only")
        sink.emit("[HOST] UI_ACTION_EVIDENCE source=exact_firmware_console_messages"
                  " timestamp_basis=host_receipt render_completion=NOT_OBSERVED"
                  " original_v2_filter_still_applied=1")
        os.set_blocking(child.stdout.fileno(), False)
        selector.register(child.stdout, selectors.EVENT_READ)
        while time.monotonic() - origin < MAX_CAPTURE_SECONDS:
            for key, _ in selector.select(timeout=0.25):
                chunk = os.read(key.fd, READ_BYTES)
                if not chunk:
                    code = child.poll()
                    sink.emit("[HOST] ERROR monitor_ended exit_code="
                              + (str(code) if code is not None else "unavailable")
                              + " capture_incomplete=1")
                    return 3
                filtered.feed(chunk, sink.stamp())
            if child.poll() is not None and not selector.select(timeout=0):
                sink.emit("[HOST] ERROR monitor_ended exit_code=" + str(child.returncode)
                          + " capture_incomplete=1")
                return 3
            health_stamp = sink.stamp()
            observation.check_health(health_stamp)
            filtered.targets.check_health(health_stamp)
        sink.emit("[HOST] CAPTURE_STOPPED reason=time_limit phase_acceptance=not_assessed")
        return 4
    except KeyboardInterrupt:
        sink.emit("[HOST] CAPTURE_STOPPED reason=operator_interrupt phase_acceptance=not_assessed")
        return 0
    except OSError as error:
        sink.emit("[HOST] ERROR monitor_or_capture_io_failed errno=" + str(error.errno or 0)
                  + " capture_incomplete=1")
        return 2
    finally:
        old_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
        try:
            selector.close()
            stop_monitor(child)
        finally:
            signal.signal(signal.SIGINT, old_handler)
            filtered.finish()
            sink.emit(f"[HOST] CAPTURE_COUNTS accepted_lines={filtered.accepted}"
                      f" discarded_lines={filtered.discarded}"
                      f" counter_decrease_warnings={observation.counter_warnings}"
                      f" configuration_invalid={int(observation.config_invalid)}"
                      f" dashboard_complete={filtered.targets.completed['dashboard']}"
                      f" dashboard_incomplete={filtered.targets.incomplete['dashboard']}"
                      f" final_complete={filtered.targets.completed['final']}"
                      f" final_incomplete={filtered.targets.incomplete['final']}"
                      f" max_tracked_in_complete_dashboard={filtered.targets.max_tracked}"
                      f" ui_action_records={filtered.ui_action_records}")


def main():
    cli = shutil.which("arduino-cli")
    if cli is None:
        print("ERROR: arduino-cli was not found; capture was not started.")
        return 2
    try:
        path, logfile = create_log(Path.home() / "Downloads")
    except OSError:
        print("ERROR: a new exclusive log could not be created; capture was not started.")
        return 2
    print("Sanitized log:", path, flush=True)
    try:
        with logfile:
            result = capture(cli, logfile)
    except (OSError, subprocess.TimeoutExpired):
        print("ERROR: capture or cleanup failed; keep the partial sanitized log.")
        return 2
    print("Saved sanitized log:", path, flush=True)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
