"""Build and self-host from a fresh source copy containing no tinycc directory."""
import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["debug", "release"], default="debug")
    args = parser.parse_args()
    output = ROOT / "build" / "independent"
    output.mkdir(parents=True, exist_ok=True)
    # A new directory prevents prior caches or binaries from supplying a
    # missing source, include, runtime or generated constructor.
    directory = Path(tempfile.mkdtemp(prefix=args.mode + "-", dir=output))
    for name in ["src", "include", "runtime", "tests"]:
        shutil.copytree(ROOT / name, directory / name,
                        ignore=shutil.ignore_patterns("__pycache__"))
    for project in ["neoioic", "openoicc"]:
        names = ["src", "include"]
        if project == "neoioic":
            names += ["tests", "examples"]
        for name in names:
            relative = Path("third_party") / project / name
            shutil.copytree(ROOT / relative, directory / relative)
    shutil.copyfile(ROOT / "xmake.lua", directory / "xmake.lua")

    def run(command):
        subprocess.run([str(item) for item in command], cwd=directory,
                       check=True, timeout=600)

    if (directory / "tinycc").exists():
        raise RuntimeError("independent source copy unexpectedly contains tinycc")
    run(["xmake", "f", "-P", ".", "-p", "mingw", "-a", "x86_64",
         "--toolchain=gcc", "-m", args.mode, "-y"])
    run(["xmake", "-P", ".", "-y"])
    run(["xmake", "test", "-P", "."])
    build = Path("build/mingw/x86_64") / args.mode
    run([sys.executable, "tests/bootstrap.py", "--build-dir", build])
    report = {"mode": args.mode, "tinycc_directory": "absent",
              "fresh_build": True, "root_tests": True,
              "stage2_stage3_bootstrap": True,
              "source_copy": directory.relative_to(ROOT).as_posix()}
    (output / (args.mode + ".json")).write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Fresh build, execution and bootstrap passed without tinycc/", flush=True)


if __name__ == "__main__":
    main()
