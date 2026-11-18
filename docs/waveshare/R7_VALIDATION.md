# R7 validation and test evidence (bounded)

**Data sources:** Accepted R7 source/compile review, recorded guarded upload receipts, completed private sanitized guided log `...111022Z.log`, accepted operator photographs and the living engineering record v1.41 Section 53. Preserve complete original logs in the private archive; this document is a public-facing measurement summary.

## Release identity and compilation

| Test | Result | Boundary |
|---|---|---|
| Exact R7 source | 243,614 bytes, SHA-256 `7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2` | Hash only identifies source |
| R7 compilation | Exit code 0, 707,823 B program, 32,036 B static globals | Not a hermetic clean-machine build; warning flags not a safety certification |
| Guarded upload | Four flash segments verified; uploader exit code 0 | Receipt-based, not independent flash readback |
| Privacy/source review | No obvious source secrets in bounded pattern inspection | Not full code/security audit |

## R7 completed guided session — November 17, 2026

| Observation | Recorded result | Interpretation |
|---|---|---|
| Guided capture | **396.652 s**; completed full protocol | One bounded bench run, not endurance/range validation |
| Four-source dashboard | **169 complete blocks**, all 4 ACTIVE in each accepted block | Host-retained complete blocks; not zero loss of every transmitted packet |
| First→last packet counters | T01 3→1073; T02 2→1537; T03 2→1208; T04 1→489 | Session counters; relative test-source rates differ |
| GNSS | **80/80 FRESH**; reported SATS 30–33 | Does not prove location accuracy or cold-start timing |
| Checksum/malformed | Checksum 1→1; malformed 0→0; long 0→0 | **Zero NEW** errors during this observation |
| UART | buf_full 2→2; FIFO overflow 11→11; framing/parity/break/other 0→0 | **Zero NEW** errors; initial nonzero counters predate capture |
| UART settings | RX requested/set **1,024 bytes**, initialized OK | Source and runtime receipts agree |
| GNSS service | Maximum recorded gap **25 ms**, queued bytes peak **278** | Not direct proof of buffer saturation or lifetime error absence |
| Final scan summary | 199,710 ms, 4 scan windows, 23,791 advertising callbacks, 4,317 transport RID fields, 4 tracked, 0 allocation failures | Bounded scan summary, not over-the-air denominator |
| HOST dashboard notices | **28 incomplete blocks**, final summary complete | Persistent host-parser limitation; exact cause not proven |
| UI flow | GNSS reference valid, RADAR ACQUIRING→READY, TARGETS, frozen SNAPSHOT→LIVE, RADAR, PRIVATE→OPEN→PRIVATE, STOP/HOME | Host action receipts plus selected operator observations/photos |

The two real R111S units were assigned **T01 and T03** in the completed color-acceptance session; T02/T04 were SYSTEM beacons without usable fresh geographic plot data. **Blue and light-gray plotted targets were not physically demonstrated** in this session. The brief earlier R7 guided attempt ended prematurely and is documented as an interrupted preliminary observation, not merged into completed metrics.

## UART troubleshooting progression

The earlier **256-byte RX** diagnostic recorded **30 checksum and 5 malformed errors** under its run. Separate **1,024-byte RX** tests recorded **1 checksum / 0 malformed** under an R111S run, and later **0 / 0** in R111S-only controls. This is evidence supporting the buffer-size change and improved servicing, **not a perfectly controlled RF/environment-matched proof of exclusive root cause**. Four-source SYSTEM beacon runs also produced recurring host dashboard-parser notices, not proven to be firmware UART errors.

## Other bounded capability tests

- **R5:** touchscreen GNSS reference selection, RADAR readiness, RESCAN behavior, and LiPo-only functional navigation with two R111S sources were observed. The standalone battery readings were voltage/provisional percentage, not calibrated state-of-charge, certified protection or charging-performance tests.
- **R6:** redesigned RADAR corner callouts and leader geometry were hardware tested; R6 remains the preserved rollback.
- **R7:** palette-only differentiation of plotted target tag/dot/leader, without modifying BLE, GNSS or RADAR projection logic.

## Outdoor evaluation and stop decision

The operator later placed R111S targets at separate property locations and reported intermittent target disappearance as separation increased, including around/beyond **~35 m** in that setup. Outdoor photographs and the account are useful negative field evidence. They do not establish a calibrated 35 m radio threshold or isolate RF reception from geographic-freshness effects. **The operator rejected the board for intended field use and stopped development**. This decision supersedes future-milestone language in the living engineering record.

## Explicit non-claims

No commercial field readiness, 250–500 m reception, complete FAA Remote ID compliance, Wi-Fi Remote ID, microSD, full battery qualification, independent geographic accuracy, guaranteed aircraft detection, antenna-radiation certification, four simultaneously geo-plotted commercial aircraft, zero lifetime UART errors, or zero host-parser notices.
