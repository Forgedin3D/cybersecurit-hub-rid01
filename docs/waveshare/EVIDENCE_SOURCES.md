> **Historical record — current-status notice (November 18, 2026):** The older material below preserves the development state at its original date. **RID-01 concluded at the R7 BLE/GNSS prototype on November 17, 2026.** The external LC76G GNSS integration was completed; Wi-Fi Remote ID and microSD logging were **not implemented**. Outdoor performance did not satisfy the intended field use, so development on this platform was stopped. For current information use [R7 status](STATUS.md), [bounded validation](R7_VALIDATION.md) and [final engineering decision](R7_FINAL_ASSESSMENT.md). Earlier statements that GNSS is pending, BAT-D4E-3B is the current release, or Wi-Fi/SD will be developed next are **superseded**. Historical evidence claims retain their original limited scope.

# Evidence source register

Original archives/documents remain private. This register identifies inputs by hash without exporting private metadata. References in the narrative are bounded by the evidence descriptions below.

| ID | Input retained privately | SHA-256 | Use / limitation |
|---|---|---|---|
| S1 | `RID01_WAVESHARE_PRIVATE_AUDIT_20261105T153258175264Z.zip` | `2f3b309d5bd02cb285d5b05f12b621cef9656b9e48444735ea22c2d32696bc78` | 678 selected original source/dependency/report artifacts. Direct source/manifest identity and local report evidence; not all tests repeated. |
| S2 | `CyberSecurit_Hub_RID01_Waveshare_RADAR_Engineering_Report(1).docx` | `41f09f223f48e574cc38f5ec467c0980e064a91dda9a885a70860ab38623ebf2` | Retrospective November 5 report through Oct 21 with continuity through Oct 29. Reconstructed diagrams are not original photographs. Earlier RESET statement requires later correction. |
| S3 | `CyberSecurit_Hub_RID01_Handoff_2026-10-29 (2).md` | `742bc63d0dc7397ba1277a592b1f6d99fff5f6207a7a603c65ea9700ba0715a0` | Older D4F–D4H status and build workflow. D4H-current and reset-retention expectations are historical, not current guarantees. |
| S4 | `CyberSecurit_Hub_RID01_Engineering_Handoff_2026-11-03.md` | `0a4dae2df01e8795535d1c71ce36cee1eb7f5e309c2351a98d7580505d57ff85` | Battery/R2 endurance and SOC observation record; later 3B qualifications come from S1 acceptance file and subsequent operator results. |
| M1 | `iCloud Photos.zip` | `b1a0064f8d6564271ff3ae15bb9477c01a48bb0eb1b0cbfd64a9098db3f78980` | 25 original RADAR photos + two clips; reviewed source, no raw originals imported. |
| M2 | `iCloud Photos (1).zip` | `5f578cee8ee5167cac4d3896e3f3dd32ead89f672103639f85cc6475364adfc7` | 31 original historical UI photos + one clip; no raw originals imported. |
| M3 | `iCloud Photos (2).zip` | `f71d3dbdea9a65ed2693c9c4a0ff6b55ed7870e0985e34b0efe2410e904331c7` | 12 operator-described new Nov 6 bench photos; no raw originals imported. |
| M4 | `RID01_Waveshare_RADAR_Media_Staging_2026-11-06.zip` | `9f27919b44ac6a209c2e39e5a6986ce2f776c77c4e903b94a9ec5a095cfdeb89` | Earlier sanitized derivatives and media review; selected entries reverified for this proposal. |
| M5 | `RID01_Waveshare_UI_Battery_Media_Staging_2026-11-06.zip` | `3081b1b9b72db91dd00744d433f9ac967a5e6d6cd3d5ac90bc8328251c876dd4` | Earlier sanitized UI/battery derivatives and review; selected entries reverified for this proposal. |

## Claim-to-evidence map

- Source roles and current/rollback identity: S1, imported SOURCE_MANIFEST and named acceptance records.
- Display/touch/migration narrative: S1 checkpoints and library diffs, S2 retrospective chronology, M1/M2 actual device images.
- Synthetic versus live RADAR: S1 injected fixture/production differences; S2 section 10; M1 visual sequence; M3 plus the operator's explicit R111S/three SYSTEM-beacon clarification.
- Reset correction: both original D4H and later BAT-D4C acceptance notes copied from S1; do not silently discard the earlier conflicting observation.
- Battery endurance and SOC discrepancy: S4 and the operator's timelines in the conversation; no new full-charge or capacity evidence.
- Provisional 3B scope: S1 original November 4 provisional acceptance report. Reported counts are not newly re-parsed raw logs in this staging pass.
- New November 6 warning logic result: tools/test_waveshare_voltage_warning.py and records/HOST-WARNING-LOGIC-2026-11-06.txt; host-only.
- Fresh isolated ESP32 compile: November 6 operator-supplied `BUILD_EVIDENCE.zip`, SHA-256 `1bc81836e20d9063e2929089d73f21d1610df4ce26e3b56886ccf92eadc87066`. Seven returned sharing records are retained under `records/build-2026-11-06/`; see [review](records/CLEAN-COMPILE-REVIEW-2026-11-06.md). Recorded compile exit 0; source hash matches the staged reference; no binary bytes or new hardware test were supplied. Earlier mocked-helper output is not used as build evidence.
- Media privacy transformations: M4/M5 and imported MEDIA_MANIFEST, with final owner review still pending.
