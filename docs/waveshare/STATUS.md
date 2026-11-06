# Waveshare pre-GNSS current state

**Prepared November 6, 2026 — review candidate, no push or new hardware test.**

## Source roles

| Role | Stage | SHA-256 |
|---|---|---|
| Current provisionally accepted frozen reference | BAT-D4E-3B, November 4 | `2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670` |
| Accepted rollback | BAT-D4D-R2, October 31 | `a485eadad7af3ac706db9e10e053e492bd48b7eb61460f17374c989bf00d2d0e` |
| Plain Kali working sketch / older freeze | D4H, October 29 | `7bdb2be698c63aba895e5b892f29478f8e215807126eb155bac9abd8fcabbfcf` |

The archive and candidate copies of BAT-D4E-3B match. None of these source hashes independently identifies the currently flashed binary. The operator previously reported uploading/testing 3B; the November 6 photographs are a later demonstration, not a hash attestation.

## Working scope

BLE receiver and four-slot target engine; landscape capacitive-touch UI; PRIVATE/OPEN and snapshots; coordinate-derived north-up RADAR; local RAM-only FIXED receiver reference; independent lifecycle/geographic freshness; STOP/HOME hardening; acknowledgment notice; battery voltage/power-state/estimated-percentage header; provisional LOW BATTERY operator warning.

**GNSS:** LC76G integration pending. Delivery progress is not technical acceptance. **Wi-Fi:** not implemented here. **Storage/enclosure:** do not infer completed SD logging or enclosure work from this package.

## Exact qualifications

The 45-minute mixed-use LiPo run belongs to BAT-D4D-R2. The subsequent 15-minute healthy-voltage and two R111S captures belong to the provisional 3B record. Synthetic geometry and simulated low-battery display tests remain labeled synthetic. No calibrated SOC/capacity, guaranteed runtime, physical low-cell alert threshold, actual charge termination, direct cell-temperature monitoring, outdoor accuracy/range or absolute packet-loss result is claimed.

The physical RESET button re-required acknowledgment in the later instrumented testing, including battery-only operation. An older October 29 statement reporting return HOME is superseded for that physical-reset case by the October 30 BAT-D4C record. Software-only reset behavior is not generalized from it.

## Gates for this proposed import

| Gate | State |
|---|---|
| Source archive and selected-byte identity | Verified |
| Curated documentation/source/media assembled | Staged |
| Selected source privacy and notice/provenance checks | Scoped review completed; not a comprehensive security/legal certification |
| Host warning logic test | 23/23 assertions PASS; not ESP32/hardware acceptance |
| Fresh isolated ESP32 compile | **PASS — November 6 Kali evidence reviewed; no flash/retest** |
| Final operator media/package approval | **PENDING** |
| GitHub commit/push, Issue or Project edits | **Not performed** |
| Public release / repository visibility change | **Not authorized** |

No shipping pressure or module arrival should silently turn a pending gate into PASS. See [validation](VALIDATION.md), [build](BUILD.md), and [roadmap](ROADMAP.md).

[Evidence source register](EVIDENCE_SOURCES.md).
