> **Historical record — current-status notice (November 18, 2026):** The older material below preserves the development state at its original date. **RID-01 concluded at the R7 BLE/GNSS prototype on November 17, 2026.** The external LC76G GNSS integration was completed; Wi-Fi Remote ID and microSD logging were **not implemented**. Outdoor performance did not satisfy the intended field use, so development on this platform was stopped. For current information use [R7 status](waveshare/STATUS.md), [bounded validation](waveshare/R7_VALIDATION.md) and [final engineering decision](waveshare/R7_FINAL_ASSESSMENT.md). Earlier statements that GNSS is pending, BAT-D4E-3B is the current release, or Wi-Fi/SD will be developed next are **superseded**. Historical evidence claims retain their original limited scope.

# Local development and repository workflow

**November 6, 2026 current-state replacement proposal.** This supersedes the prior guide's use of the plain D4H working file as the newest source.

## Identify the baseline explicitly

Current provisional reference: **BAT-D4E-3B**, SHA-256 `2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670`. Accepted rollback: **BAT-D4D-R2**, SHA-256 `a485eadad7af3ac706db9e10e053e492bd48b7eb61460f17374c989bf00d2d0e`. The plain Kali `RID_UI_Migration.ino` still hashes to historical D4H (`7bdb2be6…`). Importing a copy here must not overwrite any of those Kali files or flash a board.

The source [manifest](../firmware/waveshare/SOURCE_MANIFEST.json) maps repository copies to archived filenames. Source identity is not installed-binary identity. Existing device functionality is supported by the operator's stage-specific tests, not by a Git commit.

## Development gate

Inspect → isolate → bounded change → review diff → compile → physical acceptance when required → freeze → record evidence → approved repository update. Do not patch the accepted archive. Do not silently turn a synthetic visual or fault-injection build into a production reference.

## Fresh compile for this historical import

Use [BUILD.md](waveshare/BUILD.md) and the supplied compile-only helper. It creates a new directory, verifies the current freeze plus rollback/working-source guards, copies hash-pinned dependency inputs, compiles with the recorded board options and checks input hashes afterward. It neither uploads nor requires a board connection. A separate explicit approval is required for a repository write or device flash.

**Current gate:** the November 6 isolated Kali compilation of BAT-D4E-3B passed and its returned evidence was reconciled. See the [build review](waveshare/records/CLEAN-COMPILE-REVIEW-2026-11-06.md). This is separate from the host-only test; it does not repeat historical-stage hardware acceptance. Final owner review and explicit push approval remain pending.

## Records required for future milestones

Record stage/date, source hash, dependency/configuration changes, compiler command/result, binary hashes, device/test setup, pass/fail criteria, raw local observations, privacy-clean sharing evidence, limitations and rollback. Keep firmware test dates separate from documentation/import dates.

For GNSS, start a new isolated candidate only after the pre-GNSS snapshot is accounted for. Preserve stale/no-fix behavior and receiver-position provenance. Freeze BLE + battery + GNSS before a separate Wi-Fi branch. A failed combined-radio experiment must leave a recoverable BLE-only reference.

## Existing historical records

Existing CYD1 and October handoff documents remain history. Where an older document says D4H is current or predicts RESET-to-HOME, use the newer [status reconciliation](waveshare/STATUS.md) and [validation matrix](waveshare/VALIDATION.md). Do not erase the earlier record to hide a corrected observation.
