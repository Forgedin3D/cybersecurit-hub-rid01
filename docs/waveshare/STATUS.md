# Waveshare RID-01 — final portfolio status

**Updated November 17, 2026.** Current R7 firmware: `RX1024_RADAR_COLOR_R7`. **Project development closed on this board.**

| Item | State |
|---|---|
| Final source | [Exact R7 Arduino sketch](../../firmware/waveshare/final-r7/RID_UI_Migration/RID_UI_Migration.ino), SHA-256 `7874c62c8d94be5684311e2f76dbec70ca205bf4c24a3cea005c1b03f92ddaf2` |
| Previous pre-GNSS source | Historical `BAT-D4E-3B`, November 6 repository import; not final firmware |
| BLE/target engine | Four source slots demonstrated with two R111S units + two controlled ESP32 SYSTEM beacons |
| GNSS | External LC76G integrated; NMEA checksum/FRESH reference accepted under bounded tests |
| RADAR | R6 geometry refinement; R7 slot palette; two real geographic targets plotted in final validation |
| Python telemetry | Bounded, sanitized captures and source/artifact SHA-256 guards |
| LiPo | Bounded standalone functional operation; no calibrated gauge/endurance/safety qualification |
| Wi-Fi Remote ID | **Not implemented** |
| microSD logging | **Not implemented** |
| Outdoor field-readiness | **Rejected by operator** due to unreliable reception/target continuity near/beyond ~35 m in informal property trial; no calibrated cutoff measurement |
| Current disposition | Completed **prototype/case-study**; not a field product or certified safety tool |

The full living record v1.41 contains older recommendations to proceed with microSD and Wi-Fi; **those proposed next steps were superseded by the owner's November 17 final stop decision**. Do not mistake historical open tasks for a current backlog.

**Source of truth for accepted measurements:** [R7 validation](R7_VALIDATION.md). **For why development ended:** [Final assessment](R7_FINAL_ASSESSMENT.md). **For portfolio narrative:** [Case study](R7_CASE_STUDY.md).
