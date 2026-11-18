# Final engineering assessment — RID-01 R7

**Decision date:** November 17, 2026\
**Outcome:** Close development on the Waveshare ESP32-S3/LC76G architecture. Retain software, test evidence and supported lessons as a cybersecurity/embedded-systems portfolio. **Do not represent this as a market-ready or field-qualified Remote ID scanner.**

## What succeeded

R7 demonstrates genuine BLE Remote ID reception on a touchscreen, four session target slots, lifecycle handling, GNSS-referenced coordinate visualization, snapshots, RESCAN and responsive user controls under the observed bench workloads. Candidate freezes, SHA-256 source verification, isolated staging/compilation, guarded uploads and Python guided logging preserve engineering evidence. The 1,024-byte GNSS RX investigation produced a measurable improvement relative to the earlier 256-byte diagnostic workload. One completed R7 observation recorded zero **new** GNSS/UART receive errors, while retaining 28 HOST incomplete-dashboard notices.

## Why the board was rejected

The operator carried out informal outdoor separation experiments with two real R111S sources and repeatedly observed unsatisfactory target continuity near or beyond roughly 35 metres under that setup. This was a decisive *operational rejection*, **not a controlled proof of a hard 35 m RF cutoff**. The precise contribution of BLE reception, transmitter update behavior, geographic freshness and GNSS positional uncertainty was not isolated with independent RF instrumentation before project closure.

In addition, the board was not an attractive final field platform given its lack of a convenient user-supported external BLE/Wi-Fi antenna connector, separate GNSS board/harness, unqualified fuel-gauge percentage/charging safety performance, and small reflective outdoor touchscreen. These are architectural suitability findings, not claims that every part of the board is defective. The earlier purchase and integration effort was useful as a development testbed, but continuing with more software features would not have met the operator's objectives.

## Closure boundary

- Final accepted **portfolio version is R7 BLE/GNSS**. Preserve R6 as a rollback reference.
- **Stop** further Waveshare microSD and Wi-Fi RID work; neither was implemented. No additional RADAR modes, new branding animation, or final commercial enclosure.
- Existing R7 bounded results and historical CYD1/pre-GNSS records remain valid within their original test scopes; **the old future roadmap is superseded**.
- No more hardware testing is required merely to publish this accurately bounded case study.
- Future hardware selection, if ever revisited, is a *new* design decision and cannot retroactively upgrade R7's field claims.

## Professional learning outcomes

This work connects wireless packet decoding, embedded-systems scheduling, serial communication, privacy-conscious logging, traceable artifacts and structured failure analysis. The strongest conclusion is that **passing controlled functionality tests does not establish production or field suitability**. Its prototype status, unresolved issues and reasons for stopping are intentionally visible to reviewers.

## Disclosure and limitations

Passive cooperative broadcast data can be spoofed or absent; RID-01 cannot guarantee detection or operational aviation safety. PRIVATE prevents selected UI/serial disclosures but is not encryption or authentication. The operator led requirements, wiring, bench/field experiments, observations and acceptance decisions; AI assistance supplied substantial code/tooling and technical documentation. Third-party firmware/libraries remain credited separately. This project is not a claim of unaided advanced software-engineering authorship.
