# Provenance, attribution, and evidence integrity

This private repository was created on **2026-10-29**, after RID-01 had already passed multiple offline development milestones. Its commit chronology must not be mistaken for the dates of the original firmware tests.

## Historical project evidence

Documented sources include:
- CyberSecurit Hub conversations: **“Review remote ID Setup”**, **“Execute Previous prompt”**, and the continuing RID-01 / GitHub project thread.
- **CYD1 RID Reference Firmware — Full Technical Record v1.0** (accepted reference-platform record, dated 2026-10-12).
- **CyberSecurit Hub RID-01 Chat Handoff**, 2026-10-21 (D4E WaveShare baseline, test behavior, RF/privacy constraints).
- Operator-reported Waveshare D4F, D4G, D4H run results and SHA-256 source-integrity logs through 2026-10-29.
- Operator-provided D4H screen image and serial log from physical Waveshare testing.

The above historical documents and source artifacts have **not been imported wholesale** into this repository. They may include information that requires privacy review. Descriptions and acceptance status in this repository are based on those records; no claimed tests were performed by GitHub itself.

## CYD1 historical import — November 4, 2026

The private repository now contains the SHA-verified original CYD1 Stage 4B-B2C accepted sketch (SHA-256 `a76f5a9cf94899af528b5bcdace01e2bb7207ed3ecf3f4690f8d63ee9516525c`), selected intermediate source stages and nine metadata-cleaned photographic derivatives. The original CYD1 engineering report and two October 12 serial logs were used for [documented validation](validation/CYD1.md); other tests are explicitly identified as report-only. This is a November **reconstruction import**, not original October Git commit history.

Original unredacted photos, their GPS EXIF, untouched logs, and device factory firmware backups remain private and are not published. The imported source was AI-assisted, with real equipment, tests and acceptance directed by the project operator. Dependencies are named and linked, not vendored. No public license or certification is implied.

## Who did what

- **Project owner/operator:** Hardware assembly, purchasing/selection, controlled tests, observation, physical acceptance, release/freeze decisions, and local command execution.
- **Assistant-supported work:** Engineering planning, test planning, code-patch proposals, review, diagnostic assistance, documentation drafting, GitHub repository organization.
- **Third parties:** Espressif ESP32/Arduino toolchain and libraries, Waveshare hardware and display components, Remote ID standards/vendor firmware, and any source code/libraries explicitly imported later.

Do not describe AI-generated patch assistance or third-party software as independently authored human code. Future license/attribution review must precede publication of full firmware and derived components.

## Evidence categories

- **Accepted:** Named frozen source with recorded hash and completed acceptance tests.
- **Operator-reported test:** Test output/log/photo supplied from a real test; no independent access to the board unless explicitly stated.
- **Planned:** Hardware or feature not yet implemented/validated (Wi-Fi Beacon and onboard LC76G GNSS; battery safety qualification remains incomplete despite functional battery operation).
- **Research:** Technical feasibility hypothesis only; not proof of operation.

## Public sharing

Repository is currently **private**. Before any public mirror/portfolio release, audit code, history, screenshots, telemetry, locations, BLE addresses, aircraft registration, personal details, and third-party licenses. See [SECURITY.md](SECURITY.md).
