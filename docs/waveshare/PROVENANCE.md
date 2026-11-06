# Source provenance, notices and sharing controls

## Evidence hierarchy

A recorded source hash establishes file identity, not runtime success. A recorded compile establishes that build, not all other source/configuration variants. A still photograph establishes displayed content, not a time-series transition or installed binary hash. The new November 6 four-source photos are not backdated October acceptance. Reported tests retain the stage and hardware used.

Primary inputs for this reconstruction are the uploaded hash-verified Waveshare private audit ZIP, its 678 selected original artifacts, the November 5 engineering report, October/November handoffs, the operator's pasted test outputs, and the November 6 staged media manifests. The raw archive, whole conversations, factory firmware dumps, unredacted logs and GPS-tagged originals are deliberately not imported.

## Human, assistant and third-party contributions

Christopher Estrada directed requirements and scope, operated boards/transmitters, observed defects, supplied results and controlled acceptance. Assistant support supplied design/code proposals, analysis and documentation. Open-source/vendor components are separate dependencies. Do not imply unaided original authorship of all code or independent laboratory certification.

## Notices actually recovered

| Component | Supplied notice/identity | Handling in this proposal |
|---|---|---|
| Arduino_GFX 1.5.5 | Top-level BSD-style Adafruit copyright notice; SPI source also references Espressif origin | Retain supplied notice and original-origin explanation; include only exact local diffs, not entire library |
| SensorLib 0.3.1 | Top-level MIT, Lewis He; additional bundled Bosch files require their own notices | Retain top-level notice/version metadata; do not vendor library/firmware blobs here |
| TCA9554 0.1.2 | MIT, Rob Tillaart | Retain notice/version metadata; no complete library import |
| Arduino ESP32/platform/toolchain | External installed components; version recorded in Kali | Not redistributed; build/log records required |
| Waveshare board examples | Used as hardware bring-up reference | Original vendor references stay in private audit archive; no wholesale example import |

This is a scoped provenance/notice review of the proposed private import, **not blanket legal clearance** of all transitive components or a new license grant. No project-wide source license is assigned. Public distribution of source or binaries and any dependency vendoring require a separate decision/review. Do not remove upstream maintainers' public attribution as though it were the operator's private data.

## Privacy treatment

Selected image derivatives are separate copies. EXIF/GPS/XMP are absent; selected visible identifier/coordinate/altitude fields are irreversibly masked. The OPEN example remains labeled OPEN, with sharing edits explicitly disclosed. Original audio and auxiliary video tracks were removed; generic codec metadata remains. Source/derivative hashes and crops are recorded without original GPS values.

Selected source/records received a bounded pattern screen and manual context checks for credentials, private keys, literal device/network IDs, coordinates and local paths. Synthetic fixture coordinates are documented as synthetic. This is not proof that all secrets or vulnerabilities are absent. Retained comments/startup labels include historical text and disabled CYD1 sections; this import preserves bytes rather than silently refactoring them.

## Publication gates

Keep the repository private. The November 6 fresh Kali compile evidence has now been reviewed. Reconfirm the current base commit, complete final operator review (including video playback and reflections), approve the exact file list, then perform a separate controlled commit. No original media, raw logs, factory binaries, new release tag, Issue change, Project creation or visibility change is authorized by the preparation step.

[Evidence source register](EVIDENCE_SOURCES.md).
