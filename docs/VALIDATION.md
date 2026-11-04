# RID-01 validation register

**Updated through 2026-11-04.** Results are derived from operator-provided serial logs, photographs, acceptance commands, and earlier handoff records. Do not extrapolate beyond tested scope.

## Battery engineering acceptance and diagnostics (2026-10-30 through 2026-11-04)

**BAT-D4B/C/D-R2:** Operator reports accepted/immutable frozen SHA-256 source archives; battery-voltage header and provisional estimated SOC displayed on HOME, SELECT OUTPUT MODE, TARGETS and RADAR; battery excluded from NOTICE and SNAPSHOT. Source hashes and accepted scope are listed in [BASELINES.md](BASELINES.md). Electrical polarity and USB-only / LiPo-only / LiPo+USB functional power tests were operator validated; startup warning discloses that the two-wire pack does not supply direct cell-temperature sensing.

**BAT-D4E-1 supervised 45-minute LiPo-only endurance (2026-11-03):** Operator stopped/modified the initial test *preparation* when an unexpected charge-gauge transition occurred, then completed the planned official test after a separate supervised 35-minute charging session and rest. The reported official endurance chronology:

| Elapsed (min) | Mode | Observed start → finish voltage | Observed estimated SOC |
| --- | --- | --- | --- |
| 0–10 | HOME | 3.88V → 3.85V | 82% → 77% |
| 10–20 | TARGETS, R111S | 3.83V → 3.82V (occasional 3.81V) | 75% → 72%; OPEN selected at 15min |
| 20–30 | RADAR, R111S | 3.82V → 3.80V (some 3.81V) | 72% → 68% |
| 30–35 | TARGETS/RADAR switching | 3.80V / 3.79V | 68% → 66%; PRIVATE selected at 31min |
| 35–45 | STOP/HOME | 3.81V → 3.80V | 66% → 64%; R111S OFF at 40min |

Operator reports UI functioning, no visibly lost R111S reception, and approximately 9,000 packet counter units (+/-), with no raw per-packet evidence to guarantee zero dropped RID packets. No abnormal battery concerns reported during supervised **pre-test charge**; independent end-of-endurance pouch inspection not separately recorded. Operator completed full 45min without reaching conservative experiment stop limits (3.65V sustained loaded or provisional EST35%). **PASS within the mixed-use BLE-only endurance scope; not calibrated battery capacity, full-load duration or GNSS/Wi-Fi runtime.**

**Separate pre-test fuel-gauge anomaly:** After >24h disconnected, short battery-only boot 3.86V EST75%. Connecting USB-C/Kali while LiPo was attached produced reported EST ~30% and voltage toward 4.05V. Operator halted preparation and then supervised USB+LiPo charge for ~35min until EST~76% and 4.07V, cell remained cool to slightly warm; after USB removal 3.89V EST76%. Following ~1h board off with battery attached and an additional brief USB/Kali manual-reference setup, official battery-only start read 3.88V EST82%. **No claim that 30%, 76%, or 82% represents true battery capacity.**

**BAT-D4E-2/2B audit (2026-11-04):** SHA-guarded accepted source audit found no application PMIC charger-register configuration writes; existing `Wire.write(reg)` only selects the repeated-start read register pointer. Isolated BAT-D4E-2B diagnostic cleanly compiled (691,407 bytes sketch; 31,660 bytes globals), SHA-256 `7003f7fd7febe5ac605cc8d8b0e3d2a097f3ea8ec68bb131bb82f485694c41c8`. 120sec USB-only read-only live snapshots consistent, `USB=1 BAT=ABSENT MV=NA SOC=NA PHASE=5`, all 15 registers identical with no ERR:

```text
00=20 01=15 18=0A 50=12 58=00 61=05 62=08 63=15
64=03 65=02 67=E6 68=01 69=05 A1=ED A2=00
```

