#!/usr/bin/env python3
"""RID-01 D3B2 RX1024 R3 -- GUARDED COMPILE-ONLY CANDIDATE.

Derives a separate test sketch from the exact reviewed UART Events R2 source.
Requests a 1024-byte GNSS UART software RX buffer before begin(), checks the
request result and installed driver, and labels the existing 10-second UART
summary with candidate/configuration metadata. Error counters and all normal
GNSS/BLE/parser/reference behavior are retained. Failed initialization inhibits
GNSS parsing and reports uart_init=FAIL; it never silently falls back.

This helper verifies the existing source/application and accepted rollbacks,
stages only a new directory, and compiles with the same pinned core and board
menus. It never opens a serial port, uploads, installs dependencies, removes an
existing stage, or writes to GitHub. A review ZIP is generated automatically in
Downloads when possible. No third-party Python modules are required.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path.home() / "Waveshare_RID_S3"
SOURCE_WORK = ROOT / "RID01_GNSS_D3B2_UART_EVENTS_R2_TEST_ONLY"
SOURCE = SOURCE_WORK / "RID_UI_Migration" / "RID_UI_Migration.ino"
EXPECTED_SOURCE = "2aeb5aac6fd4c76911f01e13c7b710f59556b7efd35ddc16e0479394d514a265"
# Source and application identities reviewed after the successful R2 upload/test.
R2_BIN = SOURCE_WORK / "build" / "RID_UI_Migration.ino.bin"
EXPECTED_R2_BIN = "65c88bf60f1f30be931807dedb5193f88992d95f58110277774e6f40e7f8f9be"

D3B1_FREEZE = ROOT / "RID01_GNSS_D3B1_R1_ACCEPTED_2026-11-12_LOCAL"
D3B1_SOURCE = D3B1_FREEZE / "source" / "RID_UI_Migration.ino"
EXPECTED_D3B1 = "695fae2ba7f112ba215da055ab994ffafaed22add607f0ab968faeb5bd25c41b"
D3A_SOURCE = ROOT / "RID01_GNSS_D3A_ACCEPTED_2026-11-12_LOCAL" / "source" / "RID_UI_Migration.ino"
EXPECTED_D3A = "da324a7495fe04b8db05b3c583786635d1a97bd22cb9565fd8f387043e84ea6b"
CORE = Path.home() / ".arduino15" / "packages" / "esp32" / "hardware" / "esp32" / "3.3.11"

WORK = ROOT / "RID01_GNSS_D3B2_RX1024_R3_TEST_ONLY"
SKETCH_DIR = WORK / "RID_UI_Migration"
SKETCH = SKETCH_DIR / "RID_UI_Migration.ino"
BUILD = WORK / "build"
LOG = WORK / "COMPILE_RX1024_R3.log"
FQBN = (
    "esp32:esp32:esp32s3:USBMode=hwcdc,CDCOnBoot=cdc,"
    "FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,"
    "PSRAM=opi,CPUFreq=240"
)
EXPECTED_CANDIDATE = "bdad0c79b6d08b4a622348e10c2c0234cff2005033edd6404e2d536d2a13f607"
EXPECTED_CORE_FILES = {
    "HardwareSerial.h": "f0a25409647e9c5daf3cb9b59f5d7020a422962aab7afe7cb0e637e510af92ac",
    "HardwareSerial.cpp": "df718558a059669e35b1c034d6e458b00d13317ec8403e5a74dd9a84898544bf",
}
EXPECTED_MENU = dict(item.split("=", 1) for item in FQBN.split(":", 3)[3].split(","))
ARTIFACT_NAMES = (
    "RID_UI_Migration.ino.bin", "RID_UI_Migration.ino.elf",
    "RID_UI_Migration.ino.bootloader.bin", "RID_UI_Migration.ino.partitions.bin",
)

# Only these scoped replacements alter the exact R2 sketch.
# No custom return types are introduced for Arduino's .ino prototype generator.
CONFIG_GLOBALS = r'''// RX1024 R3: one UART configuration experiment with explicit init evidence.
// rx_set is the setter result, not a measurement of usable ring-buffer capacity.
static constexpr size_t RID_GNSS_RX1024_BYTES = 1024U;
static size_t ridGnssRx1024Set = 0U;
static bool ridGnssRx1024InitOk = false;

'''

BEGIN_BLOCK = r'''void ridGnssD3b1Begin() {
    ridGnssRx1024InitOk = false;
    ridGnssRx1024Set = Serial1.setRxBufferSize(RID_GNSS_RX1024_BYTES);
    if (ridGnssRx1024Set != RID_GNSS_RX1024_BYTES) {
        Serial.printf("[GNSS-D3B2-RXCFG] candidate=RX1024_R3 rx_requested=%lu "
                      "rx_set=%lu uart_init=FAIL reason=RX_SIZE\n",
            (unsigned long)RID_GNSS_RX1024_BYTES,
            (unsigned long)ridGnssRx1024Set);
        return;
    }
    Serial1.begin(115200, SERIAL_8N1, 44, -1);
    if (!Serial1) {
        Serial.printf("[GNSS-D3B2-RXCFG] candidate=RX1024_R3 rx_requested=%lu "
                      "rx_set=%lu uart_init=FAIL reason=UART_INIT\n",
            (unsigned long)RID_GNSS_RX1024_BYTES,
            (unsigned long)ridGnssRx1024Set);
        return;
    }
'''

SUCCESS_BLOCK = r'''    // Driver installation was checked above; onReceiveError() returns void.
    // This flag does not independently attest UART event-task allocation.
    ridGnssRx1024InitOk = true;
    Serial.printf("[GNSS-D3B2-RXCFG] candidate=RX1024_R3 rx_requested=%lu "
                  "rx_set=%lu uart_init=OK\n",
        (unsigned long)RID_GNSS_RX1024_BYTES,
        (unsigned long)ridGnssRx1024Set);

'''

def stop(message: str) -> None:
    raise RuntimeError(message)


def digest(path: Path) -> str:
    if not path.is_file() or path.stat().st_size == 0:
        stop(f"Missing/empty required file: {path}")
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def check(path: Path, expected: str) -> None:
    actual = digest(path)
    if actual != expected:
        stop(f"SHA-256 mismatch: {path}\nEXPECTED {expected}\nACTUAL   {actual}")
    print(f"PASS SHA256: {path}", flush=True)


def check_accepted_manifest(folder: Path, filename: str = "SHA256SUMS.txt") -> dict:
    manifest = folder / filename
    if not manifest.is_file():
        stop(f"Missing manifest: {manifest}")
    checked = 0
    entries = {}
    for ln in manifest.read_text(encoding="utf-8").splitlines():
        if not ln.strip():
            continue
        pair = ln.split("  ", 1)
        if len(pair) != 2 or len(pair[0]) != 64 or any(c not in "0123456789abcdef" for c in pair[0]):
            stop("Invalid acceptance manifest line")
        hash_expected, rel = pair
        if rel in entries or Path(rel).is_absolute() or any(p in ("", ".", "..") for p in rel.split("/")) or "\\" in rel:
            stop("Unsafe or duplicate acceptance manifest path")
        entries[rel] = hash_expected
        target = (folder / rel).resolve()
        if not target.is_relative_to(folder.resolve()):
            stop("Refusing a path outside the acceptance folder")
        check(target, hash_expected)
        checked += 1
    if checked < 5:
        stop(f"Acceptance manifest unexpectedly small: {checked}")
    return entries


def check_core() -> dict:
    header = CORE / "cores" / "esp32" / "HardwareSerial.h"
    cpp = CORE / "cores" / "esp32" / "HardwareSerial.cpp"
    for path in (header, cpp):
        if not path.is_file():
            stop(f"Missing installed core source: {path}")
    h = header.read_text(encoding="utf-8")
    code = cpp.read_text(encoding="utf-8")
    required_h = (
        "void onReceiveError(OnReceiveErrorCb function);",
        "typedef std::function<void(hardwareSerial_error_t)> OnReceiveErrorCb;",
        "UART_BUFFER_FULL_ERROR", "UART_FIFO_OVF_ERROR",
        "UART_FRAME_ERROR", "UART_PARITY_ERROR", "UART_BREAK_ERROR",
        "UART_NO_ERROR",
        "size_t setRxBufferSize(size_t new_size);",
        "operator bool() const;",
    )
    for needle in required_h:
        if needle not in h:
            stop(f"Installed core lacks expected UART API: {needle}")
    if "void HardwareSerial::onReceiveError(OnReceiveErrorCb function)" not in code:
        stop("Installed core lacks onReceiveError implementation")
    if "void HardwareSerial::_uartEventTask(void *args)" not in code:
        stop("Installed core lacks UART event task implementation")
    for needle in ("size_t HardwareSerial::setRxBufferSize(size_t new_size)",
                   "HardwareSerial::operator bool() const", "uartIsDriverInstalled(_uart)"):
        if needle not in code:
            stop(f"Installed core lacks required RX configuration/driver check: {needle}")
    hashes = {}
    for name, expected in EXPECTED_CORE_FILES.items():
        path = CORE / "cores" / "esp32" / name
        check(path, expected)
        hashes[name] = expected
    print("PASS: Installed UART API files match official Arduino-ESP32 3.3.11", flush=True)
    return hashes


def capture_runtime() -> dict:
    # Reuse this exact executable, cwd and environment for selection and compile.
    environment = dict(os.environ)
    executable = shutil.which("arduino-cli", path=environment.get("PATH"))
    if not executable:
        stop("arduino-cli is not available on PATH")
    cli = Path(executable).resolve(strict=True)
    if not cli.is_file() or not os.access(cli, os.X_OK):
        stop(f"arduino-cli is not an executable file: {cli}")
    return {"cli": str(cli), "cwd": str(ROOT.resolve(strict=True)), "env": environment}


def check_selected_core(runtime: dict) -> dict:
    version = subprocess.run(
        [runtime["cli"], "version"], cwd=runtime["cwd"], env=runtime["env"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False, timeout=15,
    )
    if version.returncode != 0 or not version.stdout.strip():
        stop(f"Could not record arduino-cli version ({version.returncode}):\n{version.stderr[-8000:]}")
    command = [runtime["cli"], "board", "details", "--fqbn", FQBN, "--json"]
    result = subprocess.run(
        command, cwd=runtime["cwd"], env=runtime["env"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=False, timeout=60,
    )
    if result.returncode != 0:
        stop(f"Board-details lookup failed ({result.returncode}):\n{result.stderr[-8000:]}")
    try:
        details = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        stop(f"Board-details stdout is not valid JSON: {exc}")
    if not isinstance(details, dict):
        stop("Unexpected board-details JSON object")
    if details.get("fqbn") != "esp32:esp32:esp32s3" or details.get("version") != "3.3.11":
        stop(f"Unexpected selected board/core: {details.get('fqbn')} / {details.get('version')}")

    required_paths = {
        "runtime.platform.path": CORE,
        "build.board.platform.path": CORE,
        "build.core.platform.path": CORE,
        "build.core.path": CORE / "cores" / "esp32",
    }
    properties = details.get("build_properties")
    if not isinstance(properties, list):
        stop("Board-details build_properties is not a list")
    selected_paths = {}
    for key, expected in required_paths.items():
        values = [p.split("=", 1)[1] for p in properties
                  if isinstance(p, str) and p.startswith(key + "=")]
        if len(values) != 1:
            stop(f"Expected exactly one resolved build property: {key}")
        actual = Path(values[0])
        if not actual.is_absolute() or actual.resolve(strict=True) != expected.resolve(strict=True):
            stop(f"Selected core path does not match expected 3.3.11 installation: {key}={values[0]}")
        selected_paths[key] = str(actual.resolve(strict=True))

    options = details.get("config_options")
    if not isinstance(options, list):
        stop("Board-details config_options is not a list")
    selected_menu = {}
    for key, expected in EXPECTED_MENU.items():
        matches = [o for o in options if isinstance(o, dict) and o.get("option") == key]
        if len(matches) != 1 or not isinstance(matches[0].get("values"), list):
            stop(f"Missing/ambiguous board menu option: {key}")
        selected = [v.get("value") for v in matches[0]["values"]
                    if isinstance(v, dict) and v.get("selected") is True]
        if selected != [expected]:
            stop(f"Unexpected selected board menu: {key}={selected}; expected {expected}")
        selected_menu[key] = expected
    # Validate menu selections: QIO menu legitimately resolves to build.flash_mode=dio.
    print("PASS: CLI selected ESP32-S3 / core 3.3.11 / expected paths and board menus", flush=True)
    return {
        "arduino_cli": runtime["cli"], "working_directory": runtime["cwd"],
        "arduino_cli_version": version.stdout.strip(),
        "board": details["fqbn"], "core_version": details["version"],
        "requested_fqbn": FQBN, "selected_menu": selected_menu,
        "selected_paths": selected_paths,
    }


def replace_once(s: str, previous: str, following: str, label: str) -> str:
    count = s.count(previous)
    if count != 1:
        stop(f"Expected exactly one {label} anchor; found {count}; no output written")
    return s.replace(previous, following, 1)


def patch(original: bytes) -> bytes:
    if hashlib.sha256(original).hexdigest() != EXPECTED_SOURCE:
        stop("Patch input is not the independently reviewed R2 source")
    if b"\r\n" in original:
        stop("Unexpected CRLF source; refusing to change newlines")
    text = original.decode("utf-8")
    if "Serial1.setRxBufferSize(" in text or "RX1024_R3" in text:
        stop("RX-buffer experiment is already present; refusing to duplicate")
    text = replace_once(text,
        "static portMUX_TYPE ridGnssR2ErrorMux = portMUX_INITIALIZER_UNLOCKED;",
        CONFIG_GLOBALS + "static portMUX_TYPE ridGnssR2ErrorMux = portMUX_INITIALIZER_UNLOCKED;",
        "configuration globals")
    text = replace_once(text,
        "void ridGnssD3b1Begin() {\n    Serial1.begin(115200, SERIAL_8N1, 44, -1);\n",
        BEGIN_BLOCK, "guarded UART initialization")
    text = replace_once(text,
        '    Serial.println("[GNSS-D3B1] UART1 RX=GPIO44 / 115200 / receive-only");',
        SUCCESS_BLOCK + '    Serial.println("[GNSS-D3B1] UART1 RX=GPIO44 / 115200 / receive-only");',
        "successful initialization marker")
    text = replace_once(text,
        "void ridGnssD3b1Tick() {\n",
        "void ridGnssD3b1Tick() {\n"
        "    // A rejected configuration must not produce a valid GNSS comparison.\n"
        "    if (!ridGnssRx1024InitOk) return;\n",
        "failed-initialization receive guard")
    text = replace_once(text,
        '        "frame=%lu parity=%lu break=%lu other=%lu\\n",',
        '        "frame=%lu parity=%lu break=%lu other=%lu "\n'
        '        "candidate=RX1024_R3 rx_requested=%lu rx_set=%lu uart_init=%s\\n",',
        "periodic configuration metadata")
    text = replace_once(text,
        "        (unsigned long)breakCount, (unsigned long)unknown\n",
        "        (unsigned long)breakCount, (unsigned long)unknown,\n"
        "        (unsigned long)RID_GNSS_RX1024_BYTES,\n"
        "        (unsigned long)ridGnssRx1024Set,\n"
        '        ridGnssRx1024InitOk ? "OK" : "FAIL"\n',
        "periodic metadata arguments")
    if text.count("Serial1.setRxBufferSize(") != 1 or text.count("Serial1.begin(") != 1:
        stop("Unexpected UART setup call count")
    if text.count("Serial1.onReceiveError(") != 1 or text.count("ridGnssR2ReportUartErrorEvents();") != 1:
        stop("R2 callback/report count changed")
    return text.encode("utf-8")


def check_preserved_inputs() -> None:
    check(SOURCE, EXPECTED_SOURCE)
    check(R2_BIN, EXPECTED_R2_BIN)
    entries = check_accepted_manifest(SOURCE_WORK, "CANDIDATE_SHA256SUMS.txt")
    required = {"RID_UI_Migration/RID_UI_Migration.ino", "PREFLIGHT_UART_EVENTS_R2.json",
                "COMPILE_UART_EVENTS_R2.log", "SOURCE_DIFF_R1_TO_R2.patch"}
    required |= {"build/" + name for name in ARTIFACT_NAMES}
    if not required.issubset(entries):
        stop("Preserved R2 manifest lacks required source, build or environment evidence")
    if entries["RID_UI_Migration/RID_UI_Migration.ino"] != EXPECTED_SOURCE or entries["build/RID_UI_Migration.ino.bin"] != EXPECTED_R2_BIN:
        stop("Preserved R2 manifest disagrees with independently reviewed source/application")
    result = json.loads((SOURCE_WORK / "RUN_RESULT.json").read_text(encoding="utf-8"))
    if not isinstance(result, dict) or result.get("status") != "COMPILE_PASS" or result.get("compile_exit_code") != 0:
        stop("Preserved R2 run record does not report COMPILE_PASS / exit 0")
    artifacts = result.get("artifact_sha256")
    if not isinstance(artifacts, dict) or any(artifacts.get(name) != entries["build/" + name] for name in ARTIFACT_NAMES):
        stop("Preserved R2 run record fingerprints disagree with its manifest")
    check(D3B1_SOURCE, EXPECTED_D3B1)
    check(D3A_SOURCE, EXPECTED_D3A)
    rollback = check_accepted_manifest(D3B1_FREEZE)
    if rollback.get("source/RID_UI_Migration.ino") != EXPECTED_D3B1:
        stop("Accepted D3B1 manifest lacks the reviewed rollback source")


def library_selection(log: Path) -> list[str]:
    lines = []
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if re.fullmatch(r"Using library .+ at version .+ in folder: .+", line):
            lines.append(line)
    if not lines or len(lines) != len(set(lines)):
        stop(f"Missing or duplicate selected-library evidence: {log}")
    return sorted(lines)


def compare_r2_environment(evidence: dict) -> list[str]:
    prior = json.loads((SOURCE_WORK / "PREFLIGHT_UART_EVENTS_R2.json").read_text(encoding="utf-8"))
    if not isinstance(prior, dict):
        stop("Preserved R2 preflight is not a JSON object")
    for key in ("arduino_cli", "arduino_cli_version", "working_directory", "board",
                "core_version", "requested_fqbn", "selected_menu", "selected_paths"):
        if prior.get(key) != evidence.get(key):
            stop(f"Build environment differs from reviewed R2: {key}; review before changing variables")
    selected = library_selection(SOURCE_WORK / "COMPILE_UART_EVENTS_R2.log")
    print("PASS: CLI/core/menu selection matches preserved R2 preflight.", flush=True)
    return selected


def save_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def save_candidate_manifest() -> dict:
    paths = [SKETCH, WORK / "PREFLIGHT_RX1024_R3.json", WORK / "SOURCE_DIFF_R2_TO_RX1024_R3.patch"]
    if (WORK / "RUN_RESULT.json").is_file():
        paths.append(WORK / "RUN_RESULT.json")
    paths += [p for p in [LOG] + [BUILD / name for name in ARTIFACT_NAMES]
              if p.is_file() and p.stat().st_size]
    hashes = {p.relative_to(WORK).as_posix(): digest(p) for p in paths}
    (WORK / "CANDIDATE_SHA256SUMS.txt").write_text(
        "".join(f"{h}  {name}\n" for name, h in sorted(hashes.items())), encoding="utf-8",
    )
    return hashes


def compile_candidate(runtime: dict, command: list[str]) -> int:
    with LOG.open("w", encoding="utf-8") as output:
        process = subprocess.Popen(
            command, cwd=runtime["cwd"], env=runtime["env"],
            stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT, start_new_session=True,
        )
        try:
            return process.wait()
        except BaseException:
            # Signal the owned group even if its CLI leader already exited.
            # This also stops any surviving compiler descendants.
            try:
                os.killpg(process.pid, signal.SIGINT)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait()
            raise


def make_review_zip() -> None:
    """Collect only this candidate's review evidence; never overwrite a ZIP."""
    files = [SKETCH, WORK / "PREFLIGHT_RX1024_R3.json",
             WORK / "SOURCE_DIFF_R2_TO_RX1024_R3.patch", WORK / "RUN_RESULT.json",
             WORK / "CANDIDATE_SHA256SUMS.txt", LOG]
    files += [BUILD / name for name in ARTIFACT_NAMES]
    files += [BUILD / name for name in ("build.options.json", "boot_app0.bin", "partitions.csv")]
    destination = Path.home() / "Downloads"
    if not destination.is_dir():
        destination = WORK
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
    archive = destination / f"RID01_D3B2_RX1024_R3_COMPILE_REVIEW_{stamp}.zip"
    members = {}
    with ZipFile(archive, "x", compression=ZIP_DEFLATED) as zipped:
        for path in files:
            if path.is_file() and path.stat().st_size:
                name = path.relative_to(WORK).as_posix()
                data = path.read_bytes()
                zipped.writestr(name, data)
                members[name] = hashlib.sha256(data).hexdigest()
        zipped.writestr("REVIEW_ARCHIVE_SHA256SUMS.txt",
                        "".join(f"{sha}  {name}\n" for name, sha in sorted(members.items())))
    with ZipFile(archive) as zipped:
        if zipped.testzip() is not None:
            stop(f"Review ZIP integrity check failed: {archive}")
    print(f"REVIEW ZIP: {archive}", flush=True)
    print("Attach this ZIP for compile review before the separate upload/test step.", flush=True)


