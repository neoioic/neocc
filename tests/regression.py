"""Compiler behavior checks. Run from any cwd; outputs stay under build/."""
import argparse
import json
import re
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir", default="build/mingw/x86_64/debug")
parser.add_argument("--cc", default="gcc")
args, rest = parser.parse_known_args()
BUILD = (ROOT / args.build_dir).resolve()
NEOCC = BUILD / "neocc.exe"
BASELINE = BUILD / "tinycc_baseline.exe"
RUNTIME = BUILD / "runtime"
OUT = BUILD / "regression"
OUT.mkdir(parents=True, exist_ok=True)
REPORT = {"passed_cases": [], "excluded_cases": {}}


def run(command, *, timeout=45, input=None, cwd=ROOT):
    return subprocess.run([str(value) for value in command], cwd=cwd,
                          input=input, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, timeout=timeout)


def output(result):
    return result.stdout.replace(b"\r\n", b"\n")


def flags(compiler):
    return [compiler, "-B" + str(RUNTIME), "-std=c11"]


class Regression(unittest.TestCase):
    def success(self, command):
        result = run(command)
        self.assertEqual(result.returncode, 0,
                         f"{command}\n{result.stdout.decode(errors='replace')}")
        return result

    def test_standard_c_and_linking(self):
        for case, sources in {
            "semantics": ["tests/cases/semantics.c"],
            "scope": ["tests/cases/scope.c"],
            "functions": ["tests/cases/functions.c"],
            "switches": ["tests/cases/switches.c"],
            "initializers": ["tests/cases/initializers.c"],
            "temporaries": ["tests/cases/temporaries.c"],
            "translation": ["tests/cases/translation.c"],
            "inlines": ["tests/cases/inlines.c", "tests/cases/inlines_other.c"],
            "link": ["tests/cases/link_a.c", "tests/cases/link_b.c"],
        }.items():
            with self.subTest(case=case):
                results = []
                for label, compiler in [("gcc", args.cc), ("baseline", BASELINE),
                                        ("neocc", NEOCC)]:
                    executable = OUT / f"{case}-{label}.exe"
                    command = [compiler, "-std=c11"] if label == "gcc" else flags(compiler)
                    self.success([*command, *sources, "-o", executable])
                    results.append(output(self.success([executable])))
                self.assertEqual(results[0], results[1])
                self.assertEqual(results[0], results[2])
                self.assertEqual(results[0],
                                 (ROOT / f"tests/cases/{case}.expect").read_bytes())
                REPORT["passed_cases"].append(case)

    def test_preprocessing_and_object_bytes(self):
        for source in ["tests/cases/preprocess.c", "tests/cases/semantics.c",
                       "tests/cases/scope.c", "tests/cases/functions.c",
                       "tests/cases/switches.c", "tests/cases/initializers.c",
                       "tests/cases/temporaries.c", "tests/cases/translation.c",
                       "tests/cases/translation_asm.S", "tests/cases/inlines.c",
                       "tests/cases/inlines_other.c"]:
            with self.subTest(source=source):
                preprocessed = []
                for compiler in (BASELINE, NEOCC):
                    preprocessed.append(output(self.success(
                        [*flags(compiler), "-E", "-P", source])))
                self.assertEqual(preprocessed[0], preprocessed[1])
                # Debug emission reads the source cursor through its context.
                for debug in ([], ["-g"], ["-gdwarf-5"]):
                    with self.subTest(debug=debug):
                        objects = []
                        for compiler in (BASELINE, NEOCC):
                            obj = OUT / f"{Path(source).stem}-{compiler.stem}.o"
                            self.success([*flags(compiler), *debug,
                                          "-c", source, "-o", obj])
                            objects.append(obj.read_bytes())
                        self.assertEqual(objects[0], objects[1])
                REPORT["passed_cases"].append(source + " preprocessing/object")

    def test_diagnostics_and_failure_exit(self):
        for command, expected in [
            (["-c", "tests/cases/error.c"], b"nested_error.h:29:"),
            (["-c", "tests/cases/temporaries_error.c"], b"missing_temporary"),
            (["-c", "tests/cases/translation_error.c"], b"missing_translation"),
            (["-c", "tests/cases/inlines_error.c"], b"missing_inline"),
            (["-c", "tests/cases/inlines_redefinition.c"], b"redefinition"),
            (["-c", "tests/cases/inlines_unfinished.c"], b"unexpected end"),
            (["-c", "tests/cases/missing.c"], b"not found"),
            (["-c", "tests/cases/semantics.c", "-o", OUT / "absent/file.o"],
             b"could not write"),
        ]:
            results = [run([*flags(compiler), *command])
                       for compiler in (BASELINE, NEOCC)]
            for result in results:
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(expected, result.stdout)
            self.assertEqual(output(results[0]), output(results[1]))

    def test_neoioic_programs(self):
        runtime_sources = ["third_party/neoioic/src/object.c",
                           "third_party/neoioic/src/neoioic.c"]
        general = sorted((ROOT / "third_party/neoioic/examples/multiple_inheritance/objects/general").glob("*/*.c"))
        programs = {
            "dispatch": ["third_party/neoioic/tests/dispatch.c", *runtime_sources],
            "source": ["tests/source.c", "src/objects/source.c",
                       "src/objects/tokens.c", "src/objects/preprocessor.c", "src/objects/symbols.c", "src/objects/expressions.c", "src/objects/scopes.c", "src/objects/functions.c", "src/objects/switches.c", "src/objects/initializers.c", "src/objects/temporaries.c", "src/objects/translation.c",
                       "src/host/host.c",
                       *runtime_sources],
            "tokens": ["tests/tokens.c", "src/objects/source.c",
                       "src/objects/tokens.c", "src/objects/preprocessor.c", "src/objects/symbols.c", "src/objects/expressions.c", "src/objects/scopes.c", "src/objects/functions.c", "src/objects/switches.c", "src/objects/initializers.c", "src/objects/temporaries.c", "src/objects/translation.c", "src/host/host.c", *runtime_sources],
            "preprocessor": ["tests/preprocessor.c", "src/objects/source.c",
                             "src/objects/tokens.c", "src/objects/preprocessor.c", "src/objects/symbols.c", "src/objects/expressions.c", "src/objects/scopes.c", "src/objects/functions.c", "src/objects/switches.c", "src/objects/initializers.c", "src/objects/temporaries.c", "src/objects/translation.c",
                             "src/host/host.c", *runtime_sources],
            "symbols": ["tests/symbols.c", "src/objects/source.c",
                        "src/objects/tokens.c", "src/objects/preprocessor.c",
                        "src/objects/symbols.c", "src/objects/expressions.c", "src/objects/scopes.c", "src/objects/functions.c", "src/objects/switches.c", "src/objects/initializers.c", "src/objects/temporaries.c", "src/objects/translation.c", "src/host/host.c", *runtime_sources],
            "expressions": ["tests/expressions.c",
                            *[str(p.relative_to(ROOT)) for p in
                              sorted((ROOT / "src/objects").glob("*.c"))],
                            "src/host/host.c", *runtime_sources],
            "scopes": ["tests/scopes.c",
                       *[str(p.relative_to(ROOT)) for p in
                         sorted((ROOT / "src/objects").glob("*.c"))],
                       "src/host/host.c", *runtime_sources],
            "functions": ["tests/functions.c",
                          *[str(p.relative_to(ROOT)) for p in
                            sorted((ROOT / "src/objects").glob("*.c"))],
                          "src/host/host.c", *runtime_sources],
            "switches": ["tests/switches.c",
                         *[str(p.relative_to(ROOT)) for p in
                           sorted((ROOT / "src/objects").glob("*.c"))],
                         "src/host/host.c", *runtime_sources],
            "initializers": ["tests/initializers.c",
                             *[str(p.relative_to(ROOT)) for p in
                               sorted((ROOT / "src/objects").glob("*.c"))],
                             "src/host/host.c", *runtime_sources],
            "temporaries": ["tests/temporaries.c",
                            *[str(p.relative_to(ROOT)) for p in
                              sorted((ROOT / "src/objects").glob("*.c"))],
                            "src/host/host.c", *runtime_sources],
            "inlines": ["tests/inlines.c",
                        *[str(path.relative_to(ROOT)) for path in
                          sorted((ROOT / "src/objects").glob("*.c"))],
                        "src/host/host.c", *runtime_sources],
            "translation": ["tests/translation.c",
                            *[str(p.relative_to(ROOT)) for p in
                              sorted((ROOT / "src/objects").glob("*.c"))],
                            "src/host/host.c", *runtime_sources],
            "directory": ["tests/directory.c", "third_party/openoicc/src/support/text.c"],
            "diamond": ["third_party/neoioic/examples/multiple_inheritance/host.c",
                        "third_party/neoioic/examples/multiple_inheritance/factory.c",
                        "third_party/neoioic/examples/multiple_inheritance/objects/application/main/main.c",
                        *general, *runtime_sources],
        }
        for name, sources in programs.items():
            with self.subTest(program=name):
                executable = OUT / f"{name}-neocc.exe"
                self.success([*flags(NEOCC), "-Iinclude", "-Isrc",
                              "-Ithird_party/openoicc/include",
                              "-Ithird_party/openoicc/src",
                              "-Ithird_party/neoioic/include", *sources,
                              "-o", executable])
                self.success([executable])
                REPORT["passed_cases"].append("NEOioic/" + name)

    def test_neoioic_fault_injection(self):
        # NeoCC's PE linker does not implement GNU ld --wrap. Rename only
        # the tested translation units, and keep real allocators separate.
        sources = ["third_party/neoioic/tests/robustness.c",
                   "third_party/neoioic/src/object.c",
                   *sorted((ROOT / "third_party/neoioic/examples/multiple_inheritance/objects/general").glob("*/*.c"))]
        objects = []
        for index, source in enumerate([*sources, "tests/neoioic_alloc.c"]):
            obj = OUT / f"robustness-{index}.o"
            rename = [] if index == len(sources) else [
                "-Dcalloc=__wrap_calloc", "-Dfree=__wrap_free"]
            self.success([*flags(NEOCC), "-Ithird_party/neoioic/include",
                          *rename, "-c", source, "-o", obj])
            objects.append(obj)
        executable = OUT / "robustness-neocc.exe"
        self.success([*flags(NEOCC), *objects, "-o", executable])
        self.success([executable])
        REPORT["passed_cases"].append("NEOioic/robustness")

    def test_tinycc_corpus(self):
        excluded = {
            "34": "upstream skips nonstandard array assignment",
            "98": "i386-only register extension", "99": "i386-only fastcall",
            "106": "pthread version symbols unavailable on Windows",
            "112": "bounds/backtrace runtime not enabled in this milestone",
            "113": "bounds/backtrace DLL runtime not enabled",
            "114": "POSIX signal/pthread bounds runtime",
            "115": "bounds runtime not enabled", "116": "bounds runtime not enabled",
            "117": "upstream excludes Windows builtins test",
            "124": "pthread atomic counter",
            "126": "bounds runtime not enabled", "132": "bounds runtime not enabled",
            "138": "ARM64 encoding", "139": "ARM64 errors", "140": "ARM64 assembly",
            "141": "RISC-V assembly", "144": "pthread TLS executable",
            "146": "TLS with external linker requires separate scenario",
            "145": "Windows ARM64 intrinsics", "148": "ELF linker symbols",
        }
        corpus = ROOT / "tinycc/tests/tests2"
        for source in sorted(corpus.glob("*.c")):
            match = re.match(r"(\d{2,3})_", source.name)
            if not match:
                continue
            number = match.group(1)
            if number in excluded:
                REPORT["excluded_cases"][source.name] = excluded[number]
                continue
            with self.subTest(source=source.name):
                extra = ["-dt"] if number in {"60", "96", "125", "128"} else []
                extra_sources = []
                if number in {"104", "120"}:
                    extra_sources = [source.with_name(source.name.replace(number, number + "+", 1))]
                parameters = ["arg1", "arg2", "arg3", "arg4", "arg5"] if number == "31" else []
                if number == "46":
                    parameters = [r"[^* ]*[:a:d: ]+\:\*-/: $", str(source)]
                results = []
                for compiler in (BASELINE, NEOCC):
                    command = [*flags(compiler), *extra, *extra_sources]
                    if number in {"42", "108", "120"}:
                        executable = OUT / f"{source.stem}-{compiler.stem}.exe"
                        self.success([*command, source, "-o", executable])
                        result = run([executable, *parameters], cwd=OUT)
                    else:
                        result = run([*command, "-run", source, *parameters], cwd=OUT)
                    results.append(result)
                self.assertEqual(results[0].returncode, results[1].returncode)
                self.assertEqual(output(results[0]), output(results[1]))
                REPORT["passed_cases"].append(source.name)


if __name__ == "__main__":
    try:
        unittest.main(argv=[__file__, *rest], verbosity=2)
    finally:
        (OUT / "results.json").write_text(json.dumps(REPORT, indent=2) + "\n",
                                           encoding="utf-8")
