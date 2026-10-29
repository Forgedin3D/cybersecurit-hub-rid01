# Firmware baseline and acceptance register

**Record status:** Based on user-reported command outputs, verified SHA-256 comparisons performed on the Kali VM, engineering handoffs, and physical test reports through 2026-10-29. These records are **not a substitute for possessing the binary/source artifact in GitHub**.

| Stage | Date | Scope | SHA-256 | Status |
| --- | --- | --- | --- | --- |
| CYD1 Stage 4B-B2C | 2026-10-12 | Earlier ESP32-2432S028R functional receiver reference | Not entered here; consult CYD1 full technical record | Accepted reference platform |
| STAGE1B-D4E | 2026-10-21 | Waveshare RADAR lifecycle visibility | `dee2136cdac5b58d438d4ddaa068816d263e4df090638ecffcbd59dc46117860` | Accepted historical snapshot |
| STAGE1B-D4F | 2026-10-23 | RADAR right-panel ACTIVE/STALE labels | `166f7f32ca92de4e2858b9e490873c132a59310a5c88478a54df180a3ba64e67` | Accepted historical snapshot |
| STAGE1B-D4G | 2026-10-23 | STOP/HOME hardening | `3a28f08f51f42046e673d69a81073c95fdb63629ef35e3d5540d04067cdc415b` | Accepted rollback |
| **STAGE1B-D4H** | **2026-10-29** | Cold-boot operator acknowledgment + RTC warm-reset gate | **`7bdb2be698c63aba895e5b892f29478f8e215807126eb155bac9abd8fcabbfcf`** | **Current accepted working source and frozen snapshot** |

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

## D4H acceptance scope

Five operator-performed tests **PASS**: (A) cold startup notice; (B) RESET before acknowledgment retains gate; (C) RESET after acknowledgment returns directly HOME; (D) START and STOP/HOME still work; (E) second cold startup shows notice again. Legible screen and touch-release operation corroborated by photograph and diagnostic log. Build clean-compiled using ESP32 core 3.3.11. See [VALIDATION.md](VALIDATION.md).

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
