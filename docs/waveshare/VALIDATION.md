# Waveshare evidence and validation matrix

**Prepared November 6, 2026.** Each row identifies its evidence class. Historical source identity and acceptance-record recovery do not constitute new tests.

| Scope/build | Evidence | Supported result | Not established |
|---|---|---|---|
| Migration/display/touch | Named source checkpoints; retrospective engineering report; actual photos | Landscape mapping and staged integration recorded | Calibrated touch accuracy; new recompile of each stage |
| D4A geometry | Historical report of synthetic numerical cases | Cardinal/diagonal solver checks recorded | Outdoor GNSS accuracy or RF range |
| D4D/D2C.1 visual | Source fixture and matching four-cardinal layout photos | Repeatable synthetic on-device display | Four live geographic transmitters |
| D4E lifecycle | Source guard; historical handoff; longer visual clip | Plot expiry, row removal, independent nongeographic source | Exact last-packet-to-pixel latency |
| D4F/D4G | Source diffs; later handoff; lifecycle-label clip | Label evolution and failed-STOP path present/recorded | Fresh execution of failure injection in this review |
| D4H versus BAT-D4C | Both original local acceptance records retained | Later instrumented physical RESET returns to ACK | Universal warm/software-reset behavior |
| BAT-D4B/C/D-R2 | Recovered local acceptance records and media | Header, notice, estimate and intended screen coverage | Calibrated gauge/capacity |
| BAT-D4E-1 on R2 | Operator timeline and Nov 3 handoff | 45-minute supervised battery-only mixed UI/R111S operation | Maximum endurance, quantified packet loss, current 3B loaded endurance |
| BAT-D4E-2B | Operator-provided 120 s USB-only register dump and source | Stable read-only PMIC register snapshots with battery absent | Actual battery charge current/termination/thermal safety |
| BAT-D4E-3A | Historical self-test and explicitly labeled visual simulation | 12/12 simulated logic cases reported; warning paint path tested | Real low-cell activation |
| BAT-D4E-3B | Recovered provisional acceptance record | Historical compile; USB-only UI; 15-min LiPo regression; two R111S captures with 1,012 and 961 reported validated packets | Absolute packet-loss metric; current-build transmitter-off STALE→LOST acceptance |
| November 6 demonstration | User setup description and new photos | R111S + 3 SYSTEM beacons; four ACTIVE rows, one plot, FIXED reference | Installed binary hash; four actual aircraft |
| New host-only warning check | Exact-source extraction and C++ host test | 23/23 assertions passed during this staging pass | ESP32 compile, PMIC/ADC, real scheduling, physical battery safety |
| Fresh isolated ESP32 build | November 6 operator-run evidence ZIP, command, board resolution and verbose log | **PASS; exit 0; 691,519 program bytes; 31,668 global/static bytes; recorded input integrity preserved** | Warnings suppressed; no flash, runtime retest, binary equality or clean-machine reproduction |

## Original acceptance records

The `records/` folder contains the archived BAT-D4B, BAT-D4C, BAT-D4D-R2, BAT-D4E-3B and D4H records. Their original bytes are retained. D4H's earlier reset-retention claim must be read alongside the later correction in BAT-D4C, not as the current guarantee.

## Host-only test scope

`tools/test_waveshare_voltage_warning.py` verifies the exact current source hash, extracts the two data structures and pure warning step function, then compiles them with a host C++ compiler. Assertions include activation/recovery boundaries, interrupted dwell, USB and invalid telemetry resets, SOC independence and 32-bit timestamp rollover. It does not test the main-loop poll scheduler, I2C reads, renderer, concurrent BLE behavior or fault-free continuous voltage monitoring.

## Open questions carried forward

Physical low-cell warning threshold; charge termination; SOC discontinuity/calibration; battery-cell temperature sensing absent; meaningful outdoor geographic movement; source/log attribution of READY→ACQUIRING→READY transient; malformed USB summaries; GNSS integration; Wi-Fi transport/channel/coexistence; final enclosure and integrated endurance. Known limits are not silently closed by adding documentation or photographs.

[Evidence source register](EVIDENCE_SOURCES.md).
