# Product and engineering roadmap

Status date: **2026-10-29**. The name RID-01 is an internal engineering designation; future marketing names are undecided.

## Current accepted baseline

- [x] CYD1 BLE reference firmware functional acceptance (2026-10-12).
- [x] Waveshare ESP32-S3 UI migration, multi-target BLE receiver, north-up coordinate display.
- [x] D4E/D4F RADAR lifecycle and labeling.
- [x] D4G STOP/HOME hardening.
- [x] D4H cold-power-on operator acknowledgment with pre/post-ack warm reset behavior (five physical tests).

## V1 required capabilities and sequence

- [ ] Battery qualification: polarity/connector fit, safe charge settings, battery-only operation, controlled endurance (only after safe electrical checks).
- [ ] Waveshare D4H real Remote ID R111S regression/endurance and controlled range tests.
- [ ] LC76G GNSS integration: real receiver reference, cold/warm fix status, invalid/lost-fix handling, preserve manual-reference test fallback.
- [ ] Home UX: show receiver GNSS status; preserve **scan-only** access on no fix, while gating geographic RADAR features as appropriate.
- [ ] Wi-Fi-only 2.4GHz Beacon Remote ID capture proof-of-concept and protocol decoding.
- [ ] DJI Mini 5 Pro real-aircraft Wi-Fi Beacon test, with lawful flight and separate airspace/flight safety management.
- [ ] Combined BLE + Wi-Fi transport support as **standard V1 requirement**, validated against ESP32-S3 shared-radio coexistence limits. If concurrent capture fails qualification, prototype scheduling/alternative design; do not advertise simultaneous capture without evidence.
- [ ] End-to-end integrated lifecycle, privacy, snapshot, STOP/HOME, RESET, cold boot, receiver-reference and field regression.
- [ ] Final power/range characterization, enclosure CAD and physical fit/thermal/antenna checks **after** hardware layout and functional validation.
- [ ] Curate truthful employer portfolio; review licensing/security before any public source or website release.

## Deferred

- 5GHz Wi-Fi / other bands.
- Non-cooperative aircraft detection, proprietary/hardened radio decoding, advanced RF direction finding.
- More powerful radio board(s) and expanded revision (possible RID-02).
- Any claim of safety certification or operational drone avoidance function.

## Principles

Keep tested firmware immutable by accepted hash. Use isolated firmware candidate changes. Do not conflate user goals, research feasibility, compiled builds, and real-hardware acceptance.
