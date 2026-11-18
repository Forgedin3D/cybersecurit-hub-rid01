# Final R7 firmware — exact tested source

**Candidate:** `RX1024_RADAR_COLOR_R7` · November 17, 2026\
**Platform:** Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0 with external LC76G GNSS.\
**Status:** Bounded compile/upload/runtime acceptance PASS. **Retired from intended outdoor field use.**

- `RID_UI_Migration/RID_UI_Migration.ino` — **243,614 bytes; 11,973 lines**; SHA-256 `7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2`.
- Recorded compiled application `.bin` SHA-256: `1240f5c17591fac55443f3bb57321a51f5925ab99e0fa4c8b10e68cac368b4a6`. **The compiled binary is not redistributed here**.
- The source is published byte-for-byte from the accepted compile-review archive; it has not been refactored or altered for readability.
- Original/source archive includes a disabled (`#if 0`) legacy CYD1 section; its presence is historical, not a second active firmware implementation.
- The older [Waveshare reference sketch](../reference/RID_UI_Migration/RID_UI_Migration.ino) is `BAT-D4E-3B` (November 2026 pre-GNSS) and should not be mistaken for R7.

See also [final build and reproducibility notes](../../../docs/waveshare/R7_BUILD.md) and [machine-readable acceptance summary](../../../docs/waveshare/records/R7_ACCEPTANCE_SUMMARY.json).

## Recorded compile environment

- Arduino CLI **1.5.1** (historically recorded) and Espressif Arduino ESP32 core **3.3.11**.
- Recorded dependencies: `GFX Library for Arduino` **1.5.5** local compatibility copy, `SensorLib` **0.3.1**, `TCA9554` **0.1.2**, plus ESP32 core's Wire/SPI/BLE components.
- Recorded FQBN: `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CPUFreq=240`.
- Returned R7 compile exit code **0**; program storage **707,823 bytes (22%)**, global/static allocation **32,036 bytes (9%)**. These are compiler figures, not measured runtime heap.
- Four upload flash segments verified in the uploader receipts; this is not an independent readback of installed application code.

**Reproducibility limitation:** A fresh clone alone does **not** contain the complete locally modified compatibility libraries or exact original build environment. Do not claim a clean-machine reproducible build from this repository alone or run historical upload tools on arbitrary hardware.

## SHA-256 check

```bash
sha256sum firmware/waveshare/final-r7/RID_UI_Migration/RID_UI_Migration.ino
```

Expected: `7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2`. The source hash proves file identity, not operational performance. Historical source/build receipts and exact toolchain inventory remain preserved privately.

See [validation](../../../docs/waveshare/R7_VALIDATION.md), [architecture](../../../docs/waveshare/R7_ARCHITECTURE.md), [final assessment](../../../docs/waveshare/R7_FINAL_ASSESSMENT.md) and [third-party notices](../notices).
