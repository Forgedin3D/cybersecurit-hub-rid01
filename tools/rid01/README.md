# Python automation — audited historical RID-01 tools

These four scripts were used as **environment-coupled development/test tooling**, not general-purpose utilities. They are shown as educational portfolio examples. They are not a Python library, not tested on a clean machine, and not a safe one-click workflow for unrelated ESP32 devices.

| File | Observed workflow role |
|---|---|
| [Checksum A/B/A analysis](analysis/RID01_D3B1_CHECKSUM_ABA_ANALYZE.py) | Read and compare diagnostic counters from bounded test output |
| [RX1024 staging](staging/RID01_STAGE_D3B2_RX1024_R3_v1.py) | Verify source/rollback hashes, create isolated compile-only candidate |
| [Four-source capture](capture/RID01_CAPTURE_RX1024_R3_4SRC_ABA_v3.py) | Allowlisted, privacy-filtered serial monitor for R111S plus controlled beacon tests |
| [R7 guided capture](capture/RID01_CAPTURE_RADAR_COLOR_R7_GUIDED_v1.py) | Time-bounded read-only capture and manual UI acceptance prompts |

## Engineering sequence

1. Capture the accepted source/dependency identifiers with SHA-256.
2. Stage a new candidate separately; validate expected input hashes before/after.
3. Compile in an isolated directory and review outputs. Compiling is distinct from uploading.
4. Use a separate deliberately authorized upload step; retain a rollback. The **historical R7 uploader** is omitted from the main portfolio shortlist because it depends on a large embedded-data staging pair and exact local files.
5. Run a bounded hardware test with a guided serial observer and manual touchscreen steps, retaining only allowlisted sanitized evidence.
6. Compare error counter *changes*, target lifecycle and UI observations; record accepted conditions and known limits.

**Safety and portability:** Scripts use historical exact firmware hashes, OS-dependent paths, serial ports, installed `arduino-cli`, and a specific local ESP32 core/dependency tree. Do **not** execute staging scripts on unreviewed files, launch a serial capture against an unrelated device, or assume Python syntax validation proves runtime safety. The capture helper can open USB serial and thereby affect/reset a device even though it does not send application commands.

**Attribution:** Source was generated/refined through substantial AI assistance based on owner-directed requirements, manual hardware operations and testing. Use as examples of the controlled development process, not proof of unaided Python software-engineering expertise. Third-party library/vendor code is not included here.
