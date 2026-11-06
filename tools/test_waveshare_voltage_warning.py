#!/usr/bin/env python3
"""Host-only test of exact archived warning logic. Does not flash or use Arduino."""
from pathlib import Path
import hashlib, re, shutil, subprocess, tempfile, sys
EXPECTED = "2704617b96b24bc394ce4a9b1b7dbfa8b367c0e31776923e0ffd1ef026528670"
def main():
    source = Path(__file__).resolve().parents[1] / "firmware/waveshare/reference/RID_UI_Migration/RID_UI_Migration.ino"
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED:
        raise RuntimeError("Refusing to test a different source hash")
    compiler = shutil.which("g++")
    if compiler is None:
        raise RuntimeError("g++ not installed; no automatic installation attempted")
    text = raw.decode()
    parts = []
    for name in ("RidBatD4aStatus", "RidBatD4e3Warning"):
        found = re.findall(r"struct " + name + r"\s*\{.*?\};", text, re.S)
        if len(found) != 1: raise RuntimeError("Ambiguous struct " + name)
        parts.append(found[0])
    match = re.search(r"static void ridBatD4e3Step\s*\([^;{]*\)\s*\{", text)
    if not match: raise RuntimeError("Warning definition not found")
    start = match.start(); pos = match.end(); depth = 1
    while depth:
        depth += (text[pos] == "{") - (text[pos] == "}")
        pos += 1
    parts.append(text[start:pos])
    harness = r"""
unsigned checks = 0;
void require(bool condition, const char *name) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << name << "\n"; std::exit(1); }
}
RidBatD4aStatus sample(uint32_t t, uint16_t mv) {
    RidBatD4aStatus s; s.pmicOK=true; s.batteryPresent=true;
    s.voltageValid=true; s.sampleTimeMs=t; s.millivolts=mv;
    s.percentageValid=true; s.percentage=50; return s;
}
int main() {
    RidBatD4e3Warning w;
    auto feed=[&](uint32_t t,uint16_t mv){auto s=sample(t,mv);ridBatD4e3Step(w,s);};
    feed(1,3850); require(!w.low && !w.lowPending,"healthy voltage");
    feed(2500,3700); require(!w.low && w.lowPending,"inclusive activation threshold");
    feed(17499,3700); require(!w.low,"14,999 ms does not activate");
    feed(17500,3700); require(w.low,"15,000 ms activates");
    feed(18000,3799); require(w.low && !w.clearPending,"hysteresis band stays low");
    feed(20000,3800); require(w.low && w.clearPending,"inclusive recovery threshold");
    feed(49999,3800); require(w.low,"29,999 ms does not clear");
    feed(50000,3800); require(!w.low,"30,000 ms clears");
    feed(60000,3690); require(w.lowPending,"second dip starts pending");
    feed(61000,3701); require(!w.lowPending,"rise interrupts activation");
    feed(62000,3690); feed(76999,3690); require(!w.low,"new activation dwell");
    feed(77000,3690); require(w.low,"new complete activation");
    feed(78000,3800); feed(79000,3799); require(w.low && !w.clearPending,"dip interrupts recovery");
    auto s=sample(80000,3690);s.usbPresent=true;ridBatD4e3Step(w,s);
    require(!w.low && !w.lowPending && !w.clearPending,"USB clears warning state");
    w.low=true;s=sample(81000,3690);s.batteryPresent=false;ridBatD4e3Step(w,s);
    require(!w.low,"absent battery resets state");
    w.low=true;s=sample(82000,3690);s.pmicOK=false;ridBatD4e3Step(w,s);
    require(!w.low,"PMIC failure resets state");
    w.low=true;s=sample(83000,3690);s.voltageValid=false;ridBatD4e3Step(w,s);
    require(!w.low,"invalid voltage resets state");
    w=RidBatD4e3Warning();s=sample(90000,3850);s.percentage=0;ridBatD4e3Step(w,s);
    require(!w.low,"zero SOC does not trigger healthy voltage");
    s=sample(100000,3690);s.percentage=100;ridBatD4e3Step(w,s);
    s.sampleTimeMs=115000;ridBatD4e3Step(w,s);require(w.low,"high SOC does not suppress low voltage");
    w=RidBatD4e3Warning();feed(0xfffffff0U,3690);feed(uint32_t(0xfffffff0U+14999U),3690);
    require(!w.low,"activation wrap before deadline");
    feed(uint32_t(0xfffffff0U+15000U),3690);require(w.low,"activation wrap at deadline");
    w=RidBatD4e3Warning();w.low=true;feed(0xfffffff0U,3800);
    feed(uint32_t(0xfffffff0U+29999U),3800);require(w.low,"recovery wrap before deadline");
    feed(uint32_t(0xfffffff0U+30000U),3800);require(!w.low,"recovery wrap at deadline");
    std::cout << "PASS " << checks << "/" << checks << " host warning assertions\n";
}
"""
    code = "#include <cstdint>\n#include <iostream>\n#include <cstdlib>\n" + "\n".join(parts) + harness
    with tempfile.TemporaryDirectory(prefix="rid01-host-warning-") as tmp:
        cpp=Path(tmp)/"test.cpp";exe=Path(tmp)/"test";cpp.write_text(code)
        subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-pedantic",str(cpp),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    if hashlib.sha256(source.read_bytes()).hexdigest()!=EXPECTED:
        raise RuntimeError("Source changed while testing")
    print("Source SHA256:", EXPECTED)
    print("HOST LOGIC ONLY: not an ESP32 compile, ADC test, runtime timing or battery safety qualification.")
if __name__=="__main__":
    try: main()
    except (OSError,RuntimeError,subprocess.CalledProcessError) as error:
        print("ERROR:", error,file=sys.stderr);sys.exit(1)
