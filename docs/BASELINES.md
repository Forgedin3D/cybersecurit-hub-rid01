# Firmware baseline and acceptance register

**Record status:** Based on user-reported command outputs, verified SHA-256 comparisons performed on the Kali VM, engineering handoffs, and physical test reports through 2026-11-04. These records are **not a substitute for possessing the binary/source artifact in GitHub**.

| Stage | Date | Scope | SHA-256 | Status |
| --- | --- | --- | --- | --- |
| CYD1 Stage 4B-B2C | 2026-10-12 | Earlier ESP32-2432S028R functional receiver reference | Not entered here; consult CYD1 full technical record | Accepted reference platform |
| STAGE1B-D4E | 2026-10-21 | Waveshare RADAR lifecycle visibility | `dee2136cdac5b58d438d4ddaa068816d263e4df090638ecffcbd59dc46117860` | Accepted historical snapshot |
| STAGE1B-D4F | 2026-10-23 | RADAR right-panel ACTIVE/STALE labels | `166f7f32ca92de4e2858b9e490873c132a59310a5c88478a54df180a3ba64e67` | Accepted historical snapshot |
| STAGE1B-D4G | 2026-10-23 | STOP/HOME hardening | `3a28f08f51f42046e673d69a81073c95fdb63629ef35e3d5540d04067cdc415b` | Accepted rollback |
| **STAGE1B-D4H** | **2026-10-29** | Cold-boot operator acknowledgment + historical reset acceptance | **`7bdb2be698c63aba895e5b892f29478f8e215807126eb155bac9abd8fcabbfcf`** | **Accepted historical local working source and frozen snapshot; later RESET qualification applies** |
| BAT-D4B | 2026-10-30 | Voltage/source power header | `f26d860255f933de068c71b4ce8960187cbcad6ffa634ed051c8fd0277eba3ea` | Frozen accepted source |
| BAT-D4C | 2026-10-30 | Startup cell-temperature limitation and charging-safety notice | `5a55a5d9fe0b95ecf4943199793fbdba87016ca10d1fa5bf12ea138dfb8e314e` | Frozen accepted source |
| **BAT-D4D-R2** | **2026-10-31** | **Estimated SOC, measured voltage and power state across HOME/PRIVACY/LIVE views** | **`a485eadad7af3ac706db9e10e053e492bd48b7eb61460f17374c989bf00d2d0e`** | **Newest accepted/frozen source; physically flashed/restored on 2026-11-04** |

## Current local acceptance files (Kali VM)

```text
~/Waveshare_RID_S3/RID_UI_Migration/
  RID_UI_Migration.ino
  RID_UI_Migration.ino.STAGE1B-D4H-COLD-BOOT-ACK-ACCEPTED-2026-10-29
  RID_UI_Migration.ino.STAGE1B-D4G-STOP-HOME-HARDENING-ACCEPTED-2026-10-23
  STAGE1B-D4H-ACCEPTANCE-2026-10-29.txt
```

Operator-run freeze command on 2026-10-29 verified:
- Working `RID_UI_Migration.ino` hash == accepted D4H hash.
- D4H frozen snapshot hash == accepted D4H hash.
- D4G frozen rollback hash remained unchanged.
- No additional firmware upload was performed by the freeze step; physical D4H-B testing was completed prior to freeze.

## Battery milestones and separate test firmware (October 30–November 4)

**Filesystem nuance:** The Kali working `~/Waveshare_RID_S3/RID_UI_Migration/RID_UI_Migration.ino` remained the **D4H original source** through BAT-D4E-2B. The newest accepted R2 source is an **immutable named archive**, and the **physically installed device** was deliberately restored from that archive on 2026-11-04. Do not assume the working file, archive, and flashed image are the same until verified.