Charger programmed 200mA CC / 4.20V CV, precharge/termination/timer values stable vs prior USB-only readings; **no physical battery attached**, therefore charging voltage/current behavior with the battery was not directly measured in this test.

**Firmware restore (2026-11-04):** Isolated restore from accepted BAT-D4D-R2 archive clean compiled (691,167 bytes sketch/31,652 globals), esptool flash data hashes verified, accepted source and diagnostic source unchanged; physical R2 reported flashed. Separate post-restore UI check pending.

**Open safety and QA:** SOC estimation anomaly, cell-temperature absence, charger termination and actual charge current, conservative voltage-based low-battery warning (not yet implemented), full-endurance curve, battery post-test physical inspection, and changes in load following GNSS/Wi-Fi integration. No deliberate deep discharge or unattended charge as qualification.

## Waveshare D4H-B startup acknowledgment — ACCEPTED

Board: Waveshare ESP32-S3-Touch-LCD-3.5 Rev2.0. Accepted source hash: `7bdb2be698c63aba895e5b892f29478f8e215807126eb155bac9abd8fcabbfcf`.

| ID | Hardware test performed by operator | Outcome |
| --- | --- | --- |
| A | Remove all power, restore power: notice displayed | PASS |
| B | RESET *before* acknowledgment: notice remains required | PASS |
| C | RESET *after* acknowledgment: HOME, without repeated notice | PASS |
| D | BEGIN -> START -> STOP/HOME, without notice reappearing | PASS |
| E | Second full cold power-on: notice appears again | PASS |

Additional D4H-A screen evidence: legible warning and button; touch serial log included DOWN/UP ACKNOWLEDGE, START, T01–T04, STOP. One short scan reported 676 BLE advertisement callbacks, zero validated RID packets, zero target allocations and a clean STOP after 7,199 ms. **This short no-target scan is not a live-aircraft decode test.**

D4H-B clean compilation: **PASS**, Arduino ESP32 core 3.3.11; sketch 689,543 bytes (21% of selected partition maximum), globals 31,588 bytes (9% of RAM report). D4G source protected and frozen.

**RESET evidence correction (2026-10-30):** D4H historical entry C records post-ACK physical RESET -> HOME, but later instrumented RESET observed an ESP_RST_POWERON-like reset and acknowledgment reappeared, meaning physical RESET handling is **not established to behave consistently as earlier described**. Keep both data points; retest with controlled reset reason before product guarantees.

## Waveshare prior functionality — previously accepted

- D4E radar lifecycle field validation with real **Ruko R111S** and Beacon #1.
- D4F colored ACTIVE (green) / STALE (yellow) right-panel labels, LOST hidden.
- D4G STOP/HOME hardening including intentional failed-STOP injection, restoration of tested production firmware, and successful regression.
- 2026-10-27 UI/operator regression: mode switch PRIVATE/OPEN and redacted SNAPSHOT; four target buttons; RESCAN; scan-continuing TARGETS/RADAR navigation; STOP from RADAR; coexistence of R111S and synthetic beacon.
- Geographic fidelity: fresh target + valid receiver reference required, north-up coordinates rather than RF radar, no stale ghost plots.

## CYD1 earlier reference platform (different hardware, not D4H)

Earlier standalone ESP32-2432S028R functional acceptance documented 2026-10-12 includes: 12-minute two-target endurance (13 windows, 71,427 BLE callbacks, 9,438 validated RID packets), five-minute no-target rejection (25,904 ambient BLE callbacks and zero validated RID), and four-target lifecycle/reacquisition (124.920 sec, three windows, 11,540 BLE callbacks, 1,986 validated RID packets, zero allocation failures). **These statistics belong to CYD1 and must not be presented as Waveshare D4H results.**

## BAT-D4E-3D real-aircraft BLE validation (2026-11-04)

Two operator-run, redacted serial monitor captures on **BAT-D4E-3B TEST_ONLY**, real Ruko R111S, USB-only Waveshare (LiPo disconnected):

