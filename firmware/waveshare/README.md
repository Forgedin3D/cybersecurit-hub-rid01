> **Historical record — current-status notice (November 18, 2026):** The older material below preserves the development state at its original date. **RID-01 concluded at the R7 BLE/GNSS prototype on November 17, 2026.** The external LC76G GNSS integration was completed; Wi-Fi Remote ID and microSD logging were **not implemented**. Outdoor performance did not satisfy the intended field use, so development on this platform was stopped. For current information use [R7 status](../../docs/waveshare/STATUS.md), [bounded validation](../../docs/waveshare/R7_VALIDATION.md) and [final engineering decision](../../docs/waveshare/R7_FINAL_ASSESSMENT.md). Earlier statements that GNSS is pending, BAT-D4E-3B is the current release, or Wi-Fi/SD will be developed next are **superseded**. Historical evidence claims retain their original limited scope.

# Waveshare reference and curated historical source

**Looking for the final tested implementation?** See [R7 source and build record](final-r7/README.md). This directory preserves a **November 6 pre-GNSS historical snapshot** and does not represent the final firmware.

`reference/RID_UI_Migration/RID_UI_Migration.ino` is the byte-identical **BAT-D4E-3B** provisional accepted archive, not the older plain working master on Kali. SHA-256:

```text
2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670
```

`history/` contains seven selected historical/rollback sketches as `.ino.txt` to prevent accidental co-compilation. `fixtures/` contains the explicitly synthetic visual sketch, also `.ino.txt`, and is not production firmware. `SOURCE_MANIFEST.json` maps all nine source copies to original archived names/hashes. `HISTORICAL_CHECKPOINTS.json` retains 33 canonical working-area checkpoint records without duplicating the entire archive.

`patches/` preserves the two graphics compatibility changes and four selected source diffs. Do not run or apply historical patches to the accepted sketch. `notices/` preserves supplied third-party license/version metadata; full libraries are not vendored. `DEPENDENCY_LOCK.json` records the 539 inventoried compatibility files for local verification, not a complete compiler backup.

The fresh isolated ESP32 compile **passed on Kali on November 6, 2026**, and its returned evidence has been reviewed. Follow [BUILD.md](../../docs/waveshare/BUILD.md) and the [recorded result](../../docs/waveshare/records/CLEAN-COMPILE-REVIEW-2026-11-06.md). The host-only warning test remains separate. No firmware upload or runtime acceptance of the newly generated image occurred; final import approval is pending.

See [current state](../../docs/waveshare/STATUS.md), [validation](../../docs/waveshare/VALIDATION.md) and [provenance](../../docs/waveshare/PROVENANCE.md). No new project-wide license is granted by this private historical import.