```text
~/Waveshare_RID_S3/RID_UI_Migration/
  RID_UI_Migration.ino.STAGE1B-BAT-D4B-POWER-HEADER-ACCEPTED-2026-10-30
  RID_UI_Migration.ino.STAGE1B-BAT-D4C-SAFETY-NOTICE-ACCEPTED-2026-10-30
  RID_UI_Migration.ino.STAGE1B-BAT-D4D-R2-EST-ALL-SCREENS-ACCEPTED-2026-10-31

~/Waveshare_RID_S3/RID01_BAT_D4D_R2_EST_ALL_SCREENS_TEST_ONLY/
  BAT-D4D-R2-ACCEPTANCE-2026-10-31.txt

~/Waveshare_RID_S3/RID01_BAT_D4E2_LIVE_CONFIG_TEST_ONLY/
  RID_UI_Migration/RID_UI_Migration.ino   # diagnostic, not production
  R2_RESTORE/RID_UI_Migration/RID_UI_Migration.ino  # accepted R2 copy
```

- BAT-D4D-R2 first physically tested USB-only, LiPo-only and USB+LiPo. Its accepted header uses **EST** explicitly to avoid overstating fuel-gauge accuracy. Clean compile: **691,167 bytes program / 31,652 bytes globals** (November 4 repeat).
- BAT-D4E-1 (**2026-11-03**): 45-min supervised LiPo-only mixed HOME + TARGETS/RADAR + STOP/HOME using R111S, operator-reported **PASS**, 3.88V / EST82% → 3.80V / EST64%; not a maximum-runtime or calibrated gauge test. Pre-test USB insertion showed anomalous estimated percent ~75% → 30%; charging/rest afterward gave ~76% → 82%. Root cause open.
- BAT-D4E-2B (**2026-11-04**) temporary USB-only read-only register audit candidate: SHA256 `7003f7fd7febe5ac605cc8d8b0e3d2a097f3ea8ec68bb131bb82f485694c41c8`. Operator reports approx. 120sec of identical 15-register snapshots, no read errors; AXP2101 `0x62=0x08` (200mA configured CC), `0x64=0x03` (4.20V), `0x63=0x15`, `0x67=0xE6`. No LiPo connected, no charger setting changes.
- **Restoration 2026-11-04:** accepted R2 source hash and diagnostic hash validated; esptool v5.3.1 flashed bootloader, partition table and application; flash hashes verified; `RID_UI_Migration.ino.bin` app write reported **691,312 bytes**, while sketch usage reported **691,167** (different size representations). Source hashes passed after upload; **post-restore touchscreen/UI functional regression still awaits separate user confirmation**.

**Interpretation:** Battery telemetry and 45-minute runtime are validated only for the recorded scope. Fuel-gauge calibration, low-voltage warning, actual charger-current under load, safe end-of-charge behavior, temperature sensing and expanded GNSS/Wi-Fi power impacts remain open. See [validation](VALIDATION.md) and [Issue #7](https://github.com/Forgedin3D/cybersecurit-hub-rid01/issues/7).

## D4H acceptance scope

Five operator-performed tests **PASS**: (A) cold startup notice; (B) RESET before acknowledgment retains gate; (C) RESET after acknowledgment returns directly HOME; (D) START and STOP/HOME still work; (E) second cold startup shows notice again. Legible screen and touch-release operation corroborated by photograph and diagnostic log. Build clean-compiled using ESP32 core 3.3.11. See [VALIDATION.md](VALIDATION.md).

**Dated correction (2026-10-30):** A later read-only reset diagnostic on this board observed a physical RESET classified as ESP_RST_POWERON and the operator acknowledgment **reappeared after RESET**, contrary to the earlier D4H acceptance table's post-ACK RESET->HOME claim. Both are preserved as historical observations; do not guarantee ACK retention across physical RESET until retested with controlled reset reason.

**Not implied:** full live R111S packet regression on D4H, battery charging qualification, GNSS, Wi-Fi Beacon decoding, combined-radio performance, product certification or legal sufficiency of acknowledgment.

## Engineering change control

1. Protect accepted working + frozen sources with full SHA-256 checks.
2. Stage **one named, isolated candidate**; do not overwrite accepted rollback.
3. Read-only inspect; review exact diff before compilation/upload.
4. Compile using pinned board options; upload only the intended candidate.
5. Test real hardware against stated pass/fail criteria.
6. Freeze source plus SHA-256 and acceptance report **only after** acceptance.
7. Do not misrepresent Git commits as evidence of historical commits: earlier milestones were developed offline, before this repository existed.

GitHub contributions begin with documentation on 2026-10-29. Local source has not yet been audited or imported into this repository.
