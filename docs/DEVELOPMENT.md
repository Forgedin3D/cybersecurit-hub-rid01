# Local development and repository workflow

## Source of truth

Accepted Waveshare source currently resides **only on the operator's Kali VM**:
`~/Waveshare_RID_S3/RID_UI_Migration/RID_UI_Migration.ino`.

**Accepted D4H hash:** `7bdb2be698c63aba895e5b892f29478f8e215807126eb155bac9abd8fcabbfcf` (2026-10-29). Match the working and frozen acceptance copy before doing further work. See [BASELINES.md](BASELINES.md).

GitHub currently contains project documentation and **not the compiled/accepted `.ino`**, so do not build from this repository until the source is deliberately imported and dependencies are recorded.

## Toolchain

- Kali Linux VM; Arduino CLI
- `esp32:esp32` core `3.3.11`
- Waveshare ESP32-S3 Rev2.0, /dev/ttyACM0 when enumerated (not a portable fixed device name)
- Local libraries under `~/Waveshare_RID_S3/compat_libraries`
- Required compiled board options below (hardware acceptance environment)

```bash
FQBN='esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CPUFreq=240'

arduino-cli compile \
  --libraries "$HOME/Waveshare_RID_S3/compat_libraries" \
  --fqbn "$FQBN" \
  "$HOME/Waveshare_RID_S3/RID_UI_Migration"
```

An upload must use the accepted candidate and verified build output, with explicit board options plus `UploadSpeed=115200,UploadMode=default,EraseFlash=none`. Perform live acceptance after flashing. **Do not use this document as an instruction to flash unreviewed development code**.

## Recommended branch and milestone method

- `main`: documentation of current accepted status and eventual audited source only.
- `feature/...`: isolated development and discussion.
- Do not alter the accepted frozen baseline to fix a speculative issue.
- Record named stage, source hash, diff, compile result, upload target, hardware outcome, privacy review and rollback path.
- Commit honest documentation even before source import; historic acceptance stages precede repository creation.

## Future audited source import

1. On Kali, independently confirm D4H working source SHA-256 against [BASELINES](BASELINES.md).
2. Preserve frozen source/rollbacks in an offline backup.
3. Audit the **entire source tree**, comments, fixtures and local logs for GPS coordinates, Remote ID serials, registration, MAC addresses, credentials and vendor license obligations.
4. Decide whether source stays private, which third-party files can legally be committed, and which files belong in a public case-study-only mirror.
5. Import a vetted source copy via a distinct reviewable change; create a baseline tag **only after** checking the hosted file hash. Do not force-push or rewrite history casually.
6. Recompile from repository only after dependencies and pinning are repeatable.

## External documentation

Original conversations and long-form handoffs remain external historical evidence rather than GitHub-attached artifacts. [PROVENANCE](PROVENANCE.md) explains which evidence this repository's descriptions were based on.
