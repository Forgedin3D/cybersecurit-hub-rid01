#!/usr/bin/env python3
"""Summarize a privacy-filtered GNSS D3B1 counter A/B/A observation.

Reads only the sanitized output from RID01_D3B1_CHECKSUM_ABA_CAPTURE.sh.
No reads of raw NMEA or BLE identifiers, no network or firmware writes.
"""
from __future__ import annotations
from pathlib import Path
import re
import sys

LOG = Path.home() / 'Downloads/RID01_D3B1_CHECKSUM_ABA_SANITIZED.log'
RE = re.compile(
    r'^\[GNSS-D3B1\] bytes=(?P<bytes>\d+) lines=(?P<lines>\d+) '
    r'nmea_ok=(?P<nmea_ok>\d+) checksum_bad=(?P<checksum_bad>\d+) '
    r'malformed=(?P<malformed>\d+) long=(?P<long>\d+) '
    r'GGA=(?P<GGA>\d+) RMC=(?P<RMC>\d+) '
    r'FIX=(?P<FIX>FRESH|STALE|NO_FIX) SATS=(?P<SATS>\d+)$'
)


def main() -> int:
    if not LOG.is_file():
        print(f'STOP: Missing sanitized log: {LOG}')
        return 1
    if LOG.stat().st_size > 2_000_000:
        print('STOP: Unexpectedly large log; inspect before parsing.')
        return 1
    segments = {'A_IDLE_BEFORE': [], 'B_SCAN_ACTIVE': [], 'C_IDLE_AFTER': []}
    mode = 'A_IDLE_BEFORE'
    started = stopped = rollover = 0
    for lineno, raw in enumerate(LOG.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
        line = raw.strip()
        if line == '[EVENT] SCAN_START':
            started += 1
            mode = 'B_SCAN_ACTIVE'
        elif line == '[EVENT] SCAN_STOP':
            stopped += 1
            mode = 'C_IDLE_AFTER'
        elif line == '[EVENT] SCAN_ROLLOVER':
            rollover += 1
        else:
            match = RE.fullmatch(line)
            if match:
                row = {k: (v if k == 'FIX' else int(v)) for k, v in match.groupdict().items()}
                segments[mode].append(row)
            elif line:
                print(f'WARNING: Unrecognized line {lineno}; please inspect sanitized log locally.')
    print('=== RID-01 D3B1 CHECKSUM A/B/A SUMMARY ===')
    print(f'SCAN_START events: {started} | SCAN_STOP events: {stopped} | ROLLOVERS: {rollover}')
    sufficient = started == 1 and stopped == 1
    for label, rows in segments.items():
        print(f'\n{label}: {len(rows)} GNSS reports')
        if len(rows) < 2:
            print('  INSUFFICIENT REPORTS; repeat or extend this phase')
            sufficient = False
            continue
        first, last = rows[0], rows[-1]
        lines = last['lines'] - first['lines']
        bad = last['checksum_bad'] - first['checksum_bad']
        good = last['nmea_ok'] - first['nmea_ok']
        print(f'  First → last line count: {first["lines"]} → {last["lines"]} (Δ {lines})')
        print(f'  First → last valid NMEA: {first["nmea_ok"]} → {last["nmea_ok"]} (Δ {good})')
        print(f'  First → last checksum_bad: {first["checksum_bad"]} → {last["checksum_bad"]} (Δ {bad})')
        print(f'  First → last FIX: {first["FIX"]} → {last["FIX"]}')
        print(f'  Malformed Δ: {last["malformed"]-first["malformed"]}; long Δ: {last["long"]-first["long"]}')
        if lines <= 0 or bad < 0 or good < 0:
            print('  STOP: Counters did not progress monotonically; inspect/reset suspected.')
            sufficient = False
        else:
            print(f'  Checksum failures / completed lines (within sampled interval): {bad}/{lines} ({100*bad/lines:.3f}%)')
        if len(rows) < 9:
            print('  NOTE: fewer than 9 reports (~40 sec sampled span); extend for stronger comparison.')
    print('\nINTERPRETATION: A/B/A compares correlation with scanner state, not root cause.')
    print('Possible competing explanations include UART buffer overflow, serial scheduling, line parser sync, and electrical noise.')
    print('No unfiltered serial log, coordinates, or target identifiers were inspected.')
    return 0 if sufficient else 2


if __name__ == '__main__':
    sys.exit(main())
