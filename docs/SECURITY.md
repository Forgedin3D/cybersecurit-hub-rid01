> **Historical record — current-status notice (November 18, 2026):** The older material below preserves the development state at its original date. **RID-01 concluded at the R7 BLE/GNSS prototype on November 17, 2026.** The external LC76G GNSS integration was completed; Wi-Fi Remote ID and microSD logging were **not implemented**. Outdoor performance did not satisfy the intended field use, so development on this platform was stopped. For current information use [R7 status](waveshare/STATUS.md), [bounded validation](waveshare/R7_VALIDATION.md) and [final engineering decision](waveshare/R7_FINAL_ASSESSMENT.md). Earlier statements that GNSS is pending, BAT-D4E-3B is the current release, or Wi-Fi/SD will be developed next are **superseded**. Historical evidence claims retain their original limited scope.

> **Current-state reconciliation — November 6, 2026:** Read [Waveshare pre-GNSS status](waveshare/STATUS.md), [build procedure](waveshare/BUILD.md) and [validation](waveshare/VALIDATION.md) alongside this earlier record. The current provisional source is BAT-D4E-3B, not the plain D4H working file. Historical GNSS/Wi-Fi/battery and reset statements retain their original date and may be superseded by the linked records. This reconstruction does not rerun or extend historical acceptance.

# Security, privacy and responsible operation

**Repository:** Private engineering work-in-progress. This document is an operational handling policy, not a product security certification.

## Do not commit

- Real aircraft or Remote ID serials, FAA registration or other uniquely identifying codes.
- Unredacted GPS coordinates, home/work addresses, flight tracks, telemetry exports, BLE MAC addresses or other third-party identifiers.
- Credentials, API tokens, signing keys, SSH private keys, `.env` secrets or access tokens.
- Raw packet captures or non-sanitized USB logs/screenshots.
- Firmware dumps or proprietary licensed code without authorization or license review.

A `.gitignore` is a defense-in-depth helper, **not a security boundary**. Audit *every staged file* and its history before publishing; use synthetic fixtures when possible. GitHub private repositories can still expose content to authorized collaborators or future copies.

## Firmware privacy

- Default PRIVATE mode; deliberate OPEN mode for local controlled testing.
- Redact before sharing even if UI reports PRIVATE: inspect metadata, filenames, logs, images, screenshots and any console debug output.
- Snapshots inherit privacy selection; no assumption that every future transport parser automatically redacts new fields.
- Operator notice does not constitute a legal waiver or certify reliability.

## RF and UAS testing

- Passive reception of cooperative Remote ID is not a guarantee of detecting every aircraft or determining actual airborne safety.
- Do not describe coordinate-derived position plotting as RF radar or compute aircraft distance from RSSI.
- Use authorized equipment and lawful flight locations; manage FAA registration, airspace approvals and operating limitations separately.
- DJI Mini 5 Pro is a planned real-world Wi-Fi target; BLE-only firmware is not expected to decode Wi-Fi-only broadcasts.
- Keep testing safe and non-interfering; no deliberate RF jamming or interference.

## LiPo battery handling

Do not peel the battery's yellow electrical insulation or probe underneath it. Measure only accessible connector contacts with insulated, suitably fine voltage probes, avoiding shorts. Verify pack condition, single-cell voltage, connector keying/polarity and charger configuration before connection/charging. Do not assume wire color guarantees polarity. Stop if swollen or damaged.

## Publication checklist

Prior to public case study, source push or website embed:
1. Audit content and **full commit history** for private data/secrets.
2. Review third-party library/code attribution, applicable software licenses, hardware/vendor imagery and permitted redistribution.
3. Remove/sanitize real aircraft identifiers and coordinates.
4. Clearly mark operational vs planned claims and preserve evidence for accepted stages.
5. Keep public documentation informational, not flight-safety or law-enforcement marketing.
