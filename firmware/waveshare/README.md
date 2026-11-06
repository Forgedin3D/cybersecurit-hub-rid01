# Waveshare reference and curated historical source

`reference/RID_UI_Migration/RID_UI_Migration.ino` is the byte-identical **BAT-D4E-3B** provisional accepted archive, not the older plain working master on Kali. SHA-256:

```text
2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670
```

`history/` contains seven selected historical/rollback sketches as `.ino.txt` to prevent accidental co-compilation. `fixtures/` contains the explicitly synthetic visual sketch, also `.ino.txt`, and is not production firmware. `SOURCE_MANIFEST.json` maps all nine source copies to original archived names/hashes. `HISTORICAL_CHECKPOINTS.json` retains 33 canonical working-area checkpoint records without duplicating the entire archive.

`patches/` preserves the two graphics compatibility changes and four selected source diffs. Do not run or apply historical patches to the accepted sketch. `notices/` preserves supplied third-party license/version metadata; full libraries are not vendored. `DEPENDENCY_LOCK.json` records the 539 inventoried compatibility files for local verification, not a complete compiler backup.

The fresh isolated ESP32 compile **passed on Kali on November 6, 2026**, and its returned evidence has been reviewed. Follow [BUILD.md](../../docs/waveshare/BUILD.md) and the [recorded result](../../docs/waveshare/records/CLEAN-COMPILE-REVIEW-2026-11-06.md). The host-only warning test remains separate. No firmware upload or runtime acceptance of the newly generated image occurred; final import approval is pending.

See [current state](../../docs/waveshare/STATUS.md), [validation](../../docs/waveshare/VALIDATION.md) and [provenance](../../docs/waveshare/PROVENANCE.md). No new project-wide license is granted by this private historical import.
