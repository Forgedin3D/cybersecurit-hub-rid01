> **Historical record — current-status notice (November 18, 2026):** The older material below preserves the development state at its original date. **RID-01 concluded at the R7 BLE/GNSS prototype on November 17, 2026.** The external LC76G GNSS integration was completed; Wi-Fi Remote ID and microSD logging were **not implemented**. Outdoor performance did not satisfy the intended field use, so development on this platform was stopped. For current information use [R7 status](waveshare/STATUS.md), [bounded validation](waveshare/R7_VALIDATION.md) and [final engineering decision](waveshare/R7_FINAL_ASSESSMENT.md). Earlier statements that GNSS is pending, BAT-D4E-3B is the current release, or Wi-Fi/SD will be developed next are **superseded**. Historical evidence claims retain their original limited scope.

> **Current-state reconciliation — November 6, 2026:** Read [Waveshare pre-GNSS status](waveshare/STATUS.md), [build procedure](waveshare/BUILD.md) and [validation](waveshare/VALIDATION.md) alongside this earlier record. The current provisional source is BAT-D4E-3B, not the plain D4H working file. Historical GNSS/Wi-Fi/battery and reset statements retain their original date and may be superseded by the linked records. This reconstruction does not rerun or extend historical acceptance.

# RID-01 architecture

**Project:** CyberSecurit Hub — RID-01  
**Current accepted board:** Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0  
**Status (2026-11-04):** BLE-based Remote ID and coordinate-derived geographic UI validated, with provisional LiPo telemetry and 45-minute battery-only mixed-use endurance. GNSS and Wi-Fi Beacon are **not yet integrated**.

## Purpose and limits

RID-01 is a **passive receiver for cooperative broadcast Remote ID**. It is a situational information aid, not a conventional RF radar, an aircraft detector for non-broadcast targets, a collision-avoidance system, or an enforcement instrument. Geographic positions are calculated from received target coordinates and a trusted receiver reference, not from RSSI.

## Current hardware

- Waveshare ESP32-S3-Touch-LCD-3.5 **Rev2.0**, 3.5-inch 480 × 320 landscape touchscreen.
- Integrated ESP32-S3 2.4 GHz BLE/Wi-Fi radio; current accepted firmware **receives BLE only**.
- Ruko R111S: primary real Remote ID BLE test transmitter.
- Beacon #1: synthetic, low-power SYSTEM-only bench transmitter (not representative of field range).
- External Waveshare LC76G GNSS module: ordered, not yet integrated.
- Protected single-cell 3.7V nominal / 2000mAh LiPo: polarity and on-device power operation tested; accepted battery-voltage/power/EST% display, 45-minute BLE-only mixed-use endurance passed. Estimated SOC calibration, cell temperature, end-of-charge qualification and low-voltage alerts remain open.
- DJI Mini 5 Pro: future real-aircraft target; its documented Wi-Fi Beacon Remote ID must **not** be described as already decoded by this BLE-only build.

## Firmware and UI data flow (accepted)

```text
BLE advertisements / OpenDroneID service data
   -> packet validation and message decoding
   -> mutex-protected multi-target state (T01-T04)
   -> per-target ACTIVE / STALE / LOST lifecycle
   -> TARGETS view, privacy-controlled snapshots
   -> geographic readiness checks
       -> fresh target position + valid receiver reference
       -> true-bearing / horizontal great-circle distance
       -> north-up coordinate-derived RADAR view
```

1. Operator accepts the cold-power-on notice before controls can start a BLE session.
2. HOME -> BEGIN -> PRIVATE/OPEN selection -> START -> TARGETS; RADAR is gated by reference and fresh geographic data.
3. One operator-owned continuous BLE session uses asynchronous scan windows with rollover until STOP or RESCAN.
4. Target positions are plotted **only** when fresh. Stale target coordinates are never retained as current plots. Never-allocated/LOST targets are hidden.
5. Right-side lifecycle label: ACTIVE green; STALE yellow; LOST removed. Fresh in-range plots cyan; off-scale plots yellow.
6. PRIVATE mode is the default and redacts sensitive fields in intended UI/serial outputs. OPEN is intentionally selected for controlled local diagnostics.
7. STOP returns to HOME after a successful scan stop; STOP errors must not falsely present a stopped state.
8. Full cold-power-on requires the acknowledgment notice. D4H historical validation reported an acknowledged warm RESET -> HOME, but a later instrumented physical RESET classified as a cold-like reset and re-required ACK. Preserve this distinction and don't assume all RESET paths retain ACK state.

**Receiver reference:** Currently supplied manually via serial `R,<latitude>,<longitude>` for controlled tests; `R,STATUS` and `R,CLEAR` are supported. Do not store real coordinates in this repository. If the receiver has no valid geographic reference, TARGETS scanning should still work; RADAR remains unavailable / acquiring until prerequisites are met.

## Battery telemetry and power behavior (latest accepted R2)

AXP2101 PMIC at I2C address `0x34`; firmware uses register-pointer reads, with no application charging-configuration writes in the audited code. The AXP2101 provides USB/battery presence, voltage and a **provisional SOC estimate**. Voltage/source/EST state is rendered on HOME, SELECT OUTPUT MODE, TARGETS and RADAR; operator notice and frozen snapshots deliberately exclude this live status. Battery status is **not a qualified fuel gauge** and no automatic voltage-based shutdown or low-battery alert has been accepted yet.

The 2026-11-04 **USB-only** read-only audit showed a stable 200mA programmed constant-current charger limit (`0x62=0x08`), 4.20V target (`0x64=0x03`) and enabled charger timers. This does **not** constitute actual current measurement with the battery attached or validated full-charge termination. The two-wire pack provides no direct battery cell-temperature sensor; manual supervision and startup caution are required.

The 2026-11-03 LiPo-only 45min mixed-use run used a local fixed receiver reference for the RADAR segment and the R111S. Reported display readings declined 3.88V/EST82% → 3.80V/EST64% with stable UI, but abrupt gauge changes across power/charging transitions remain unresolved. The eventual LC76G and Wi-Fi receiver will increase/change load, requiring renewed endurance and power acceptance.

## Next architecture increment — V1 target, not delivered

- **Sequence decision (2026-11-04):** battery closeout and repo audit first, then LC76G reference integration and full frozen BLE+battery+GNSS hardware acceptance **before** attempting Wi-Fi Beacon. Add **2.4 GHz Wi-Fi Beacon** Remote ID capture and parsing in a separate proof-of-concept afterward.
- Validate the actual ESP32-S3 RF coexistence and channel-coverage limitations before claiming concurrent BLE + Wi-Fi reception.
- Integrate the LC76G as a receiver-position provider; preserve manual-fixed test fallback and BLE/Wi-Fi scan-only functionality when GNSS has no valid fix.
- Perform real-device trials, packet-level validation, negative tests, BLE/Wi-Fi coexistence and measured ground/flight separation experiments.
- **5 GHz**, non-cooperative/hardened devices, and advanced RF architectures are explicitly deferred to a future revision.

## Toolchain / source-of-truth policy

Kali VM, Arduino CLI, ESP32 core 3.3.11, libraries pinned by the accepted local installation. Actual compiled and tested `.ino` source is currently on the operator's Kali machine; GitHub documentation does **not** claim to contain or reproduce that source yet. See [Baseline register](BASELINES.md), [Validation](VALIDATION.md), [Development](DEVELOPMENT.md), and [Data handling](SECURITY.md).

Project authorship and evidence include hands-on operator validation, assistant-supported engineering and documentation, and third-party hardware/libraries. See [Provenance](PROVENANCE.md).