def main(preflight_only: bool = False) -> None:
    print("=== RID-01 GNSS D3B2 RX1024 R3: GUARDED STAGING ===", flush=True)
    print("COMPILE ONLY / NO UPLOAD / NO GITHUB / NO ACCEPTED SOURCE EDIT", flush=True)
    check_preserved_inputs()
    if WORK.exists() or WORK.is_symlink():
        stop(f"Diagnostic directory already exists; no overwrite: {WORK}")
    if not (ROOT / "compat_libraries").is_dir():
        stop("Expected local compatibility libraries not present")
    runtime = capture_runtime()
    evidence = check_selected_core(runtime)
    evidence["official_3_3_11_uart_files"] = check_core()
    prior_libraries = compare_r2_environment(evidence)
    evidence["r2_selected_libraries"] = prior_libraries
    original = SOURCE.read_bytes()
    if hashlib.sha256(original).hexdigest() != EXPECTED_SOURCE:
        stop("R2 source changed during preflight")
    candidate = patch(original)
    candidate_hash = hashlib.sha256(candidate).hexdigest()
    if candidate_hash != EXPECTED_CANDIDATE:
        stop(f"Unexpected RX1024 R3 candidate hash: {candidate_hash}")
    evidence["r2_source_sha256"] = EXPECTED_SOURCE
    evidence["r2_application_sha256"] = EXPECTED_R2_BIN
    evidence["candidate_source_sha256"] = candidate_hash
    evidence["candidate"] = "RX1024_R3"
    evidence["rx_buffer_request_bytes"] = 1024
    evidence["runtime_acceptance"] = "PENDING: requires separate reviewed upload and physical A/B/A"
    print(f"PASS: Guarded in-memory patch SHA256={candidate_hash}", flush=True)
    if preflight_only:
        print(json.dumps(evidence, indent=2, sort_keys=True), flush=True)
        print("=== RX1024 R3 PREFLIGHT PASS: NO CANDIDATE DIRECTORY WRITTEN / NO COMPILE / NO UPLOAD ===", flush=True)
        return

    command = [
        runtime["cli"], "compile", "--jobs", "2", "--verbose",
        "--libraries", str(ROOT / "compat_libraries"),
        "--fqbn", FQBN,
        "--build-path", str(BUILD),
        str(SKETCH_DIR),
    ]
    # Atomically reserve the top-level directory. Never delete or reuse an existing stage.
    WORK.mkdir(exist_ok=False)
    run_result = {"status": "STAGING", "compile_command": command, "compile_exit_code": None}
    result_path = WORK / "RUN_RESULT.json"
    try:
        save_json(result_path, run_result)
        SKETCH_DIR.mkdir(exist_ok=False)
        with SKETCH.open("xb") as output:
            output.write(candidate)
        save_json(WORK / "PREFLIGHT_RX1024_R3.json", evidence)
        diff = difflib.unified_diff(
            original.decode("utf-8").splitlines(keepends=True),
            candidate.decode("utf-8").splitlines(keepends=True),
            fromfile="D3B2_UART_EVENTS_R2/RID_UI_Migration.ino",
            tofile="D3B2_RX1024_R3/RID_UI_Migration.ino",
        )
        (WORK / "SOURCE_DIFF_R2_TO_RX1024_R3.patch").write_text("".join(diff), encoding="utf-8")
        save_candidate_manifest()
        print(f"NEW ISOLATED SOURCE: {SKETCH}", flush=True)
        print("=== COMPILING RX1024 R3 (2 JOBS, VERBOSE LOG, NO UPLOAD) ===", flush=True)
        run_result["status"] = "COMPILING"
        save_json(result_path, run_result)
        exit_code = compile_candidate(runtime, command)
        run_result["compile_exit_code"] = exit_code
        print(f"COMPILER EXIT CODE: {exit_code}", flush=True)
        print("=== LAST 40 COMPILER LOG LINES ===", flush=True)
        for line in LOG.read_text(encoding="utf-8", errors="replace").splitlines()[-40:]:
            print(line, flush=True)
        check(SKETCH, candidate_hash)
        check_preserved_inputs()
        if exit_code != 0:
            run_result["status"] = "COMPILE_FAILED"
            stop(f"RX1024 R3 compile failed; log preserved at {LOG}; NO UPLOAD")
        selected = library_selection(LOG)
        if selected != prior_libraries:
            stop("Selected library versions/folders differ from R2; compile review required")
        run_result["selected_libraries_match_r2"] = True
        for name in ARTIFACT_NAMES:
            digest(BUILD / name)
        hashes = save_candidate_manifest()
        run_result["status"] = "COMPILE_PASS"
        run_result["artifact_sha256"] = {name: hashes["build/" + name] for name in ARTIFACT_NAMES}
        save_json(result_path, run_result)
        save_candidate_manifest()
    except BaseException as exc:
        if run_result["status"] != "COMPILE_FAILED":
            run_result["status"] = "INTERRUPTED" if isinstance(exc, KeyboardInterrupt) else "STOPPED"
        run_result["reason"] = str(exc) or type(exc).__name__
        # Best-effort diagnostics never remove or replace preserved inputs.
        try:
            check_preserved_inputs()
        except (OSError, RuntimeError, UnicodeError, ValueError) as preservation_error:
            run_result["preservation_check_error"] = str(preservation_error)
            print(f"PRESERVATION CHECK: {preservation_error}", file=sys.stderr, flush=True)
        try:
            save_json(result_path, run_result)
            if SKETCH.is_file() and (WORK / "PREFLIGHT_RX1024_R3.json").is_file() and (WORK / "SOURCE_DIFF_R2_TO_RX1024_R3.patch").is_file():
                save_candidate_manifest()
        except (OSError, RuntimeError, UnicodeError) as diagnostic_error:
            print(f"Could not finish diagnostics: {diagnostic_error}", file=sys.stderr, flush=True)
        try:
            make_review_zip()
        except (OSError, RuntimeError, UnicodeError, ValueError) as archive_error:
            print(f"Could not create review ZIP; staged evidence remains at {WORK}: {archive_error}", file=sys.stderr, flush=True)
        raise

    print("=== GNSS-D3B2 RX1024 R3 COMPILE PASS ===", flush=True)
    print(f"SOURCE SHA256: {candidate_hash}", flush=True)
    print(f"APPLICATION BIN SHA256: {run_result['artifact_sha256']['RID_UI_Migration.ino.bin']}", flush=True)
    print(f"BUILD: {BUILD}", flush=True)
    print(f"COMPILER LOG: {LOG}", flush=True)
    print(f"CANDIDATE MANIFEST: {WORK / 'CANDIDATE_SHA256SUMS.txt'}", flush=True)
    print("RX BUFFER REQUEST=1024 / R2 COUNTERS RETAINED / INITIALIZATION GUARDS ADDED", flush=True)
    print("NO UPLOAD / NO GITHUB CHANGE / NO ACCEPTED SOURCE EDIT", flush=True)
    make_review_zip()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preflight-only", action="store_true", help="Verify inputs, CLI selection and UART API without staging or compilation")
    args = parser.parse_args()
    try:
        main(preflight_only=args.preflight_only)
    except KeyboardInterrupt:
        print(f"STOP: Interrupted; any staged files and compiler log remain at {WORK}; NO UPLOAD", file=sys.stderr, flush=True)
        sys.exit(130)
    except (OSError, RuntimeError, UnicodeError, ValueError, subprocess.SubprocessError) as exc:
        print(f"STOP: {exc}", file=sys.stderr, flush=True)
        sys.exit(1)
