#!/usr/bin/env python3
"""RID-01 R7 read-only RADAR target-color / navigation observation (v1).

Reuses the established bounded telemetry and dashboard privacy filter; retains
full SANITIZED records with quiet local prompts. No firmware upload, installs,
application serial commands or keyboard-command forwarding. arduino-cli monitor
stdin is OPEN UNWRITTEN to prevent EOF; existing USB control-line behavior remains.

One session: 75s idle; touchscreen clear-selection/BACK/USE GNSS; READY;
RADAR 75s; TARGETS 30s; SNAPSHOT 15s; LIVE 15s; RADAR 30s; controlled MODE
OPEN then PRIVATE; 10s PRIVATE settle; STOP; >=75s diagnostically covered idle.
The brief deliberate OPEN interval repeats the R6 MODE-control observation on
the RADAR screen; the color-only firmware does not change its geometry. Exact allowlists prevent saving raw identities, coordinates or NMEA,
including OPEN data. Do not photograph OPEN or use another serial terminal.

This is not automatic firmware acceptance, complete RF/position accuracy,
four-geographic-target hardware coverage, forced-loss or battery/SD/Wi-Fi testing.
Source/application hashes are host expectations, NOT device attestation.
Adapted from RID01_CAPTURE_RADAR_UI_R6_GUIDED_v1.py, reviewed with R7 source.
Only plottable slots show the new palette: T01 cyan, T02 blue, T03 violet,
T04 near-white. Two R111S units provide at most two geographic colors per run;
SYSTEM beacons do not become geographic targets merely to show more colors.
Record actual slot/color coverage; fixed slot color is not persistent aircraft ID.
The guide does not infer color or rendering correctness from serial telemetry.
Host validation is recorded separately; physical device validation is pending.
"""

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
HELPER_VERSION = "RADAR_COLOR_R7_GUIDED_v1"
EXPECTED_CANDIDATE = "RX1024_RADAR_COLOR_R7"
EXPECTED_RX_BYTES = 1024
EXPECTED_SOURCE_SHA256 = "7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2"
EXPECTED_APPLICATION_SHA256 = "1240f5c17591fac55443f3bb57321a51f5925ab99e0fa4c8b10e68cac368b4a6"
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
CONFIG_SUFFIX = (r" candidate=(?P<candidate>RX1024_RADAR_COLOR_R7) " +
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
    "[UI] BEGIN -> REFERENCE SELECT / backend PRIVATE / scanner IDLE.": "REFERENCE_SCREEN",
    "[GNSS-UI-R5] reference=GNSS_REQUESTED origin=TOUCH": "UI_REFERENCE_GNSS",
    "[GNSS-UI-R5] reference=NONE origin=TOUCH": "UI_REFERENCE_NONE",
    "[UI] REFERENCE SELECT -> OUTPUT MODE / scanner IDLE.": "OUTPUT_SCREEN",
    "[UI] BACK -> REFERENCE SELECT": "BACK_REFERENCE",
    "[UI-SHELL] BACK -> HOME": "BACK_HOME",
    "[UI] START SCAN -> TARGETS / backend ACTIVE.": "SCAN_START",
    "[UI] RESCAN complete / fresh RID capture ACTIVE.": "RESCAN_COMPLETE",
    "[UI] RESCAN ignored: no active RID session.": "RESCAN_IGNORED",
    "[UI ERROR] RESCAN failed.": "RESCAN_FAILED",
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


# Exact messages retained by wsProcessButton() in the returned R7 source.
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
    return parse_reference_radar_line(line)


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
                               + " expected_candidate=RX1024_RADAR_COLOR_R7 expected_rx_requested=1024"
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
                self.sink.emit("[HOST] NOTICE mode_open_observed bounded_control_check_requires_guide=1", stamp)
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
            self.sink.emit("[HOST] READY_FOR_TOUCHSCREEN_BASELINE guided_timer=1 reference_selection_by_touch_only=1"
                           " candidate=RX1024_RADAR_COLOR_R7 rx_requested=1024 rx_set=1024 uart_init=OK"
                           " gnss_progress_observed=1 idle_uart_reports=2"
                           " startup_and_setup_require_operator_check=1", stamp)
        if (self.stopped_at is not None and not self.post_idle_shown
                and all(self.latest[name][0] > self.stopped_at for name in ("GNSS", "DIAG", "UART"))):
            self.post_idle_shown = True
            self.sink.emit("[HOST] POST_STOP_IDLE_CONFIRMED begin_manual_75_second_idle=0 guided_timer_required=1"
                           " candidate=RX1024_RADAR_COLOR_R7 rx_requested=1024 rx_set=1024 uart_init=OK"
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



# Exact, coordinate-free messages retained in the returned R7 firmware. These are
# discrete observations, not continuously sampled reference/RADAR telemetry.
REFERENCE_MESSAGES = {
    "*** RECEIVER REFERENCE -> NONE ***": "CLEAR_ACK",
    "*** RECEIVER REFERENCE: NONE ***": "STATUS_NONE",
    "*** RECEIVER REFERENCE: GNSS / VALID ***": "STATUS_GNSS_VALID",
    "*** RECEIVER REFERENCE: FIXED / VALID ***": "STATUS_FIXED_VALID",
    "*** RECEIVER REFERENCE -> GNSS REQUESTED ***": "GNSS_REQUEST_ACK",
    "*** RECEIVER REFERENCE -> FIXED / VALID ***": "MANUAL_FIXED_ACK",
    "*** RECEIVER REFERENCE REJECTED ***": "COMMAND_REJECTED",
    "[GNSS-D3B2] GNSS receiver reference VALID (coordinates hidden).": "GNSS_VALID",
    "[GNSS-D3B2] GNSS reference withdrawn (fix invalid/stale).": "GNSS_WITHDRAWN",
}
RADAR_MESSAGES = {
    "[RADAR] READY: valid receiver reference + fresh geographic target.":
        ("READY", "PREREQUISITE_EVALUATION"),
    "[RADAR] ACQUIRING: geographic prerequisites unavailable.":
        ("ACQUIRING", "PREREQUISITE_EVALUATION"),
    "[UI] RADAR state = ACQUIRING.": ("ACQUIRING", "UI_SESSION_START_ANNOUNCEMENT"),
}


def parse_reference_radar_line(line):
    """Accept exact fixed strings only; never copy raw device text to output."""
    event = REFERENCE_MESSAGES.get(line)
    if event is not None:
        return "REFERENCE", {"event": event}, "[REFERENCE] event=" + event
    radar = RADAR_MESSAGES.get(line)
    if radar is not None:
        state, basis = radar
        return ("RADAR", {"state": state, "evidence": basis},
                "[RADAR-STATE] state=" + state + " evidence=" + basis)
    return None


class RadarObservation(Observation):
    """Record exact reference/state observations; there is no command writer."""
    def __init__(self, sink):
        super().__init__(sink)
        self.reference_records = 0
        self.radar_records = 0
        self.guide = None

    def accept(self, record, stamp):
        super().accept(record, stamp)
        if record[0] == "REFERENCE":
            self.reference_records += 1
        elif record[0] == "RADAR":
            self.radar_records += 1
        if self.guide is not None:
            self.guide.observe_record(record, stamp)


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
                if getattr(self.observation, "guide", None) is not None:
                    self.observation.guide.observe_ui_action(ui_action, stamp)
            if getattr(self.observation, "guide", None) is not None:
                mode_token = None if raw is None else parse_target_line(raw)
                if mode_token is not None and mode_token[0] == "MODE":
                    self.observation.guide.observe_target_mode(mode_token[1]["mode"], stamp)
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


class QuietSink(Sink):
    """Retain every sanitized record; show only deliberate host guidance."""
    def emit(self, message, stamp=None):
        utc, elapsed = self.stamp() if stamp is None else stamp
        timestamp = utc.isoformat(timespec="milliseconds").replace("+00:00", "Z")
        self.logfile.write(f"{timestamp} elapsed_s={elapsed:.3f} {message}\n")
        self.logfile.flush()
        if (message.startswith("[HOST] ERROR ")
                or message.startswith("[HOST] NOTICE missing_stream=")):
            self.console.write(message + "\n")
            self.console.flush()

    def guide(self, record, prompt, stamp):
        self.emit(record, stamp)
        self.console.write("\n" + prompt + "\n")
        self.console.flush()


class GuidedProtocol:
    """One read-only, receipt-driven layout/navigation observation.

    No application bytes are sent. MODE is intentionally toggled at the end,
    only while in RADAR, then restored to PRIVATE before STOP. Exact received
    messages do not attest rendered pixels or source/binary identity.
    """
    PROMPTS = {
        "WAIT_READY": "Leave the scanner stopped at HOME; acknowledge startup if shown. USB only, LiPo disconnected.\nWaiting for fresh advancing R7 diagnostics. Do not type into this terminal.",
        "INITIAL_IDLE": "PHASE A: leave HOME idle for 75 seconds. Keep both R111S units and beacons #1/#3 powered and stationary.",
        "WAIT_BEGIN": "Tap BEGIN. We will clear any retained reference selection without starting a scan.",
        "WAIT_NONE": "Tap TARGETS ONLY. Do NOT start scanning on the next screen.",
        "WAIT_BACK_REFERENCE": "On OUTPUT MODE tap BACK, not START. This returns to SELECT REFERENCE.",
        "WAIT_GNSS": "Tap USE GNSS on SELECT REFERENCE.",
        "WAIT_START": "Keep PRIVATE selected and tap START SCAN. Do not use another serial monitor.",
        "WAIT_READY_RADAR": "Waiting for GNSS reference VALID and RADAR READY. Identify both R111S slots in TARGETS. Keep MODE and RESCAN untouched.",
        "WAIT_RADAR": "GNSS VALID and RADAR READY received. Tap RADAR now.",
        "FIRST_RADAR": "PHASE B: remain on RADAR for 75 seconds. Photo cues at 8 and 70 seconds. Check matching corner-tag/dot/leader colors for the two real RID slots: T01 cyan, T02 blue, T03 violet, T04 near-white. Only plotted slots show colors. REF:GNSS, PLOTS:2 and right-panel status colors must remain unchanged. Record contrast, clipping or flicker. Labels are NOT buttons.",
        "WAIT_TARGETS": "Tap TARGETS now.",
        "TARGETS": "PHASE C: spend 30 seconds inspecting all four target tabs. Photograph each R111S row. Finish with one R111S selected (geography present); do not select a SYSTEM beacon for the snapshot.",
        "WAIT_SNAPSHOT": "With an R111S selected, tap SNAPSHOT now.",
        "SNAPSHOT": "PHASE D: leave the snapshot frozen for 15 seconds. Photo cue at 5 seconds; confirm its packet count stays fixed. This is not a receiver pause.",
        "WAIT_BACK_LIVE": "Tap BACK TO LIVE now.",
        "LIVE_RETURN": "Remain on LIVE/TARGETS for 15 seconds. Watch the selected R111S packet count advance. Do not tap MODE or RESCAN.",
        "WAIT_RETURN_RADAR": "Tap RADAR again now.",
        "RETURN_RADAR": "PHASE E: remain on RADAR for 30 seconds. Photo cue at 8 seconds. Check that the same slot colors persist, labels/rows still agree, right-panel/status colors remain unchanged, and controls remain distinct. Note whether colored leaders help or whether neutral leaders would be clearer.",
        "WAIT_MODE_OPEN": "CONTROL CHECK: while on RADAR, tap the center of MODE ONCE. It should change PRIVATE to OPEN without stopping or leaving RADAR. Do not photograph OPEN or run another serial terminal. The helper retains only sanitized data.",
        "MODE_OPEN_HOLD": "OPEN change received. Remain on RADAR for 2 seconds; then the next prompt will ask you to restore PRIVATE. No photo during OPEN.",
        "WAIT_MODE_PRIVATE": "Tap MODE ONCE again to restore PRIVATE. The board should remain on RADAR. If STOP fires instead or MODE does not respond, report that; do not repeatedly tap.",
        "PRIVATE_SETTLE": "PRIVATE change received. Remain on RADAR for 10 seconds and visually confirm the PRIVATE header. This brief MODE check is separate from the normal PRIVATE observation.",
        "WAIT_STOP": "Tap the center of STOP on the RADAR screen now. It should return HOME, not toggle MODE. Leave the capture running.",
        "WAIT_STOPPED": "Waiting for the scan-stop confirmation. Do not unplug or reset.",
        "WAIT_POST_IDLE": "Waiting for fresh post-STOP idle diagnostics. Leave HOME untouched.",
        "POST_IDLE": "PHASE F: remain at HOME for at least 75 seconds. The helper will save automatically once diagnostics cover the endpoint. Do not quit early.",
    }
    DURATIONS = {"INITIAL_IDLE":75, "FIRST_RADAR":75, "TARGETS":30,
                 "SNAPSHOT":15, "LIVE_RETURN":15, "RETURN_RADAR":30,
                 "MODE_OPEN_HOLD":2, "PRIVATE_SETTLE":10, "POST_IDLE":75}
    TIMED_NEXT = {"INITIAL_IDLE":"WAIT_BEGIN", "FIRST_RADAR":"WAIT_TARGETS",
                  "TARGETS":"WAIT_SNAPSHOT", "SNAPSHOT":"WAIT_BACK_LIVE",
                  "LIVE_RETURN":"WAIT_RETURN_RADAR", "RETURN_RADAR":"WAIT_MODE_OPEN",
                  "MODE_OPEN_HOLD":"WAIT_MODE_PRIVATE"}
    IDLE_STAGES = {"INITIAL_IDLE", "WAIT_BEGIN", "WAIT_NONE", "WAIT_BACK_REFERENCE",
                   "WAIT_GNSS", "WAIT_START", "POST_IDLE"}
    ACTIVE_STAGES = {"WAIT_READY_RADAR", "WAIT_RADAR", "FIRST_RADAR", "WAIT_TARGETS",
                     "TARGETS", "WAIT_SNAPSHOT", "SNAPSHOT", "WAIT_BACK_LIVE",
                     "LIVE_RETURN", "WAIT_RETURN_RADAR", "RETURN_RADAR",
                     "WAIT_MODE_OPEN", "MODE_OPEN_HOLD", "WAIT_MODE_PRIVATE",
                     "PRIVATE_SETTLE", "WAIT_STOP"}
    TARGET_SELECT_STAGES = {"WAIT_READY_RADAR", "WAIT_RADAR", "TARGETS", "WAIT_SNAPSHOT", "LIVE_RETURN"}
    PHOTO_CUES = {"FIRST_RADAR":(8,70), "SNAPSHOT":(5,), "RETURN_RADAR":(8,)}
    WAIT_LIMITS = {"WAIT_READY":300, "WAIT_READY_RADAR":120,
                   "WAIT_STOPPED":45, "WAIT_POST_IDLE":45,
                   "WAIT_MODE_OPEN":30, "WAIT_MODE_PRIVATE":20}

    def __init__(self, observation, sink):
        self.observation, self.sink = observation, sink
        self.stage, self.stage_since = "WAIT_READY", sink.stamp()[1]
        self.deadline = None
        self.complete = self.needs_review = self.initialized = self.ended = False
        self.fault_reason = None
        self.scan_started_at = None
        self.gnss_touch = self.gnss_valid = self.radar_ready = False
        self.error_previous, self.error_alerted = {}, set()
        self.photos, self.next_countdown = set(), 10
        self.mode_open_at = self.mode_private_at = None
        self.private_summary_after_restore = False
        self.mode_open_events = self.mode_private_events = 0
        self.last_selected = self.snapshot_slot = None
        self._announce(sink.stamp())

    def emit(self, body, prompt, stamp):
        self.sink.guide("[GUIDE] " + body, prompt, stamp)

    def _announce(self, stamp):
        suffix = "" if self.deadline is None else f" minimum_duration_s={self.deadline-self.stage_since:.0f}"
        self.emit("phase="+self.stage+" event=BEGIN"+suffix+
                  " timestamp_basis=host_receipt render_completion=NOT_OBSERVED",
                  self.PROMPTS.get(self.stage, self.stage), stamp)

    def _stage(self, stage, stamp):
        self.stage, self.stage_since = stage, stamp[1]
        seconds = self.DURATIONS.get(stage)
        self.deadline = None if seconds is None else stamp[1]+seconds
        self.photos, self.next_countdown = set(), 10
        self._announce(stamp)

    def fail(self, reason, stamp):
        if self.fault_reason or self.complete:
            return
        previous = self.stage
        self.fault_reason, self.needs_review = reason, True
        self.stage, self.deadline = "NEEDS_REVIEW", None
        self.emit("phase=NEEDS_REVIEW reason="+reason+" previous_phase="+previous+
                  " capture_continues=1 firmware_acceptance=NOT_ASSESSED",
                  "GUIDED STEPS PAUSED: "+reason.replace("_"," ")+
                  ".\nIf scanning, use STOP/HOME; then Ctrl+C to save this partial run. If OPEN was left active, do not photograph identifiers/coordinates. Do not reflash or change wiring.", stamp)

    def _advance_timers(self, stamp):
        if self.stage in self.TIMED_NEXT and self.deadline is not None and stamp[1]>=self.deadline:
            self._stage(self.TIMED_NEXT[self.stage], stamp)
        if (self.stage=="PRIVATE_SETTLE" and self.deadline is not None and
                stamp[1]>=self.deadline and self.private_summary_after_restore):
            self._stage("WAIT_STOP",stamp)

    def _notice_errors(self, kind, values, stamp):
        if kind not in ("GNSS","UART"):
            return
        fields = ("checksum_bad","malformed","long") if kind=="GNSS" else UART_FIELDS
        current = {n:values[n] for n in fields}
        previous = self.error_previous.get(kind)
        self.error_previous[kind] = current
        changed = [] if previous is None else [n for n in fields if current[n]>previous[n]]
        if changed:
            self.needs_review=True
            body="[GUIDE] event=NEW_ERROR_COUNTERS stream="+kind+" fields="+",".join(changed)+" protocol_may_continue=1 firmware_acceptance=NOT_ASSESSED"
            if kind not in self.error_alerted:
                self.error_alerted.add(kind)
                self.sink.guide(body,"New "+kind+" error counts observed. Complete this one sequence if the UI remains normal; do not restart to obtain a cleaner result.",stamp)
            else:
                self.sink.emit(body,stamp)

    def observe_record(self, record, stamp):
        kind, values, _ = record
        self._notice_errors(kind,values,stamp)
        if self.fault_reason or self.complete:
            return
        self._advance_timers(stamp)
        if kind=="REFERENCE":
            event=values["event"]
            if event in {"GNSS_REQUEST_ACK","CLEAR_ACK","STATUS_NONE","STATUS_GNSS_VALID",
                         "STATUS_FIXED_VALID","MANUAL_FIXED_ACK","COMMAND_REJECTED"}:
                self.fail("UNEXPECTED_SERIAL_REFERENCE_COMMAND_EVIDENCE",stamp)
            elif event=="GNSS_VALID":
                if self.stage in {"WAIT_BACK_REFERENCE","WAIT_GNSS"}:
                    self.fail("GNSS_VALID_WHILE_REFERENCE_CLEARED",stamp)
                elif self.gnss_touch:
                    self.gnss_valid=True
            elif event=="GNSS_WITHDRAWN" and self.gnss_touch:
                self.gnss_valid=False
                self.fail("GNSS_REFERENCE_WITHDRAWN_IN_WARM_TEST",stamp)
            return
        if kind=="RADAR" and values["evidence"]=="PREREQUISITE_EVALUATION":
            self.radar_ready=values["state"]=="READY"
            if not self.radar_ready and self.stage in self.ACTIVE_STAGES-{"WAIT_READY_RADAR","WAIT_RADAR"}:
                self.fail("RADAR_PREREQUISITES_LOST",stamp)
            return
        if kind!="EVENT":
            return
        event=values["event"]
        if event.endswith("FAILED") or event in {"BLE_NOT_INITIALIZED","ENDED_BY_CONTROLLER","START_BLOCKED_ACK"}:
            self.fail(event,stamp);return
        if event in {"RESCAN_COMPLETE","RESCAN_IGNORED","START_IGNORED_ACTIVE","BEGIN_PRIVATE_IDLE"}:
            self.fail("UNEXPECTED_"+event,stamp);return
        if event=="GNSS_UART_INIT" and self.initialized:
            self.fail("REINITIALIZATION_AFTER_BASELINE",stamp);return
        if event=="REFERENCE_SCREEN":
            if self.stage=="WAIT_BEGIN": self._stage("WAIT_NONE",stamp)
            else: self.fail("BEGIN_OUT_OF_SEQUENCE",stamp)
        elif event=="UI_REFERENCE_NONE":
            if self.stage=="WAIT_NONE":
                self.gnss_valid=self.radar_ready=False
                self._stage("WAIT_BACK_REFERENCE",stamp)
            else: self.fail("TARGETS_ONLY_OUT_OF_SEQUENCE",stamp)
        elif event=="BACK_REFERENCE":
            if self.stage=="WAIT_BACK_REFERENCE": self._stage("WAIT_GNSS",stamp)
            else: self.fail("BACK_REFERENCE_OUT_OF_SEQUENCE",stamp)
        elif event=="UI_REFERENCE_GNSS":
            if self.stage=="WAIT_GNSS":
                self.gnss_touch=True; self.gnss_valid=self.radar_ready=False
                self._stage("WAIT_START",stamp)
            else: self.fail("GNSS_SELECTION_OUT_OF_SEQUENCE",stamp)
        elif event=="SCAN_START":
            if self.stage=="WAIT_START":
                self.scan_started_at=stamp[1]; self.radar_ready=False
                self._stage("WAIT_READY_RADAR",stamp)
            else: self.fail("SCAN_START_OUT_OF_SEQUENCE",stamp)
        elif event=="MODE_OPEN":
            if self.stage=="WAIT_MODE_OPEN":
                self.mode_open_events+=1; self.mode_open_at=stamp[1]
                self._stage("MODE_OPEN_HOLD",stamp)
            else: self.fail("UNEXPECTED_OPEN_MODE",stamp)
        elif event=="MODE_PRIVATE":
            if self.stage=="WAIT_MODE_PRIVATE":
                self.mode_private_events+=1;self.mode_private_at=stamp[1]
                self.private_summary_after_restore=False
                self.sink.emit(f"[GUIDE] event=MODE_PAIR_OBSERVED open_duration_host_s={stamp[1]-self.mode_open_at:.3f} view_continuity_requires_photo_operator=1",stamp)
                self._stage("PRIVATE_SETTLE",stamp)
            elif self.stage not in {"WAIT_START","WAIT_BACK_REFERENCE"}:
                self.fail("MODE_PRIVATE_OUT_OF_SEQUENCE",stamp)
        elif event=="SCAN_STOP_REQUEST":
            if self.stage=="WAIT_STOP": self._stage("WAIT_STOPPED",stamp)
            else: self.fail("STOP_TOO_EARLY_OR_WRONG_CONTROL",stamp)
        elif event=="SCAN_STOPPED":
            if self.stage=="WAIT_STOPPED": self._stage("WAIT_POST_IDLE",stamp)
            else: self.fail("SCAN_STOPPED_OUT_OF_SEQUENCE",stamp)
        elif event in {"BACK_HOME","STOP_ALREADY_IDLE"}:
            self.fail(event+"_OUT_OF_SEQUENCE",stamp)

    def observe_ui_action(self, action, stamp):
        if self.fault_reason or self.complete:
            return
        self._advance_timers(stamp)
        fields=dict(re.findall(r"([a-z_]+)=([A-Z0-9_]+)",action))
        name, outcome = fields.get("action"), fields.get("outcome")
        if name=="SELECT_TARGET":
            if self.stage not in self.TARGET_SELECT_STAGES: self.fail("TARGET_SELECTION_OUT_OF_SEQUENCE",stamp)
            else: self.last_selected=fields.get("slot")
        elif name=="VIEW_RADAR" and outcome=="UNAVAILABLE":
            if self.stage not in {"WAIT_READY_RADAR","WAIT_RADAR"}:self.fail("UNEXPECTED_RADAR_UNAVAILABLE",stamp)
        elif name=="VIEW_RADAR" and outcome=="REQUEST_ACCEPTED":
            if self.stage=="WAIT_READY_RADAR" and self.gnss_valid and self.radar_ready:self._stage("WAIT_RADAR",stamp)
            if self.stage=="WAIT_RADAR":self._stage("FIRST_RADAR",stamp)
            elif self.stage=="WAIT_RETURN_RADAR":self._stage("RETURN_RADAR",stamp)
            else:self.fail("RADAR_ENTRY_OUT_OF_SEQUENCE",stamp)
        elif name=="VIEW_TARGETS" and outcome=="REQUEST_ACCEPTED":
            if self.stage=="WAIT_TARGETS":self._stage("TARGETS",stamp)
            elif self.stage not in {"WAIT_READY_RADAR","WAIT_RADAR","TARGETS","WAIT_SNAPSHOT","LIVE_RETURN"}:
                self.fail("TARGETS_ENTRY_TOO_EARLY_OR_WRONG_CONTROL",stamp)
        elif name=="SNAPSHOT":
            if (self.stage=="WAIT_SNAPSHOT" and outcome=="REQUEST_ACCEPTED" and
                    fields.get("snapshot")=="FROZEN" and fields.get("mode")=="PRIVATE"):
                self.snapshot_slot=fields.get("slot")
                self.sink.emit("[GUIDE] event=SNAPSHOT_CAPTURED slot="+str(self.snapshot_slot)+" transmitter_identity_requires_operator_photo=1",stamp)
                self._stage("SNAPSHOT",stamp)
            else:self.fail("SNAPSHOT_OUT_OF_SEQUENCE_OR_EMPTY",stamp)
        elif name=="BACK_TO_LIVE":
            if self.stage=="WAIT_BACK_LIVE":self._stage("LIVE_RETURN",stamp)
            else:self.fail("BACK_TO_LIVE_TOO_EARLY",stamp)

    def observe_target_mode(self, mode, stamp):
        if self.fault_reason or self.complete:
            return
        if mode=="OPEN":
            planned = self.stage in {"WAIT_MODE_OPEN","MODE_OPEN_HOLD","WAIT_MODE_PRIVATE"}
            # A serial block may have begun before the PRIVATE acknowledgement.
            tail = self.stage=="PRIVATE_SETTLE" and self.mode_private_at is not None and stamp[1]-self.mode_private_at<=2.0
            if not (planned or tail):self.fail("OPEN_SUMMARY_OUTSIDE_CONTROL_CHECK",stamp)
        elif mode=="PRIVATE" and self.stage=="PRIVATE_SETTLE":
            self.private_summary_after_restore=True

    def tick(self, stamp):
        if self.fault_reason or self.complete:
            return
        obs, now = self.observation, stamp[1]
        if obs.config_invalid:self.fail("INVALID_R7_CONFIGURATION",stamp);return
        if obs.counter_warnings:self.fail("COUNTER_DECREASE",stamp);return
        if self.stage=="WAIT_READY":
            if obs.ready_shown:
                self.initialized=True;self._stage("INITIAL_IDLE",stamp)
            elif now-self.stage_since>300:self.fail("READINESS_TIMEOUT",stamp)
            return
        for name in ("GNSS","DIAG","UART"):
            if name not in obs.latest or now-obs.latest[name][0]>HEALTH_SECONDS:
                self.fail("MISSING_OR_STALE_"+name,stamp);return
        if obs.latest["GNSS"][1]["FIX"]!="FRESH":self.fail("GNSS_NOT_FRESH_IN_WARM_TEST",stamp);return
        samples=obs.samples["GNSS"]
        if len(samples)<2 or not (samples[-1][1]["bytes"]>samples[0][1]["bytes"] and samples[-1][1]["lines"]>samples[0][1]["lines"]):
            self.fail("GNSS_COUNTERS_NOT_ADVANCING",stamp);return
        if self.stage in self.IDLE_STAGES and any(obs.latest[n][1]["scan"]!="IDLE" for n in ("DIAG","UART")):
            self.fail("ACTIVE_DIAGNOSTICS_DURING_IDLE",stamp);return
        if self.stage in self.ACTIVE_STAGES and self.scan_started_at is not None and now-self.scan_started_at>RECENT_SECONDS:
            if any(obs.latest[n][1]["scan"]!="ACTIVE" for n in ("DIAG","UART")):
                self.fail("IDLE_DIAGNOSTICS_DURING_SCAN",stamp);return
        self._advance_timers(stamp)
        if self.stage=="WAIT_READY_RADAR" and self.gnss_valid and self.radar_ready:self._stage("WAIT_RADAR",stamp)
        if self.stage=="WAIT_POST_IDLE" and obs.post_idle_shown:self._stage("POST_IDLE",stamp)
        for second in self.PHOTO_CUES.get(self.stage,()):
            if now-self.stage_since>=second and second not in self.photos:
                self.photos.add(second)
                self.emit(f"phase={self.stage} event=PHOTO_CUE offset_s={second}",
                          f"PHOTO NOW: {self.stage}, approximately {second} seconds. Include the whole display; keep PRIVATE.",stamp)
        if self.stage=="SNAPSHOT" and now-self.stage_since>=12 and 12 not in self.photos:
            self.photos.add(12)
            self.emit("phase=SNAPSHOT event=FROZEN_COPY_CHECK offset_s=12","Confirm the frozen packet count has not changed. No extra photo required.",stamp)
        if self.deadline is not None:
            elapsed=now-self.stage_since
            if elapsed>=self.next_countdown and now<self.deadline:
                self.next_countdown=(int(elapsed)//10+1)*10
                self.emit(f"phase={self.stage} event=TIMER remaining_s={max(0,int(self.deadline-now+0.999))}",
                          f"{self.stage}: {max(0,int(self.deadline-now+0.999))} seconds remaining.",stamp)
        if self.stage=="POST_IDLE" and now>=self.deadline:
            if all(obs.latest[n][0]>=self.deadline for n in ("GNSS","DIAG","UART")):
                self.complete=True;self.stage="COMPLETE"
                self.emit("phase=COMPLETE event=PROTOCOL_COMPLETE device_commands_sent=0 firmware_acceptance=NOT_ASSESSED",
                          "GUIDED OBSERVATION COMPLETE. Saving log. Readability, touch behavior and error deltas still need review.",stamp)
                return
        if self.stage=="PRIVATE_SETTLE" and now-self.stage_since>25:
            self.fail("PRIVATE_SUMMARY_NOT_CONFIRMED",stamp)
        elif self.stage in self.WAIT_LIMITS and now-self.stage_since>self.WAIT_LIMITS[self.stage]:
            self.fail("TIMEOUT_"+self.stage,stamp)
        elif self.deadline is None and self.stage not in {"NEEDS_REVIEW","COMPLETE"} and now-self.stage_since>300:
            self.fail("OPERATOR_ACTION_TIMEOUT",stamp)

    def finish(self, reason, stamp):
        if self.ended:return
        self.ended=True
        self.emit(f"event=CAPTURE_END reason={reason} final_phase={self.stage} protocol_complete={int(self.complete)} needs_review={int(self.needs_review)} device_commands_sent=0 mode_open_events={self.mode_open_events} mode_private_restore_events={self.mode_private_events} firmware_acceptance=NOT_ASSESSED",
                  "Saved observation: return this sanitized log, photos and control/readability notes." if self.complete else
                  "Partial observation saved. Send it for review before repeating or changing firmware.",stamp)


def create_log(directory, now=None):
    directory.mkdir(parents=True, exist_ok=True)
    now = dt.datetime.now(dt.timezone.utc) if now is None else now
    filename = "RID01_RADAR_COLOR_R7_GUIDED_V1_SANITIZED_" + now.strftime("%Y%m%dT%H%M%S_%fZ") + ".log"
    path = directory / filename
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    return path, os.fdopen(descriptor, "w", encoding="ascii", newline="\n")


def stop_monitor(child):
    if child is None:
        return
    # The CLI cancels its monitor when stdin reaches EOF. Keep this pipe open
    # throughout observation; no application bytes are written. Close on cleanup.
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
    """Run one monitor, with no stdin reader. Launch override is host-test-only."""
    origin = time.monotonic()
    sink = QuietSink(logfile, console, origin)
    observation = RadarObservation(sink)
    filtered = LineFilter(observation)
    child = None
    guide = None
    end_reason = "capture_error"
    selector = selectors.DefaultSelector()
    try:
        child = launch([cli, "monitor", "--port", PORT, "--config", "baudrate=115200"],
                       stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                       stderr=subprocess.DEVNULL, bufsize=0, start_new_session=True)
        # Keep monitor stdin open without EVER writing. Do not forward keyboard input.
        sink.emit("[HOST] CAPTURE_STARTED timestamp_basis=host_receipt"
                  " elapsed_origin=monitor_launch phase_acceptance=not_assessed"
                  " helper_version=" + HELPER_VERSION)
        sink.emit("[HOST] EXPECTED_FIRMWARE candidate=" + EXPECTED_CANDIDATE
                  + " expected_source_sha256=" + EXPECTED_SOURCE_SHA256
                  + " expected_application_sha256=" + EXPECTED_APPLICATION_SHA256
                  + " identity_basis=reviewed_host_expectations_not_device_attestation")
        sink.emit("[HOST] GUIDED_TIMING initial_idle_s=75 radar_s=75 targets_s=30 snapshot_s=15"
                  " live_return_s=15 radar_return_s=30 mode_open_hold_s=2 private_settle_s=10 post_idle_minimum_s=75"
                  " final_reports_after_endpoint_required=1 max_capture_seconds=1800 phase_acceptance=not_assessed")
        sink.emit("[HOST] COMMAND_SCOPE application_device_commands=0 terminal_input_consumed=0"
                  " reference_selection=TOUCH_ONLY stdin_pipe=OPEN_UNWRITTEN")
        sink.emit("[HOST] MODE_CHECK controlled_open_private_pair=1 raw_open_output_never_saved=1 photograph_private_only=1")
        sink.emit("[HOST] RADAR_COLOR_SCOPE T01=CYAN T02=BLUE T03=VIOLET T04=NEAR_WHITE"
                  " changed=CORNER_TAG_DOT_LEADER right_panel_unchanged=1"
                  " color_and_slot_coverage_require_photos=1 color_not_persistent_aircraft_identity=1")
        sink.emit("[HOST] REFERENCE_RADAR_EVIDENCE exact_coordinate_free_messages_only=1"
                  " transition_only=1 gnss_fresh_does_not_prove_reference_valid=1"
                  " radar_ready_does_not_prove_all_slots_have_fresh_geo=1")
        sink.emit("[HOST] TARGET_EVIDENCE schema=RX1024_R3_4SRC_v2 complete_blocks_only=1"
                  " slot_identity_not_verified=1 tracked_includes_stale_lost=1"
                  " receipt_completeness_not_assessed=1 rejection_context=bounded_structure_only")
        sink.emit("[HOST] UI_ACTION_EVIDENCE source=exact_firmware_console_messages"
                  " timestamp_basis=host_receipt render_completion=NOT_OBSERVED"
                  " original_v2_target_parser_still_applied=1")
        guide = GuidedProtocol(observation, sink)
        observation.guide = guide
        os.set_blocking(child.stdout.fileno(), False)
        selector.register(child.stdout, selectors.EVENT_READ)
        while time.monotonic() - origin < MAX_CAPTURE_SECONDS:
            for key, _ in selector.select(timeout=0.25):
                stamp = sink.stamp()
                try:
                    chunk = os.read(key.fd, READ_BYTES)
                except BlockingIOError:
                    continue
                if not chunk:
                    code = child.poll()
                    end_reason = "monitor_ended"
                    sink.emit("[HOST] ERROR monitor_ended exit_code="
                              + (str(code) if code is not None else "unavailable")
                              + " capture_incomplete=1", stamp)
                    return 3
                filtered.feed(chunk, stamp)
            if child.poll() is not None and not selector.select(timeout=0):
                end_reason = "monitor_ended"
                sink.emit("[HOST] ERROR monitor_ended exit_code=" + str(child.returncode)
                          + " capture_incomplete=1")
                return 3
            health_stamp = sink.stamp()
            observation.check_health(health_stamp)
            filtered.targets.check_health(health_stamp)
            # All received messages in this batch are processed before the
            # read-only guide advances. No application writes are possible.
            guide.tick(health_stamp)
            if guide.complete:
                end_reason = "guided_protocol_complete"
                sink.emit("[HOST] CAPTURE_STOPPED reason=guided_protocol_complete"
                          " phase_acceptance=not_assessed", health_stamp)
                return 0
        end_reason = "time_limit"
        sink.emit("[HOST] CAPTURE_STOPPED reason=time_limit phase_acceptance=not_assessed")
        return 4
    except KeyboardInterrupt:
        end_reason = "operator_interrupt"
        sink.emit("[HOST] CAPTURE_STOPPED reason=operator_interrupt phase_acceptance=not_assessed")
        return 130
    except OSError as error:
        end_reason = "monitor_or_capture_io_failed"
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
            if guide is not None:
                guide.finish(end_reason, sink.stamp())
            sink.emit(f"[HOST] CAPTURE_COUNTS accepted_lines={filtered.accepted}"
                      f" discarded_lines={filtered.discarded}"
                      f" counter_decrease_warnings={observation.counter_warnings}"
                      f" configuration_invalid={int(observation.config_invalid)}"
                      f" dashboard_complete={filtered.targets.completed['dashboard']}"
                      f" dashboard_incomplete={filtered.targets.incomplete['dashboard']}"
                      f" final_complete={filtered.targets.completed['final']}"
                      f" final_incomplete={filtered.targets.incomplete['final']}"
                      f" max_tracked_in_complete_dashboard={filtered.targets.max_tracked}"
                      f" ui_action_records={filtered.ui_action_records}"
                      f" reference_records={observation.reference_records}"
                      f" radar_records={observation.radar_records}"
                      " application_device_commands_sent=0"
                      f" guided_protocol_complete={int(guide.complete) if guide else 0}"
                      f" guided_needs_review={int(guide.needs_review) if guide else 0}")


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