| Evidence | Test 1: TARGETS focused | Test 2: RADAR focused |
| --- | --- | --- |
| Approximate capture duration | 2m39s (timer field prefix truncated) | 153,802ms (~2m34s) |
| Continuous BLE scan windows | 3 | 3 |
| Validated cooperative RID packets | **1,012** | **961** (type-total cross-check) |
| Packet types: BASIC / LOCATION / SYSTEM / OPERATOR | 472 / 239 / 64 / 237 | 450 / 206 / 78 / 227 |
| R111S T01 status | ACTIVE; reported VALID position | ACTIVE; reported VALID position |
| RSSI among displayed target summaries | −60 to −52 dBm | −89 to −36 dBm (operator moved aircraft indoors) |
| Valid receiver reference | Not entered | FIXED/VALID manual reference |
| RADAR geographic readiness | Not evaluated | READY → ACQUIRING → READY |
| UI operations | T01–T04, PRIVATE SNAPSHOT/RETURN, STOP/HOME | TARGETS/RADAR tabs, PRIVATE SNAPSHOT/RETURN, STOP/HOME |
| Battery telemetry | USB=1 BAT=ABSENT consistently | USB=1 BAT=ABSENT consistently |

**PASS** for real BLE decode, target allocation, continuous scan rollover and normal UI/STOP/HOME in these captures. RADAR readiness was observed but plot position, geographic freshness loss root cause and physical movement accuracy were not independently validated. R111S reported alternating GROUND/AIRBORNE flags while handled indoors; these are **broadcast values, not actual airborne proof**. Neither test switched the R111S off long enough to demonstrate STALE→LOST removal. No false low-battery indication with absent LiPo.

Serial-output anomalies at the STOP footer: Test 1 final `Targets Tracked: 10` contradicts all repeated `Targets Tracked: 1`; Test 2's advertisement callback count line is concatenated with another field. Treat these footers as partially corrupted, not proof of phantom targets or receiver defects. Logs preserved locally; avoid raw identifiers and coordinates in repo.

Combined message-type totals across both captures support **1,973 validated RID packets**, *not* a zero packet-loss guarantee. BAT-D4E-3B remains TEST_ONLY pending accepted-source freeze; actual voltage threshold activation, full-charge termination and GNSS/WiFi combined use remain outside these tests.

## Open / pending validation

- Further quantified packet/loss and continuous-full-load R111S regression; mixed-mode 45min endurance on R2 passed, not a full battery depletion/lifetime measurement.
- Follow through with low-voltage warning and charge-termination validation, actual on-battery charging current, thermal/pouch checks, and gauge accuracy. Polarity, battery-only operation, initial supervised charging and USB-only charger configuration already evaluated.
- Integrate LC76G GNSS, test cold/warm fixes, stale/invalid reference handling and scan-only operation without fix.
- Prototype 2.4GHz Wi-Fi Beacon Remote ID receiver; validate with DJI Mini 5 Pro only during appropriate lawful operation, and verify decoding rather than assuming transport from marketing.
- Assess BLE/Wi-Fi coexistence, channel schedule, packet loss and receiver responsiveness. Multi-transport V1 **not** yet accepted.
- Repeat STOP/HOME, PRIVATE/OPEN/SNAPSHOT, lifecycle and reset checks after GNSS/Wi-Fi changes.
- Distinguish RSSI reception strength from physical geographic range. Characterize actual range with measured separation and repeatable conditions.
- An earlier intermittent truncated serial diagnostic / target count inconsistency remains a low-severity unresolved QA observation.

## Test hygiene

Preserve raw logs **locally**, not in this repository; publish only sanitized summaries, redacted screenshots, and controlled non-identifying test artifacts. Log date, board revision, source hash, firmware, transmitter, environmental constraints, pass/fail and actual evidence for every future test.
