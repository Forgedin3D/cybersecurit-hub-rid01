# Engineering case study — from BLE receiver to GNSS-equipped handheld

## Problem and objective

Develop a passive, handheld experimental receiver for cooperative aircraft Remote ID broadcasts, with a touchscreen TARGETS view, geographic RADAR display and a repeatable validation process. The project was intended to integrate wireless-security and UAS/RF interests, including operator experience with Part 107 aviation work. This is educational cybersecurity/embedded-systems R&D, not an FAA-certified product or active radar.

## Approach

The project began on a CYD1 ESP32 board and moved to a Waveshare ESP32-S3 touchscreen. Firmware in Arduino/C++ handled BLE advertisements, packet interpretation, target-slot lifecycle, touch navigation and geographic plotting. An external LC76G GNSS module provided a receiver reference over serial NMEA. Kali Linux and Python scripts supported isolated build candidates, source integrity checks, guarded flash operations, privacy-controlled logs and bounded test protocols.

**Hardware operations and acceptance were performed by the project owner, with substantial AI assistance in coding, scripts and analysis.** Third-party libraries and board examples were identified independently from project-created work.

## Significant technical milestones

1. **Four-target receiver and lifecycle.** Track four session slots, distinguish ACTIVE/STALE/LOST, avoid plotting stale or absent geography, and preserve identity/state behavior during snapshots and RESCAN.
2. **GNSS wiring and pinout correction.** Investigate conflicting seller documentation, verify the official board pin reference, establish UART reception on GPIO44, and repeat successful NMEA/GNSS reception with compact bench wiring.
3. **UART receive-integrity investigation.** Instrument checksum/malformed and hardware UART error counters, compare 256-byte and 1,024-byte RX settings in isolated firmware candidates, and retain distinct traces for R111S versus slower ESP32 SYSTEM beacons. The revised buffer coincided with substantially lower observed error increments.
4. **Display scheduling and UI redesign.** Test TARGETS→SNAPSHOT→LIVE→RADAR interactions while GNSS and BLE were active. Split the clustered RADAR target annotations into four fixed corner callouts with true-dot leader lines (R6). Add cool, slot-specific colors for geographic callouts (R7) without altering projection and state logic.
5. **Battery-only functional observation.** Power the prototype from its LiPo input, observe a successful bounded UI/radio/GNSS interaction, and explicitly record the limitations of voltage-based percentage estimation and unqualified cell-temperature monitoring.
6. **Build and validation workflow.** Preserve exact hashes and rollback, stage candidates independently, separate compile from upload, and use Python helpers to capture sanitized logs. The R7 completed guide observed four tracked sources with no new GNSS/UART error counts and the known host parser limitations retained.

## What the final evidence shows

The final R7 code is **243,614 bytes** with SHA-256 `7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2`. The completed R7 guided observation lasted **396.652 seconds**, returned **169 complete four-target dashboard records**, and recorded zero new GNSS/UART receive-error increments. The two real R111S targets provided plottable geographic information; two controlled ESP32 beacons were tracked as SYSTEM sources. **28 incomplete-dashboard HOST notices** remain an openly documented constraint.

The outdoor trials then challenged the system's suitability for a portable field receiver. The operator reported repeated target loss at modest separation, approximately around/beyond 35 m in the tested environment, and judged the RF/antenna and power architecture unsatisfactory for the intended mission. No calibrated experiment demonstrated a universal 35 m limit or fully isolated the root cause.

## Why the effort concluded

The decision was to stop investment in this hardware, not to add microSD or Wi-Fi features to compensate for unreliable outdoor usefulness. A functioning bench prototype and test record remain; the device is **not** marketed or used as an operational field product. The source, curated images, independent build/acceptance facts, historical source lineage and final hardware rejection are being preserved for employers to review.

## Demonstrated skills and learning boundaries

The owner can discuss equipment selection, wiring, documented requirements, UART tests, source-hash verification, safe flashing gates, interpreting controlled logs, identifying design limitations and working through a multi-stage engineering process. The project used **AI-assisted software development**; it does not represent a claim that the owner personally authored or memorized all Arduino/C++ and Python source. Follow-up Python and blue-team SIEM/network monitoring study are separate growth areas, not retroactive achievements of RID-01.

## Links

[Source](../../firmware/waveshare/final-r7/RID_UI_Migration/RID_UI_Migration.ino) · [Architecture](R7_ARCHITECTURE.md) · [Validation](R7_VALIDATION.md) · [Final assessment](R7_FINAL_ASSESSMENT.md) · [Python workflow](../../tools/rid01/README.md) · [Gallery](R7_MEDIA.md)
