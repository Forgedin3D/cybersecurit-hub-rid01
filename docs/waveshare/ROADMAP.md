# Next milestones and rollback boundaries

## Pre-GNSS repository gate — current work

Assemble current source/history/media, retain byte identities and limitations, finish isolated compile, review sanitized evidence and exact file list, obtain approval, then perform one retrospective import. Preserve the CYD1 chapter; do not recreate old Git timestamps. Repository update and hardware modification are separate gates.

## Receiver GNSS — next development phase

After actual arrival, verify the delivered module/revision, connector pinout, voltage requirements and wiring against authoritative documentation before connection. Create an isolated GNSS candidate. Test acquisition, no fix, valid reference, stale reference, loss/recovery, receiver movement, display provenance and privacy. Regress BLE targets, snapshots, STOP/HOME, acknowledgment and battery behavior under the added load. An existing GNSS enum is not implementation.

Define and pass acceptance criteria, then freeze source, dependency/configuration records, actual compiled artifacts/hashes and a tested rollback. Document the milestone as it happens.

## Wi-Fi Beacon — deferred until that freeze

Start with a separate Wi-Fi-only proof of reception/decoding using the owned aircraft. Verify its actual RID transport, supported band/channel and test conditions; do not assume the controller video-link setting identifies the RID channel. Then measure combined BLE/Wi-Fi behavior: source counts versus geographic eligibility, received-message gaps, freshness/lifecycle, channel scanning, touch responsiveness and power.

**Proposed mode strategy, not implemented:** BLE-only as a recoverable baseline; Wi-Fi Beacon-only as an independently testable mode; combined scanning only after evidence supports its performance. Retain separate modes if combined operation is insufficient. No UI buttons or radio code for these modes are added by this documentation package.

## Final integration and enclosure

Finish required modules and unresolved acceptance gates before physical architecture/CAD/enclosure commitments. Preserve battery-safety qualifications, outdoor geographic validation and integrated endurance as explicit work. Do not equate a clean firmware freeze with completion of every hardware-safety or RF-performance test.
