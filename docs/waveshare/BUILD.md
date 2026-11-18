> **Historical record — current-status notice (November 18, 2026):** The older material below preserves the development state at its original date. **RID-01 concluded at the R7 BLE/GNSS prototype on November 17, 2026.** The external LC76G GNSS integration was completed; Wi-Fi Remote ID and microSD logging were **not implemented**. Outdoor performance did not satisfy the intended field use, so development on this platform was stopped. For current information use [R7 status](STATUS.md), [bounded validation](R7_VALIDATION.md) and [final engineering decision](R7_FINAL_ASSESSMENT.md). Earlier statements that GNSS is pending, BAT-D4E-3B is the current release, or Wi-Fi/SD will be developed next are **superseded**. Historical evidence claims retain their original limited scope.

# Build record and isolated pre-GNSS verification

**This is a November 6 compile record for older BAT-D4E-3B code, not R7 build instructions.** See the [R7 source/build record](R7_BUILD.md).

## November 6 clean compile — PASS

The operator ran the supplied compile-only helper in the existing Kali environment. Returned evidence confirms a successful BAT-D4E-3B compilation with Arduino CLI **1.5.1** and Espressif Arduino core **3.3.11**. The chat review independently reconciled the evidence ZIP, source identity, command, dependency lock and resolved board settings; it did not run the ESP32 compiler itself. [Full build review](records/CLEAN-COMPILE-REVIEW-2026-11-06.md).

Recorded result: **691,519 program bytes**, **31,668 global/static bytes**, exit code **0**; the helper reports original guarded inputs unchanged. Warnings were suppressed (`-w`), so no warning-clean claim is made. No flash, runtime test, bit-identical binary comparison or hermetic clean-machine build is implied.

## Recorded configuration to verify

```text
esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CPUFreq=240
```

These explicit options come from the historical report/build guide. A D4H-A verbose log supplied fewer explicit options; do not pretend it independently locked every default. The returned November 6 command and board-details records now preserve the requested options and compiler-resolved environment for this current source.

Dependencies identified in the archived `library.properties`: Arduino_GFX 1.5.5 (local compatibility copy), SensorLib 0.3.1 and TCA9554 0.1.2, plus core Wire/SPI/BLE and ESP-IDF/FreeRTOS platform components. [DEPENDENCY_LOCK.json](../../firmware/waveshare/DEPENDENCY_LOCK.json) hashes 539 inventoried compatibility-library files; it is not a complete platform/toolchain archive. No wholesale third-party library or font collection is redistributed in this proposal. Retain the original private audit package locally.

## Compile-only helper

Run the downloaded helper inside Kali:

```bash
python3 "$HOME/Downloads/RID01_WAVESHARE_PRE_GNSS_CLEAN_COMPILE.py"
```

Or, from the staged repository copy, use `python3 tools/verify_pre_gnss_compile.py`.

The helper verifies three existing source guards and all 539 inventoried dependencies, creates a unique new home-directory workspace, copies the current source and pinned libraries, isolates user-library discovery, verifies installed CLI/core versions, resolves board details, and invokes `arduino-cli compile --clean` with a unique build/output directory. It checks original/copy hashes again afterward. It does not request package installation/update, GitHub/Drive actions, a serial monitor, or an upload. No board connection is necessary. The installed Arduino platform/compiler recipes run during compilation; this is not a hermetic clean-machine/toolchain audit.

The helper leaves compiled files local and returns a `BUILD_EVIDENCE.zip` containing logs, settings and artifact hashes—not firmware binaries. Home-directory paths are replaced in that sharing copy. Raw logs stay local. A changed source/dependency or version causes an abort; do not modify an accepted file to bypass a check.

**Returned evidence has been reviewed; final owner review and explicit push approval remain required.** No further compile is requested merely to approve this evidence. A compile PASS supports source/build compatibility for the recorded configuration, not hardware acceptance or bit-identical binary reproducibility. The imported source hash must remain `2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670`.

## References

Arduino CLI 1.5 compile reference: https://arduino.github.io/arduino-cli/1.5/commands/arduino-cli_compile/ (consulted November 6, 2026). The options `--clean`, `--fqbn`, `--libraries`, `--build-path` and `--output-dir` are documented there. `--upload` is deliberately not used.
