# RID-01 — Pre-GNSS clean-build evidence review

**Reviewed November 6, 2026. Result: PASS for the returned isolated Kali compilation.**
**Local staging only. Final owner media/package review and explicit push approval remain pending.**

## Evidence receipt and identity

`BUILD_EVIDENCE.zip` is **29,801 bytes**, with seven text/JSON/log entries totaling **821,152 uncompressed bytes**. ZIP CRC, unique names, safe paths and non-symlink entry checks passed. Its independently computed SHA-256 matches the operator's terminal output:

```text
1bc81836e20d9063e2929089d73f21d1610df4ce26e3b56886ccf92eadc87066
```

The records are `BUILD_RESULT.json`, `BUILD_SUMMARY.txt`, `cli_version.txt`, `core_list.txt`, `board_details.json`, `compile_command.json` and `compile_verbose.log`. The archive contains **no firmware binaries**. The two accompanying screenshots are consistent with the timestamped workspace and the evidence-file location; they are supporting context, not the basis of the compile verdict.

## Reconciliation with the proposed reference

The result identifies **BAT-D4E-3B**, source SHA-256 `2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670`. That value matches the independently hashed **205,457-byte** sketch in the staged package and the earlier uploaded audit archive. All nine selected sketch copies in the proposal continue to match their archived sources. The earlier inventory also preserves this named provisional freeze separately from the older plain D4H working sketch.

The exact helper supplied to the operator matches `tools/verify_pre_gnss_compile.py` in the staged package. Static inspection confirms that its dependency list matches all **539** records in `DEPENDENCY_LOCK.json`. The helper's returned result reports that its three original source guards and 539 original dependency records remained unchanged after compilation. This is operator-side recorded evidence; this chat did not directly access the Kali filesystem.

## Recorded build result

| Item | Returned evidence |
|---|---|
| CLI | Arduino CLI 1.5.1 |
| ESP32 platform | esp32:esp32 3.3.11 |
| Board | ESP32S3 Dev Module / esp32:esp32:esp32s3 |
| Compatibility graphics library | GFX Library for Arduino 1.5.5 |
| Touch/sensor library | SensorLib 0.3.1 |
| I/O expander library | TCA9554 0.1.2 |
| Core libraries | Wire, SPI, BLE 3.3.11 |
| Build invocation | compile --clean, unique build/output folders, isolated copied libraries |
| Recorded exit code | 0 |
| Program storage | 691,519 / 3,145,728 bytes (compiler reports 21%) |
| Globals/static allocation | 31,668 / 327,680 bytes (compiler reports 9%) |
| Remaining in that compiler accounting | 296,012 bytes; not a runtime heap measurement |
| Hardware upload/retest | Neither performed by this helper |

Requested FQBN, command record and selected board options agree:

```text
esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CPUFreq=240
```

The verbose log points to the isolated copies of the three compatibility libraries and the installed 3.3.11 core. The fresh build's reported program/global totals also match the earlier BAT-D4E-3B compile report in the project conversation. Equal totals do **not** establish binary equality.

## Build-output qualifications

**QIO versus DIO:** The requested menu option is QIO, while image-generation/flash-argument text says DIO. The supplied `board_details.json` itself resolves this: `build.boot=qio`, `build.memory_type=qio_opi`, and `menu.FlashMode.qio.build.flash_mode=dio`, with resolved `build.flash_mode=dio`. Thus the evidence records a consistent menu/recipe combination, not a reason to change firmware settings. This review does not infer a new hardware validation from those properties.

**Warning policy:** No compiler `error:` or `warning:` diagnostic lines were found, but the returned properties explicitly use `compiler.warning_flags=-w`. Record this as a successful compile with warnings suppressed, **not** a warning-clean static analysis result. A future warning-enabled review would be separate; no repeat run is requested for this evidence reconciliation.

**Local image generation is not an upload:** The log runs `elf2image` and `merge-bin` to produce local artifacts and displays “ready to flash.” The recorded command has no upload or serial-port flag, and the result explicitly records `hardware_upload=false`. That message does not mean the board was reflashed.

## Binary hashes: recorded, not independently rehashed here

The helper records six locally built artifacts, including the **691,664-byte application image**, bootloader, partition image, ELF, map and 16 MiB merged image. The reported application-image SHA-256 is:

```text
ab567bf74adf7ed05d7cd9e660657c3a2d1942f5496fe854756fdbee195d525c
```

The binary files were intentionally not transferred. Their hashes remain operator-side build records, not independently verified binary checksums in this review. A source hash and an image hash identify different objects. This single successful rebuild is **not** proof of deterministic bit-for-bit reproduction, installed firmware identity, or successful execution of the newly generated binary.

## Privacy and preservation

The seven supplied sharing records already replace the original home path and workspace with `<HOME>` and `<isolated-work>`. A bounded check found no literal user home/Windows-user paths, matching private-key/token/MAC patterns or receiver-coordinate commands. This pattern check is not a comprehensive security audit. No original photographed metadata or new RF captures were included.

The original evidence ZIP and initial review ZIP remain unchanged. This R1 revision changes documentation/checksum records only and adds the reviewed build evidence; all selected firmware, patch, helper and media bytes are unchanged. No GitHub, Drive, Project, Issue, visibility, firmware or hardware changes were made.

## Gate disposition

- **Fresh isolated Kali compile: PASS; evidence reconciled.**
- **Final owner review of exact file list, captions, video playback/reflections: pending.**
- **Explicit approval to push the private repository update: pending.**
- Public license decisions, actual low-cell warning/charge-termination qualification, calibrated SOC, receiver GNSS, Wi-Fi and combined radio performance remain separate open work.

Retain the entire timestamped Kali workspace, including `artifacts/`, for the local rollback/build record. No further Kali command or board reflash is needed to review this package. Before any later authorized push, reread the repository head and reconcile any concurrent edits rather than assuming the previously observed commit is still current.

## Exact input locations within the evidence archive

`BUILD_RESULT.json` supplies status, source identity, exit code, integrity flag and locally recorded artifact hashes. `compile_command.json` supplies the requested invocation. `board_details.json` supplies selected board options and resolved recipes. `compile_verbose.log` lines 495–503 list used libraries and memory results, and lines 512–520 repeat the version tables. The full seven original sharing records are retained unchanged under `docs/waveshare/records/build-2026-11-06/` in the revised proposal.
