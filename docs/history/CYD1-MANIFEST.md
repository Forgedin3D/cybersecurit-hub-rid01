# CYD1 historical import — integrity manifest

**Prepared November 4, 2026.** Historical test dates and original source hashes are not Git commit dates. This manifest enumerates only the curated CYD1 historical stage, not the later Waveshare release.

## Curated firmware sources

| Repository file | SHA-256 (source bytes) | Bytes |
| --- | --- | ---: |
| `firmware/cyd1/history/patch_stage4bb2a_fix1.py` | `c04f8098741c210f2d506b8ca79c2e973ebe2d5ba6c4e6d1d55a55870e2a726b` | 2,257 |
| `firmware/cyd1/history/stage4b-a-multitarget-ui.ino` | `12e17ef7c078afe3d2c899b684167061d974cd6892b1dfd996a523fbb4cdde9c` | 28,923 |
| `firmware/cyd1/history/stage4b-b2a-compilefail.ino` | `8ff4da0d1037153ef255778d19cab427c28069358dc1d653e51914abd0d70e03` | 74,078 |
| `firmware/cyd1/history/stage4b-b2a-compilepass.ino` | `bf157de938adee6f145c8830170c4ba6a38e51a773e0d1d6cdb43243bde872d1` | 75,209 |
| `firmware/cyd1/history/stage7c-backend.ino` | `c58a990b8fc8e33542acae9c33ced51e12293334a5f4290db3ff9139985b8a1d` | 49,177 |
| `firmware/cyd1/reference/CYD1_RID_Handheld/CYD1_RID_Handheld.ino` | `a76f5a9cf94899af528b5bcdace01e2bb7207ed3ecf3f4690f8d63ee9516525c` | 85,283 |

These firmware/patch bytes match the operator-provided 14-file CYD1 source archive. Source paths were renamed for presentation; byte content was preserved. The final reference source was also confirmed identical across four original local acceptance/archive filenames.

## Sanitized historic photographs

| Repository image | SHA-256 (sanitized JPEG bytes) | Bytes |
| --- | --- | ---: |
| `assets/cyd1/b2c-compile.jpg` | `01ab5a75cf7876b3b116a7e81050923383cde4c33740a91f089377dd88bdbeba` | 270,604 |
| `assets/cyd1/board-rear.jpg` | `0f5d435fd617d2f31370dbd5d120f183dfb189faed6abd84e7295070b7188eac` | 257,140 |
| `assets/cyd1/compile-correction.jpg` | `c8e0c315eabae40f8c6c24496aa8bc91e146ff97a02039c4e618489f0c70a81b` | 301,896 |
| `assets/cyd1/compile-failure.jpg` | `eba02bb1f4d3de260771f12bf4ccc847a18f523d9f1c3305491fc87224f6d5d3` | 323,059 |
| `assets/cyd1/home-begin.jpg` | `c0e31e298cd59a358d06fad62c5b237ae0758a0b4c10e3ec07eb170d1cbf3c00` | 197,846 |
| `assets/cyd1/privacy-output.jpg` | `3603b49b152a7fa7c4a326958f3347a946b59b976600cce288a64c988040d58f` | 187,701 |
| `assets/cyd1/redacted-rid-target.jpg` | `3c0905dfb29808790ae53432c6428ae5096e6d4ae88c86e07ea7f64b3c34c967` | 114,171 |
| `assets/cyd1/touch-validation.jpg` | `be2078bbc0ad4cda2a1578acafd21bda5f3acb3a5071bfab20cefab73b4ad9ee` | 216,001 |
| `assets/cyd1/ui-stage3.jpg` | `e6bc5b4879a386fdb3b5b983560f71cfc3279183e0dc3fe5438393ead7b18eaa` | 203,767 |

Each JPEG was generated as a separate RGB publication derivative, resized and/or cropped from an operator-owned historical photo, with original EXIF/metadata intentionally excluded. Verification found no EXIF, GPS or XMP in these publication derivatives. Original camera metadata and byte-for-byte original photo hashes are retained separately in the project owner’s private evidence records; they are **not** embedded here.

## Audit boundaries

- Source lint found no patterns for embedded credential assignments, IP/MAC literals, email addresses, network URLs or nonzero hardcoded latitude/longitude values in the six selected source/patch files. This is a scoped source-content check, not a formal security certification.
- Historic byte identity does not establish reproducible compilation on the current host. Source audit did not execute or flash historic patch scripts.
- Upstream dependency licenses and any full-publication decision must be separately reviewed; the current repository remains private.
- Original raw logs, factory flash backup binaries and unredacted telemetry are deliberately excluded.
