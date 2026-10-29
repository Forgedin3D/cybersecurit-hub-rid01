# RID-01 validation register

**As of 2026-10-29.** Results are derived from operator-provided serial logs, photographs, acceptance commands, and earlier handoff records. Do not extrapolate beyond tested scope.

## Waveshare D4H-B startup acknowledgment — ACCEPTED

Board: Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0. Accepted source hash: `7bdb2be698c63aba895e5b892f29478f8e215807126eb155bac9abd8fcabbfcf`.

| ID | Hardware test performed by operator | Outcome |
| --- | --- | --- |
| A | Remove all power, restore power: notice displayed | PASS |
| B | RESET *before* acknowledgment: notice remains required | PASS |
| C | RESET *after* acknowledgment: HOME, without repeated notice | PASS |
| D | BEGIN -> START -> STOP/HOME, without notice reappearing | PASS |
| E | Second full cold power-on: notice appears again | PASS |

Additional D4H-A screen evidence: legible warning and button; touch serial log included DOWN/UP ACKNOWLEDGE, START, T01–T04, STOP. One short scan reported 676 BLE advertisement callbacks, zero validated RID packets, zero target allocations and a clean STOP after 7,199 ms. **This short no-target scan is not a live-aircraft decode test.**

D4H-B clean compilation: **PASS**, Arduino ESP32 core 3.3.11; sketch 689,543 bytes (21% of selected partition maximum), globals 31,588 bytes (9% of RAM report). D4G source protected and frozen.

## Waveshare prior functionality — previously accepted

- D4E radar lifecycle field validation with real **Ruko R111S** and Beacon #1.
- D4F colored ACTIVE (green) / STALE (yellow) right-panel labels, LOST hidden.
- D4G STOP/HOME hardening including intentional failed-STOP injection, restoration of tested production firmware, and successful regression.
- 2026-10-27 UI/operator regression: mode switch PRIVATE/OPEN and redacted SNAPSHOT; four target buttons; RESCAN; scan-continuing TARGETS/RADAR navigation; STOP from RADAR; coexistence of R111S and synthetic beacon.
- Geographic fidelity: fresh target + valid receiver reference required, north-up coordinates rather than RF radar, no stale ghost plots.

## CYD1 earlier reference platform (different hardware, not D4H)

Earlier standalone ESP32-2432S028R functional acceptance documented 2026-10-12 includes: 12-minute two-target endurance (13 windows, 71,427 BLE callbacks, 9,438 validated RID packets), five-minute no-target rejection (25,904 ambient BLE callbacks and zero validated RID), and four-target lifecycle/reacquisition (124.920 sec, three windows, 11,540 BLE callbacks, 1,986 validated RID packets, zero allocation failures). **These statistics belong to CYD1 and must not be presented as Waveshare D4H results.**

## Open / pending validation

- Complete live R111S decode/regression under D4H; extended Waveshare endurance.
- Confirm LiPo pack physical condition, output voltage, polarity, exact connector fit, board charging current / voltage settings, battery-only boot, and safe supervised charge behavior before acceptance.
- Integrate LC76G GNSS, test cold/warm fixes, stale/invalid reference handling and scan-only operation without fix.
- Prototype 2.4GHz Wi-Fi Beacon Remote ID receiver; validate with DJI Mini 5 Pro only during appropriate lawful operation, and verify decoding rather than assuming transport from marketing.
- Assess BLE/Wi-Fi coexistence, channel schedule, packet loss and receiver responsiveness. Multi-transport V1 **not** yet accepted.
- Repeat STOP/HOME, PRIVATE/OPEN/SNAPSHOT, lifecycle and reset checks after GNSS/Wi-Fi changes.
- Distinguish RSSI reception strength from physical geographic range. Characterize actual range with measured separation and repeatable conditions.
- An earlier intermittent truncated serial diagnostic / target count inconsistency remains a low-severity unresolved QA observation.

## Test hygiene

Preserve raw logs **locally**, not in this repository; publish only sanitized summaries, redacted screenshots, and controlled non-identifying test artifacts. Log date, board revision, source hash, firmware, transmitter, environmental constraints, pass/fail and actual evidence for every future test.
