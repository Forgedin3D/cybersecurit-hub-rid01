# RID-01 architecture

**Project:** CyberSecurit Hub — RID-01  
**Current accepted board:** Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0  
**Status (2026-10-29):** BLE-based Remote ID receiver and geographic UI validated; Wi-Fi Beacon and onboard GNSS **not yet integrated**.

## Purpose and limits

RID-01 is a **passive receiver for cooperative broadcast Remote ID**. It is a situational information aid, not a conventional RF radar, an aircraft detector for non-broadcast targets, a collision-avoidance system, or an enforcement instrument. Geographic positions are calculated from received target coordinates and a trusted receiver reference, not from RSSI.

## Current hardware

- Waveshare ESP32-S3-Touch-LCD-3.5 **Rev2.0**, 3.5-inch 480 × 320 landscape touchscreen.
- Integrated ESP32-S3 2.4 GHz BLE/Wi-Fi radio; current accepted firmware **receives BLE only**.
- Ruko R111S: primary real Remote ID BLE test transmitter.
- Beacon #1: synthetic, low-power SYSTEM-only bench transmitter (not representative of field range).
- External Waveshare LC76G GNSS module: ordered, not yet integrated.
- Single-cell LiPo battery: candidate pack acquired, **voltage, connector polarity, charge settings and battery-powered operation not yet accepted**.
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
8. An acknowledged warm RESET returns to HOME. A cold power-on, or RESET before acknowledgment, requires the notice.

**Receiver reference:** Currently supplied manually via serial `R,<latitude>,<longitude>` for controlled tests; `R,STATUS` and `R,CLEAR` are supported. Do not store real coordinates in this repository. If the receiver has no valid geographic reference, TARGETS scanning should still work; RADAR remains unavailable / acquiring until prerequisites are met.

## Next architecture increment — V1 target, not delivered

- Add **2.4 GHz Wi-Fi Beacon** Remote ID capture and parsing in a separate proof-of-concept first.
- Validate the actual ESP32-S3 RF coexistence and channel-coverage limitations before claiming concurrent BLE + Wi-Fi reception.
- Integrate the LC76G as a receiver-position provider; preserve manual-fixed test fallback and BLE/Wi-Fi scan-only functionality when GNSS has no valid fix.
- Perform real-device trials, packet-level validation, negative tests, BLE/Wi-Fi coexistence and measured ground/flight separation experiments.
- **5 GHz**, non-cooperative/hardened devices, and advanced RF architectures are explicitly deferred to a future revision.

## Toolchain / source-of-truth policy

Kali VM, Arduino CLI, ESP32 core 3.3.11, libraries pinned by the accepted local installation. Actual compiled and tested `.ino` source is currently on the operator's Kali machine; GitHub documentation does **not** claim to contain or reproduce that source yet. See [Baseline register](BASELINES.md), [Validation](VALIDATION.md), [Development](DEVELOPMENT.md), and [Data handling](SECURITY.md).

Project authorship and evidence include hands-on operator validation, assistant-supported engineering and documentation, and third-party hardware/libraries. See [Provenance](PROVENANCE.md).
