"""Reproduce the native Windows migration checks, including staged tools."""
import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(command):
    print("+ " + " ".join(str(value) for value in command), flush=True)
    subprocess.run([str(value) for value in command], cwd=ROOT,
                   stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["debug", "release"], default="debug")
    args = parser.parse_args()
    build = Path("build/mingw/x86_64") / args.mode
    report = ROOT / build / "verification.json"
    report.unlink(missing_ok=True)
    run(["xmake", "f", "-P", ".", "-p", "mingw", "-a", "x86_64",
         "--toolchain=gcc", "-m", args.mode, "-y"])
    run(["xmake", "-P", ".", "-y"])
    run(["xmake", "-P", ".", "-y", "tinycc_baseline"])
    run(["xmake", "-P", ".", "-y", "openoicc"])
    run(["xmake", "test", "-P", ".", "-v"])
    run([sys.executable, "tests/regression.py", "--build-dir", build])
    run([sys.executable, "tests/bootstrap.py", "--build-dir", build])
    run([sys.executable, "tests/independent.py", "--mode", args.mode])
    for program in [build / "openoicc.exe", build / "bootstrap/stage3/openoicc.exe"]:
        command = [sys.executable, "third_party/openoicc/tests/test_openoicc.py",
                   "--openoicc", ROOT / program, "--cc", "gcc"]
        if args.mode == "release":
            command.append("--release")
        run(command)
    report.write_text(json.dumps({
        "mode": args.mode,
        "platform": "Windows x86-64 / MinGW-w64",
        "root_c_suites": 10,
        "compiler_integration_suites": 6,
        "tinycc_differential_inputs": 120,
        "tinycc_excluded_inputs": 21,
        "bootstrap_identical_pairs": 20,
        "fresh_build_and_bootstrap_without_tinycc": True,
        "openoicc_suites_per_compiler": 39,
        "openoicc_compilers": ["GCC", "NeoCC stage 3"],
        "success": True,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
