"""Validate staged compilation and constructor regeneration; xmake builds stage 1."""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORE = ["libtcc.c", "tccpp.c", "tccgen.c", "tccdbg.c", "tccasm.c",
        "tccelf.c", "tccrun.c", "tccpe.c", "x86_64-gen.c", "x86_64-link.c",
        "i386-asm.c"]
SOURCES = ["src/main.c", "src/host/host.c", "src/objects/source.c", "src/objects/tokens.c",
           "src/objects/preprocessor.c", "src/objects/symbols.c", "src/objects/expressions.c", "src/objects/scopes.c", "src/objects/functions.c", "src/objects/switches.c", "src/objects/initializers.c", "src/objects/temporaries.c", "src/objects/translation.c",
           *["src/compiler/" + name for name in CORE],
           "third_party/neoioic/src/object.c", "third_party/neoioic/src/neoioic.c"]
INCLUDES = ["include", "src", "src/compiler", "third_party/neoioic/include",
            "third_party/openoicc/include"]
DEFINES = ["TCC_TARGET_X86_64", "TCC_TARGET_PE"]


def run(command):
    result = subprocess.run([str(value) for value in command], cwd=ROOT,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=120)
    if result.returncode:
        raise RuntimeError(f"{command}\n{result.stdout.decode(errors='replace')}")
    return result.stdout


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compile_stage(compiler, directory, runtime):
    directory.mkdir(parents=True, exist_ok=True)
    flags = [compiler, "-B" + str(runtime), "-std=c11",
             *["-I" + value for value in INCLUDES],
             *["-D" + value for value in DEFINES]]
    objects = []
    for source in SOURCES:
        obj = directory / (source.replace("/", "_") + ".o")
        run([*flags, "-c", source, "-o", obj])
        objects.append(obj)
    program = directory / "neocc.exe"
    run([*flags, *objects, "-o", program])
    run([program, "--version"])
    return program, objects


def compile_openoicc(compiler, directory, runtime):
    sources = sorted((ROOT / "third_party/openoicc/src").rglob("*.c"))
    program = directory / "openoicc.exe"
    run([compiler, "-B" + str(runtime), "-std=c11",
         "-Ithird_party/openoicc/include", "-Ithird_party/neoioic/include",
         *sources, "third_party/neoioic/src/object.c",
         "third_party/neoioic/src/neoioic.c", "-o", program])
    run([program, "--help"])
    return program


def compile_runtime(compiler, directory, reference):
    directory.mkdir(parents=True, exist_ok=True)
    shutil.copytree(ROOT / "runtime/include", directory / "include", dirs_exist_ok=True)
    library = directory / "lib"
    library.mkdir(exist_ok=True)
    for source in (ROOT / "runtime/windows").glob("*.def"):
        shutil.copyfile(source, library / source.name)
    sources = ["runtime/common/libtcc1.c",
               *["runtime/windows/" + name + ".c" for name in
                 ["crt1", "crt1w", "wincrt1", "wincrt1w", "dllcrt1", "dllmain", "winex"]],
               "runtime/windows/chkstk.S", "runtime/common/alloca.S",
               "runtime/common/alloca-bt.S", "runtime/common/stdatomic.c",
               "runtime/common/atomic.S", "runtime/common/builtin.c"]
    objects = []
    for source in sources:
        obj = directory / (Path(source).stem + ".o")
        run([compiler, "-B" + str(directory), "-c", source, "-o", obj])
        objects.append(obj)
    run([compiler, "-ar", "rcs", library / "libtcc1.a", *objects])
    run([compiler, "-B" + str(directory), "-c", "runtime/common/runmain.c",
         "-o", library / "runmain.o"])
    for name in ["libtcc1.a", "runmain.o"]:
        if (library / name).read_bytes() != (reference / "lib" / name).read_bytes():
            raise RuntimeError("runtime stage difference: " + name)
    return directory


