# RID-01 project roadmap — closed

**Current status (November 17, 2026): development concluded at R7.** Earlier pages and git commits document the original development intentions and must be interpreted as historical, not as ongoing work.

## Delivered as a portfolio prototype

- BLE Remote ID receiver with four target session slots, lifecycle and touch interface.
- External LC76G receiver GNSS integration and GNSS-referenced RADAR.
- Source-verified R7 RADAR UI with corner callouts and target colors.
- Python-assisted build/test workflow and bounded evidence.

## Not delivered on this hardware

- Wi-Fi Beacon Remote ID reception (including DJI Mini 5 Pro Wi-Fi RID).
- microSD logging, unvalidated extended-range reception, commercial enclosure, battery qualification or dependable field-range detection.

## Stop reason

Outdoor operator testing rejected the Waveshare architecture for the intended field-use performance. Further hardware development was suspended and the remaining feature roadmap cancelled on this platform. See [final decision](waveshare/R7_FINAL_ASSESSMENT.md), [bounded validation](waveshare/R7_VALIDATION.md) and [project overview](../README.md).

The historical roadmap remains recoverable from earlier Git commits. Nothing here claims those earlier tasks were completed.
