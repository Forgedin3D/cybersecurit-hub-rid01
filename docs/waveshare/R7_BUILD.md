# R7 verified source, build and reproducibility boundary

**Historical candidate:** `RX1024_RADAR_COLOR_R7`\
**Compile/upload date:** November 17, 2026\
**Disposition:** final *tested portfolio source*; discontinued for intended outdoor use because of unresolved field reception/continuity shortcomings.

## Exact source artifact

- [Full final `RID_UI_Migration.ino`](../../firmware/waveshare/final-r7/RID_UI_Migration/RID_UI_Migration.ino): **243,614 bytes**, SHA-256 `7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2`.
- Compiled application recorded SHA-256: `1240f5c17591fac55443f3bb57321a51f5925ab99e0fa4c8b10e68cac368b4a6`; not redistributed.
- The R7 compiled review package completed with **exit code 0**; reported application program storage **707,823 bytes (22%)** and static/global RAM **32,036 bytes (9%)**. These are compile figures, not field telemetry or dynamic heap measures.
- Guarded upload review reports successful flashing of four segments; the separate runtime guide records the **expected firmware identity**, not cryptographic attestation of the installed device.

## Environment recorded for this candidate

| Dependency or setting | Recorded value | Qualification |
|---|---|---|
| Arduino CLI | 1.5.1 | Historical Kali environment |
| Espressif Arduino ESP32 core | 3.3.11 | Built against ESP32-S3 architecture |
| Arduino GFX | 1.5.5 local compatibility copy | Includes local SPI-related modifications; upstream package alone may differ |
| SensorLib | 0.3.1 | External vendor library, notice retained |
| TCA9554 | 0.1.2 | External vendor library, notice retained |
| Build FQBN | `esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CPUFreq=240` | Captured build context; not a promise that all host settings are pinned |

The original dependency lock in [`DEPENDENCY_LOCK.json`](../../firmware/waveshare/DEPENDENCY_LOCK.json) is a *historical inventory* for a local library copy, **not** a complete vendored environment. Two compatibility SPI patches are retained under [`firmware/waveshare/patches/`](../../firmware/waveshare/patches/). Because the modified full third-party libraries are not bundled, **a clean Git clone alone is not sufficient to reproduce the recorded compilation**. This is a verified historical compile, not a continuously tested, hermetic CI build.

## Safe source integrity check

From the repository root, a reader may independently run this read-only check:

```bash
sha256sum firmware/waveshare/final-r7/RID_UI_Migration/RID_UI_Migration.ino
```

Expected: `7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2`.

**No flash command is recommended for casual readers.** The Python scripts in [RID-01 tools](../../tools/rid01/README.md) are preserved as environment-specific engineering examples; they may access USB serial ports and use historical local hashes. Do not run them against arbitrary hardware or mistake a syntax PASS for full runtime validation.

## Evidence and limitations

- [R7 bounded validation](R7_VALIDATION.md) and [public summary JSON](records/R7_ACCEPTANCE_SUMMARY.json).
- [Final engineering decision](R7_FINAL_ASSESSMENT.md), including the observed outdoor limitation and unresolved cause.
- [Source provenance](PROVENANCE.md), [historical pre-GNSS compile](records/CLEAN-COMPILE-REVIEW-2026-11-06.md), and [historical hardware milestones](VALIDATION.md).