def regenerate(program, compiler, directory, runtime):
    names = ["source", "tokens", "preprocessor", "symbols", "expressions", "scopes", "functions", "switches", "initializers", "temporaries", "translation"]
    for name in [*[f"objects/{node}.{suffix}" for node in names
                   for suffix in ["c", "h"]], "host/host.h", "support/io.h", "support/include_path.h"]:
        target = directory / "src" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / "src" / name, target)
    sources = [directory / f"src/objects/{name}.c" for name in names]
    before = [source.read_bytes() for source in sources]
    # Preview before every explicit write, including in the disposable copy.
    for _ in range(2):
        run([program, "check", *sources])
        run([program, "generate", "--write", *sources])
        if before != [source.read_bytes() for source in sources]:
            raise RuntimeError("checked-in constructors differ from regeneration")
    for name in [*names, "inlines", "preprocessor_resources"]:
        executable = directory / f"{name}-test.exe"
        run([compiler, "-B" + str(runtime), "-std=c11", "-Iinclude", "-Isrc",
             "-Ithird_party/openoicc/include", "-Ithird_party/neoioic/include",
             *sources, f"tests/{name}.c", "src/host/host.c",
             "third_party/neoioic/src/object.c", "third_party/neoioic/src/neoioic.c",
             "-o", executable])
        run([executable])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default="build/mingw/x86_64/debug")
    args = parser.parse_args()
    build = (ROOT / args.build_dir).resolve()
    directory = build / "bootstrap"
    runtime = build / "runtime"
    print("stage 1 -> stage 2: compile NeoCC and OpenOICC", flush=True)
    stage2, objects2 = compile_stage(build / "neocc.exe", directory / "stage2", runtime)
    runtime2 = compile_runtime(stage2, directory / "stage2/runtime", runtime)
    generator2 = compile_openoicc(stage2, directory / "stage2", runtime2)
    regenerate(generator2, stage2, directory / "generated2", runtime2)
    print("stage 2 -> stage 3: recompile and regenerate", flush=True)
    stage3, objects3 = compile_stage(stage2, directory / "stage3", runtime2)
    runtime3 = compile_runtime(stage3, directory / "stage3/runtime", runtime2)
    generator3 = compile_openoicc(stage3, directory / "stage3", runtime3)
    regenerate(generator3, stage3, directory / "generated3", runtime3)
    pairs = list(zip(objects2, objects3)) + [(stage2, stage3), (generator2, generator3)]
    report = []
    for a, b in pairs:
        if a.read_bytes() != b.read_bytes():
            raise RuntimeError(f"stage difference: {a.name}")
        report.append({"file": a.name, "stage2_sha256": digest(a),
                       "stage3_sha256": digest(b)})
    for compiler in [stage2, stage3]:
        executable = compiler.parent / "preprocessor-resources.exe"
        run([compiler, "-B" + str(runtime), "-std=c11",
             "tests/cases/preprocessor_resources.c",
             "tests/cases/preprocessor_resources_other.c", "-o", executable])
        if run([executable]).replace(b"\r\n", b"\n") != (ROOT / "tests/cases/preprocessor_resources.expect").read_bytes():
            raise RuntimeError("preprocessor resource stage behavior differs")
        executable = compiler.parent / "inlines.exe"
        run([compiler, "-B" + str(runtime), "-std=c11",
             "tests/cases/inlines.c", "tests/cases/inlines_other.c",
             "-o", executable])
        if run([executable]).replace(b"\r\n", b"\n") != (ROOT / "tests/cases/inlines.expect").read_bytes():
            raise RuntimeError("inline stage behavior differs")
        for source in ["semantics", "scope", "functions", "switches", "initializers", "temporaries", "translation"]:
            executable = compiler.parent / (source + ".exe")
            run([compiler, "-B" + str(runtime), "-std=c11",
                 f"tests/cases/{source}.c", "-o", executable])
            run([executable])
    (directory / "results.json").write_text(json.dumps({
        "platform": "Windows x86-64 PE", "source_standard": "C11 features used by these sources",
        "runtime": "rebuilt at every stage; libtcc1.a and runmain.o byte-identical",
        "identical_stage_outputs": report,
        "regeneration": "stage 2 and 3 constructors unchanged; rebuilt and executed",
    }, indent=2) + "\n", encoding="utf-8")
    print(f"{len(pairs)} stage output pairs byte-identical; regenerated objects execute", flush=True)


if __name__ == "__main__":
    main()
