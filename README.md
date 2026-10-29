# RID-01 — Passive Remote ID Handheld

**CyberSecurit Hub | Embedded systems · wireless protocols · UAS situational information**

An independently tested touchscreen **passive cooperative Remote ID receiver** built on a Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0. The existing firmware processes **Bluetooth Low Energy (BLE) Remote ID** data, manages multiple targets and lifecycle states, and displays coordinate-derived geographic context. **2.4 GHz Wi-Fi Beacon Remote ID and onboard GNSS are V1 requirements still under development.**

> **Engineering status (2026-10-29):** Current accepted local firmware: **STAGE1B-D4H**, cold-power-on operator acknowledgment, SHA-256 `7bdb2be698c63aba895e5b892f29478f8e215807126eb155bac9abd8fcabbfcf`. The validated firmware source remains on the operator's Kali VM and is **not yet uploaded to this repository**. Repository commits begin later than the original development work.

## Current validated behavior

- Passive BLE Remote ID parsing and up to **four tracked target slots** (T01–T04).
- ACTIVE/STALE/LOST lifecycle, target reacquisition, continuous scan windows, operator-owned START/STOP/RESCAN.
- TARGETS and a **north-up, coordinate-derived RADAR-style display**. **Not** an RF radar and **not** RSSI-based ranging.
- Geographic plotting requires a valid receiver reference **and fresh target coordinates**. Stale coordinates are not plotted; LOST targets are hidden.
- PRIVATE (default) / OPEN output modes and frozen SNAPSHOT handling.
- Physically tested cold-power-on acknowledgment, reset gating, and D4G STOP/HOME behavior (**D4H five-test acceptance: 5/5 PASS**).

## V1 target (not yet delivered)

| Capability | Current state |
| --- | --- |
| BLE Remote ID | Implemented and previously validated |
| 2.4 GHz Wi-Fi Beacon Remote ID | Required for V1; proof-of-concept and real-DJI testing pending |
| GNSS receiver position | LC76G ordered; integration pending |
| BLE + Wi-Fi combined operation | Required target; ESP32-S3 coexistence/channel performance unproven |
| Battery operation/charging | Hardware evaluation pending; pack **must not be connected** until verified |
| 5 GHz / non-cooperative RF | **Not part of V1**; future hardware research |

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
