# CYD1 archived firmware — original RID-01 reference implementation

This directory contains a **historical reconstruction import**, not a newly rewritten firmware release. The accepted source bytes are identical to the operator's frozen **October 12, 2026 Stage 4B-B2C** archive. Source files were first committed **November 4, 2026**; the historical date is test provenance, not Git commit history.

## Recommended entry point

[`reference/CYD1_RID_Handheld/CYD1_RID_Handheld.ino`](reference/CYD1_RID_Handheld/CYD1_RID_Handheld.ino) — SHA-256 `a76f5a9cf94899af528b5bcdace01e2bb7207ed3ecf3f4690f8d63ee9516525c`.

Historic build environment as recorded in the acceptance report:

- Board: **ESP32-2432S028R / classic CYD1** (`esp32:esp32:esp32`)
- Espressif Arduino ESP32 core **3.3.11**
- **TFT_eSPI 2.5.43** (requires matching board User_Setup configuration)
- **XPT2046_Touchscreen 1.4**
- Other included headers are standard Espressif Arduino/ESP-IDF/FreeRTOS components.

For an independently prepared environment, the historic command pattern is:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/cyd1/reference/CYD1_RID_Handheld
```

This is a **documented historical build command, not a claim of a fresh 2026-11-04 reproducible-build PASS**. Compilation depends on the matching core, libraries, TFT_eSPI board configuration and toolchain. No hardware upload is required for this portfolio import; the original CYD1 board and accepted local files should remain undisturbed.

## Selective historical snapshots

- `history/stage7c-backend.ino` — complete multi-target BLE parser/backend before integration.
- `history/stage4b-a-multitarget-ui.ino` — separately validated touch UI before live-backend integration. The UI employed demonstration/synthetic data; do not claim it was receiving actual BLE broadcasts at this stage.
- `history/stage4b-b2a-compilefail.ino` — uncorrected integrated source, preserved to demonstrate actual failure.
- `history/stage4b-b2a-compilepass.ino` — corrected integrated source.
- `history/patch_stage4bb2a_fix1.py` — historical writer script for the prototype-ordering correction; **never execute against the accepted source**.

These historical `.ino` snapshots are not arranged as independent buildable Arduino sketches; a correctly named sketch directory and appropriate dependency configuration are required before compiling one in isolation. The final reference sketch is organized as a conventional Arduino sketch folder above.

## Provenance and third parties

Project operator conducted hardware tests, executed commands, preserved source hashes and made stage acceptance decisions. Code/patched versions were developed with AI assistant support. The firmware **calls** libraries from Espressif, TFT_eSPI and XPT2046_Touchscreen; their upstream implementations are **not vendored here**. This repository does not confer a license to those external libraries. Review upstream license files for the precise versions before redistribution or publishing commercial firmware binaries. No repository-wide open-source license is granted yet.

This receiver is passive cooperative Remote ID research. It is not a conventional RF radar or flight-safety instrument. For the selected acceptance tests and known limits, see [CYD1 validation](../../docs/validation/CYD1.md) and [history](../../docs/history/CYD1.md).
