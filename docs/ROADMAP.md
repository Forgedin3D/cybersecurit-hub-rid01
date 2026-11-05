# Product and engineering roadmap

Status updated: **2026-11-04** (historical October 29 baseline retained below). The name RID-01 is an internal engineering designation; future marketing names are undecided.

## Current accepted baseline

- [x] CYD1 BLE reference firmware functional acceptance (2026-10-12).
- [x] Waveshare ESP32-S3 UI migration, multi-target BLE receiver, north-up coordinate display.
- [x] D4E/D4F RADAR lifecycle and labeling.
- [x] D4G STOP/HOME hardening.
- [x] D4H cold-power-on operator acknowledgment (historical acceptance; physical RESET behavior clarified in later diagnostic).
- [x] BAT-D4B/BAT-D4C/BAT-D4D-R2 power header, safety notice, provisional EST% UI, clean compile, USB/LiPo functional tests and frozen SHA-256 archives (2026-10-30/31).
- [x] BAT-D4E-1 supervised 45-minute mixed HOME/TARGETS/RADAR LiPo-only endurance; BAT-D4E-2B stable USB-only AXP2101 register audit and accepted R2 restore (2026-11-03/04).

- [x] BAT-D4E-3B voltage-based operator LOW BATTERY warning **provisional source freeze** (2026-11-04): SHA `2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670`; compiled, 12/12 simulated FSM, physical display UI sim, USB-only controls, healthy-voltage LiPo run, live R111S decode. Actual low-voltage trigger, calibrated SOC and charging termination remain open.

- [x] Historical **CYD1 2026-10-12 reference-source reconstruction** completed 2026-11-04: accepted source and key intermediate builds SHA-verified and imported with selected metadata-cleaned photographs, original test-log-reconciled validation, troubleshooting case study and provenance. This is a **retrospective Git import**, not a CYD1 reflash or retest.

## V1 required capabilities and sequence (operator-approved order, 2026-11-04)

1. **Pre-GNSS battery closeout** — provisional low-voltage warning BAT-D4E-3B has now been acceptance-frozen with source SHA; finalize battery operator guidance, charger/temperature residual-risk register and optional manual-reference loss test. Keep true SOC, low-voltage physical activation and full-charge termination OPEN for later controlled qualification. 45-min R2 and 15-min 3B battery-only mixed-use tests passed; neither proves total capacity/runtime, GNSS-loaded runtime or full-charge safety.
2. **Repository consolidation** — update current docs and historical acceptance log; inventory Kali archived sources/reports and prior thread handoffs; review exact source hashes and licenses/privacy; import the audited latest accepted source (currently frozen BAT-D4D-R2) with reproducible build notes, not original unreviewed logs. Earlier D4H working local source is not the currently flashed accepted R2; do not conflate. Keep repo private pending review.
3. **LC76G GNSS as receiver reference** — validate power/logic/UART pinout, initial valid fix, fix freshness, invalid/lost-fix, manual fixed reference fallback, and persistent scan-only TARGETS operation without fix. Do not make GNSS a prerequisite for BLE-only detection.
4. **Freeze full BLE + battery + GNSS reference baseline** — physical battery endurance, UI/ack/STOP/HOME/PRIVACY/SNAPSHOT, real R111S, multi-target lifecycle, fresh geographic RADAR, missing-fix regression, build hashes and immutable acceptance record. This is the rollback point for new radio work.
5. **2.4 GHz Wi-Fi Beacon RID after frozen reference baseline** — isolated Wi-Fi-only reception + ODID parsing; controlled real DJI Mini 5 Pro actual transport/channel verification; then BLE/Wi-Fi coexistence and channel hopping on shared ESP32-S3 radio. Compare throughput, latency, target dropouts, power and UI behavior against the BLE+GNSS frozen baseline. Wi-Fi plus BLE remains a V1 goal, **not** a presently proven feature. Reassess architecture if shared-radio reliability fails.
6. **Full V1 acceptance** — combined transport, RF/range, lifecycle/privacy and safety regressions, enclosure CAD only after actual boards/battery/GNSS layout, truthful portfolio and licensing review.

Tracking: [Battery qualification Issue #7](https://github.com/Forgedin3D/cybersecurit-hub-rid01/issues/7); [Source import and portfolio consolidation Issue #6](https://github.com/Forgedin3D/cybersecurit-hub-rid01/issues/6).

## Deferred

- 5GHz Wi-Fi / other bands.
- Non-cooperative aircraft detection, proprietary/hardened radio decoding, advanced RF direction finding.
- More powerful radio board(s) and expanded revision (possible RID-02).
- Any claim of safety certification or operational drone avoidance function.

## Principles

Keep tested firmware immutable by accepted hash. Use isolated firmware candidate changes. Do not conflate user goals, research feasibility, compiled builds, and real-hardware acceptance.
