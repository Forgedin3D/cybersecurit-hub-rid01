# RID-01 — Passive Remote ID Handheld

**CyberSecurit Hub | Embedded systems · wireless protocols · UAS situational information**

An independently tested touchscreen **passive cooperative Remote ID receiver** built on a Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0. The existing firmware processes **Bluetooth Low Energy (BLE) Remote ID** data, manages multiple targets and lifecycle states, and displays coordinate-derived geographic context. **2.4 GHz Wi-Fi Beacon Remote ID and onboard GNSS are V1 requirements still under development.**

> **Engineering status (2026-11-04):** Latest *accepted and physically flashed* local image: **BAT-D4D-R2**, provisional power/voltage/EST% UI, SHA-256 `a485eadad7af3ac706db9e10e053e492bd48b7eb61460f17374c989bf00d2d0e`. On November 4 the operator successfully restored it after a read-only PMIC diagnostic (691,167 bytes sketch, 31,652 bytes globals). D4H remains an earlier accepted rollback and the original local working `.ino` still reflects D4H. The reviewed source is **not yet uploaded to this documentation-only repository**. Current post-restore UI test is pending operator confirmation.

## Current validated behavior

- Passive BLE Remote ID parsing and up to **four tracked target slots** (T01–T04).
- ACTIVE/STALE/LOST lifecycle, target reacquisition, continuous scan windows, operator-owned START/STOP/RESCAN.
- TARGETS and a **north-up, coordinate-derived RADAR-style display**. **Not** an RF radar and **not** RSSI-based ranging.
- Geographic plotting requires a valid receiver reference **and fresh target coordinates**. Stale coordinates are not plotted; LOST targets are hidden.
- PRIVATE (default) / OPEN output modes and frozen SNAPSHOT handling.
- Physically tested cold-power-on acknowledgment and D4G STOP/HOME. Historical D4H report recorded 5/5 pass; a later instrumented physical RESET check showed the board may re-require acknowledgment after RESET. See [validation](docs/VALIDATION.md) for the dated correction.

## V1 target (not yet delivered)

| Capability | Current state |
| --- | --- |
| BLE Remote ID | Implemented and previously validated |
| 2.4 GHz Wi-Fi Beacon Remote ID | Required for V1; proof-of-concept and real-DJI testing pending |
| GNSS receiver position | LC76G ordered; integration pending |
| BLE + Wi-Fi combined operation | Required target; ESP32-S3 coexistence/channel performance unproven |
| Battery operation/charging | Polarity and controlled LiPo operation validated; 45-min mixed-use BLE-only endurance PASS; read-only USB-only PMIC audit PASS. Remaining SOC accuracy, low-voltage alert and charger termination qualification OPEN |
| 5 GHz / non-cooperative RF | **Not part of V1**; future hardware research |

## New accepted work: battery engineering (October 30–November 4, 2026)

- **BAT-D4B** (2026-10-30): Power/voltage header across supported screens; accepted source SHA-256 `f26d860255f933de068c71b4ce8960187cbcad6ffa634ed051c8fd0277eba3ea`.
- **BAT-D4C** (2026-10-30): Cell-temperature-sensing limitation and operator charging-safety notice; SHA-256 `5a55a5d9fe0b95ecf4943199793fbdba87016ca10d1fa5bf12ea138dfb8e314e`.
- **BAT-D4D-R2** (2026-10-31): Provisional `EST:%` and voltage/power state displayed on HOME, SELECT OUTPUT MODE, TARGETS and RADAR; ACK/SNAPSHOT excluded. Live power-source tests PASS; accepted/frozen hash `a485eadad7af3ac706db9e10e053e492bd48b7eb61460f17374c989bf00d2d0e`.
- **BAT-D4E-1** (2026-11-03): 45-minute supervised LiPo-only mixed-use HOME/TARGETS/RADAR/STOP, real Ruko R111S reception and navigation; observed 3.88V / EST82% at start and 3.80V / EST64% at end, ~9,000 approximate on-device counter units (not independently validated loss statistic). **PASS limited 45-min mixed-use test**, not maximum endurance.
- **BAT-D4E-2B** (2026-11-04): 120-sec USB-only read-only AXP2101 register audit, stable charger parameters (programmed 200mA CC, 4.20V CV), no LiPo attached; test firmware subsequently removed and R2 flashed and hash-verified. Sudden earlier SOC transition of about 75% to 30% on USB connection remains unexplained and the gauge **is not calibrated**.
- Direct LiPo temperature monitoring is absent; supervised charging only. See [baselines](docs/BASELINES.md), [validation](docs/VALIDATION.md) and [Issue #7](https://github.com/Forgedin3D/cybersecurit-hub-rid01/issues/7).

**Agreed sequence:** finish pre-GNSS battery closeout → audit/import code and consolidate repository → integrate LC76G → **freeze a fully tested BLE + battery + GNSS rollback** → only then attempt Wi-Fi Beacon on the shared ESP32-S3 radio. See the [updated roadmap](docs/ROADMAP.md).

## Hardware and tools

Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 (480 × 320), Kali Linux VM, Arduino CLI, Espressif Arduino core 3.3.11; Ruko R111S and low-power synthetic bench beacon for validation. DJI Mini 5 Pro is planned for Wi-Fi Remote ID demonstrations **only after reception is implemented and verified**.

## Engineering records

- **[Full project handoff — October 29, 2026](docs/HANDOFF_2026-10-29.md)** (next-thread recovery, firmware hashes, test evidence and remaining gates)
- [Architecture and technical constraints](docs/ARCHITECTURE.md)
- [Firmware acceptance baselines and hashes](docs/BASELINES.md)
- [Validation evidence and open QA](docs/VALIDATION.md)
- [V1 roadmap and scope boundaries](docs/ROADMAP.md)
- [Local development, source import and release process](docs/DEVELOPMENT.md)
- [Data protection and responsible test practices](docs/SECURITY.md)
- [Project provenance and AI/third-party attribution](docs/PROVENANCE.md)

## Scope and safety

This is an **informational receiver of cooperative broadcasts**, not a certified collision-avoidance instrument or a guarantee of aircraft detection. Broadcast content may be incomplete, delayed, wrong, or spoofed. Flight operations remain subject to applicable rules; this device alone must not be used to make flight-safety decisions.

**Repository visibility:** Private. No code license granted at this stage; third-party source/license obligations will be reviewed before publication. Development and documentation are human-directed with assistant support; see [provenance](docs/PROVENANCE.md).
