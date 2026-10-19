"""Black-box regressions; all generated data stays in build/tests/.

Run: python tests/test_openoicc.py --openoicc build/mingw/x86_64/debug/openoicc.exe
Requires a C11 compiler on PATH (default gcc). No original oicc executable is invoked.
"""
import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
import os
import random
import re
import stat

ROOT = pathlib.Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--openoicc", required=True)
parser.add_argument("--cc", default="gcc", help="C11 compiler executable (default: gcc)")
parser.add_argument("--release", action="store_true")
args, rest = parser.parse_known_args()
EXE = (ROOT / args.openoicc).resolve()


def run(command, cwd=ROOT):
    return subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True,
                          text=True, encoding="utf-8", errors="replace", timeout=60)


def source(name, bases="NEOioic", members=""):
    return (f'#include "neoioic.h"\n{members}\n'
            f'/* <oicc> */\n/* <ibn> {bases} </ibn> */\n'
            f'/* <crt> */\nold constructor\n/* </crt> */\n/* </oicc> */\n'
            f'void tog_{name}(struct object *object) {{ (void)object; }}\n')


class OpenoiccTest(unittest.TestCase):
    def setUp(self):
        directory = ROOT / "build/tests"
        directory.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="case ", dir=directory)
        self.addCleanup(self.temp.cleanup)
        self.work = pathlib.Path(self.temp.name)

    def invoke(self, *arguments, success=True):
        result = run([EXE, *arguments])
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def write(self, name="a", bases="NEOioic", content=None):
        path = self.work / (name + ".c")
        path.write_text(source(name, bases) if content is None else content,
                        encoding="utf-8", newline="")
        return path

    def invalid(self, content, reason):
        path = self.write(content=content)
        before = path.read_bytes()
        result = self.invoke("generate", "--write", path, success=False)
        self.assertIn(reason, result.stderr)
        self.assertEqual(path.read_bytes(), before)
        self.assertFalse(list(self.work.glob("*.tmp")))

    def test_cli(self):
        self.assertIn("Usage: openoicc new", self.invoke("--help").stdout)
        self.assertEqual(self.invoke("--version").stdout.strip(), "OpenOICC 0.1.0")
        for option in ["-h", "-help"]:
            self.assertEqual(self.invoke(option).stdout, self.invoke("--help").stdout)
        for argv in [("wat",), ("generate",), ("generate", "--write"),
                     ("new", "a"), ("check", "--wat")]:
            with self.subTest(argv=argv):
                self.assertEqual(self.invoke(*argv, success=False).returncode, 2)

    def test_new(self):
        path = self.work / "boat.c"
        self.invoke("new", "boat", path)
        before = path.read_bytes()
        self.invoke("new", "boat", path, success=False)
        self.assertEqual(before, path.read_bytes())
        self.invoke("check", path)
        self.invoke("generate", "--write", path)
        self.assertIn(b"crt_boat", path.read_bytes())
        for name, filename, bases in [("9x", "9x.c", "NEOioic"),
                                      ("a", "wrong.c", "NEOioic"),
                                      ("neoioic", "neoioic.c", "NEOioic"),
                                      ("a", "a.c", "NEOioic,"),
                                      ("a", "a.c", "NEOioic */ BAD")]:
            with self.subTest(name=name, bases=bases):
                dest = self.work / filename
                self.invoke("new", name, dest, bases, success=False)
                self.assertFalse(dest.exists())

    def test_typed_node_creation(self):
        self.invoke("MO", "Demo", self.work)
        host = self.work / "mo/Demo"
        for name in ["Demo.c", "Demo.h", "ho.c", "ho.h", "se/se.h",
                     "so/so.h", "so/go/go.h", "so/ao/ao.h", "so/ao/Main/Main.c"]:
            self.assertTrue((host / name).is_file(), name)
            self.assertNotIn(b"\r", (host / name).read_bytes())
        self.invoke("GO", "car", host)
        self.invoke("ao", "worker", host)
        self.assertTrue((host / "so/go/car/car.h").is_file())
        self.invoke(host)
        snapshots = {p: p.read_bytes() for p in host.rglob("*") if p.is_file()}
        for kind, name in [("go", "CAR"), ("ao", "car"), ("go", "Demo"),
                           ("go", "ho"), ("go", "SO"), ("go", "NEOioic")]:
            self.invoke(kind, name, host, success=False)
        self.assertEqual(snapshots, {p: p.read_bytes() for p in host.rglob("*") if p.is_file()})
        self.invoke("mo", "Demo", self.work, success=False)
        for kind in ["co", "ro"]:
            self.invoke(kind, "module", self.work)
            self.assertTrue((self.work / kind / "module/ho.c").exists())
            self.assertFalse((self.work / kind / "module/so/ao/Main").exists())
        result = run([EXE, "mo", "Second"], cwd=self.work / "mo")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.work / "mo/Second/Second.c").exists())
        self.invoke("mo", "Third", str(self.work / "mo") + "/")
        self.assertTrue((self.work / "mo/Third/Third.c").exists())

    def test_typed_host_lifecycle(self):
        self.invoke("co", "Demo", self.work)
        host = self.work / "co/Demo"
        self.invoke("go", "car", host)
        header = host / "ho.h"
        header.write_text(header.read_text(encoding="utf-8").replace(
            "#endif", "#define OID_HO (OID_OS | UINT64_C(0x20000))\n"
            "#define OID_ (OID_HO | UINT64_C(0xfff9))\n#define MSN_CAR MSN_\n#endif"),
            encoding="utf-8")
        source = host / "ho.c"
        source.write_text(source.read_text(encoding="utf-8").replace(
            "struct object *created;", "extern int crt_car(uint64_t *, unsigned int, "
            "unsigned int, int, struct object **, unsigned char *);\n\tstruct object *created;").replace(
            "switch (model_id) {", "switch (model_id) {\n\tcase MSN_CAR:\n"
            "\t\tresult = crt_car(&oid, input_capacity, output_capacity, caller_capacity, "
            "object_out, exotic_memory);\n\t\tbreak;"), encoding="utf-8")
        self.invoke(host)
        executable = self.work / "host.exe"
        self.compile(executable, [*host.rglob("*.c"), ROOT / "tests/scaffold.c",
                                  *list((ROOT.parent / "neoioic/src").glob("*.c"))],
                     ["-I", host])
        result = run([executable])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_directory_targets_and_stale_constructors(self):
        a, b = self.write("a"), self.write("b", "a")
        self.invoke("generate", "--write", a, b)
        host = self.work / "ho.c"
        text = ('#include "neoioic.h"\n'
                'int host_create_object(void) {\n'
                'extern int crt_a(void);\n'
                '/* crt_a(); */\nconst char *s = "crt_a()";\n'
                'return crt_b();\n}\n'
                'int unrelated(void) { return crt_a(); }\n')
        host.write_text(text, encoding="utf-8")
        (self.work / "unmarked.c").write_text('const char *s = "<oicc>";\n')
        self.invoke(self.work)
        self.assertNotIn("int crt_a", a.read_text())
        self.assertIn("int crt_b", b.read_text())
        self.assertEqual(host.read_text(), text)
        snapshot = {p: p.read_bytes() for p in self.work.glob("*.c")}
        result = run([EXE], cwd=self.work)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(snapshot, {p: p.read_bytes() for p in self.work.glob("*.c")})
        host.write_text('int host_create_object(void) { return crt_b(crt_a()); }\n')
        self.invoke(self.work)
        self.assertIn("int crt_a", a.read_text())
        host.write_text('int host_create_object(void) { return 0; }\n')
        self.invoke(self.work)
        self.assertNotIn("int crt_b", b.read_text())
        host.rename(self.work / "host.c")
        self.invoke(self.work)

    def test_directory_errors_preserve_all_sources(self):
        a, b = self.write("a"), self.write("b", "a")
        host = self.work / "ho.c"
        for source in ['int host_create_object(void) { return crt_missing(); }\n',
                       'int other(void) { return 0; }\n',
                       'int host_create_object(void) { return crt_a(); }\n'
                       'int host_create_object(void) { return crt_b(); }\n',
                       '#if 0\nint host_create_object(void) { return crt_a(); }\n#endif\n',
                       '#define CREATE crt_a\nint host_create_object(void) { return CREATE(); }\n',
                       'int host_create_object(void) { return crt_a; }\n']:
            host.write_text(source)
            snapshot = {p: p.read_bytes() for p in self.work.glob("*.c")}
            self.invoke(self.work, success=False)
            self.assertEqual(snapshot, {p: p.read_bytes() for p in self.work.glob("*.c")})
            self.assertFalse(list(self.work.rglob("*.tmp")))
        host.write_text('int host_create_object(void) { return crt_a(); }\n')
        b.write_text(b.read_text().replace("<ibn> a", "<ibn> missing"))
        snapshot = a.read_bytes()
        self.invoke(self.work, success=False)
        self.assertEqual(snapshot, a.read_bytes())


    def test_preservation_and_idempotence(self):
        for newline in ["\n", "\r\n"]:
            with self.subTest(newline=newline):
                content = '// 中文 business\n' + source("a") + '// trailer\n'
                path = self.write(content=content.replace("\n", newline))
                before = path.read_bytes()
                self.invoke("check", path)
                self.invoke("generate", path)
                self.assertEqual(path.read_bytes(), before)
                self.invoke("generate", "--write", path)
                after = path.read_bytes()
                self.assertEqual(before.split(b"/* <crt> */")[0], after.split(b"/* <crt> */")[0])
                self.assertEqual(before.split(b"/* </crt> */")[1], after.split(b"/* </crt> */")[1])
                if newline == "\r\n":
                    self.assertNotIn(b"\n", after.replace(b"\r\n", b""))
                self.invoke("generate", "--write", path)
                self.assertEqual(after, path.read_bytes())

    def test_literals_and_comments(self):
        path = self.write(content='const char *s = "<oicc><ibn><crt></crt></ibn></oicc>";\n'
                          "int slash = '/'; /* ordinary comment */\n" + source("a"))
        self.invoke("check", path)
        self.invalid(source("a").replace("/* <crt> */", "// <crt>"), "standalone")
        self.invalid(source("a").replace("/* <crt> */", "/* x <crt> */"), "standalone")

    def test_broken_markers(self):
        variants = [source("a").replace("/* </crt> */", ""),
                    source("a") + "/* <oicc> */\n",
                    source("a").replace("<ibn> NEOioic </ibn>", "<ibn> NEOioic"),
                    source("a").replace("<ibn> NEOioic </ibn>", "<crt>"),
                    source("a").replace("/* <crt> */", "/* <crt> <crt> */")]
        for content in variants:
            with self.subTest(content=content):
                reason = "standalone" if "<crt> <crt>" in content else "ibn" if "<ibn> NEOioic */" in content else "marker"
                self.invalid(content, reason)

    def test_base_errors(self):
        for bases, reason in [("missing", "unknown"), ("a", "cycle"),
                              ("NEOioic,NEOioic", "duplicate direct"),
                              ("NEOioic,", "trailing"), ("NEOioic NEOioic", "comma"),
                              ("", "required"), ("9foo", "invalid")]:
            with self.subTest(bases=bases):
                self.invalid(source("a", bases), reason)

    def test_cycle_and_no_partial_writes(self):
        a = self.write("a", "b")
        b = self.write("b", "a")
        snapshots = [x.read_bytes() for x in (a, b)]
        self.invoke("generate", "--write", a, b, success=False)
        self.assertEqual(snapshots, [x.read_bytes() for x in (a, b)])
        b.write_text(source("b", "unknown"), encoding="utf-8")
        self.invoke("generate", "--write", a, b, success=False)
        self.assertEqual(a.read_bytes(), snapshots[0])
        self.assertFalse(list(self.work.glob("*.tmp")))

    def test_duplicate_nodes(self):
        a = self.write()
        self.invoke("check", a, a, success=False)
        directory = self.work / "other"
        directory.mkdir()
        other = directory / "A.c"
        other.write_text(source("A"), encoding="utf-8")
        result = self.invoke("check", a, other, success=False)
        self.assertIn("duplicate", result.stderr)

    def test_lexical_failures(self):
        for suffix, reason in [('/* unfinished', 'comment'), ('"unfinished', 'literal'),
                               ('\x00', 'NUL'), ('// splice \\\n', 'splicing')]:
            with self.subTest(reason=reason):
                self.invalid(source("a") + suffix, reason)
        path = self.write()
        path.write_bytes(path.read_bytes() + b"\xff")
        self.assertIn("UTF-8", self.invoke("check", path, success=False).stderr)
        self.invalid(source("a") + '// ??/\n', 'trigraph')
        self.invalid('%:if FLAG\n' + source("a") + '%:endif\n', 'digraph')

    def test_unsupported_declarations(self):
        self.invalid('#if FLAG\n' + source("a") + '#endif\n', "conditional")
        self.invalid(source("a", members='struct em_a { int x; }; struct em_a { int y; };'), "duplicate")
        self.invalid(source("a").replace("tog_a", "tog_other"), "binding")
        self.invalid(source("a", members='struct em_a __attribute__((aligned(32))) { int x; };'), "unsupported")
        self.invalid(source("a", members='#define DECL struct em_a { int x; };'), "macro")
        self.invalid(source("a", members='void local(void) { struct em_a { int x; }; }'), "unsupported")
        path = self.write()
        path.with_suffix(".h").write_text('#ifdef FLAG\nstruct em_a { int x; };\n#endif\n', encoding="utf-8")
        self.assertIn("conditional", self.invoke("check", path, success=False).stderr)
        path.with_suffix(".h").write_text('struct unrelated { int x; };\n#ifndef FEATURE\n#define FEATURE\nstruct em_a { int x; };\n#endif\n', encoding="utf-8")
        self.assertIn("conditional", self.invoke("check", path, success=False).stderr)
        path.with_suffix(".h").write_bytes(b'\xef\xbb\xbf#ifndef A_H\n#define A_H\nstruct em_a { int x; };\n#endif\n')
        self.invoke("check", path)

    def test_members_and_forward_graph(self):
        leaf = self.write("leaf", "left,right", source("leaf", "left,right", "struct em_leaf { int value; };"))
        left = self.write("left", "base")
        right = self.write("right", "base")
        base = self.write("base")
        self.invoke("generate", "--write", leaf, right, base, left)
        text = re.sub(r"\s+", " ", leaf.read_text(encoding="utf-8"))
        self.assertIn("NEOIOIC_ALIGNED_SIZE(struct em_leaf)", text)
        self.assertIn("direct_0[] = { 1, 4 }", text)
        self.assertIn("MSN_NEOIOIC, tog_neoioic, NEOIOIC_ALIGNED_SIZE(struct em_neoioic), _Alignof(struct em_neoioic), 4", text)
        before = leaf.read_bytes()
        self.invoke("generate", "--write", base, leaf, left, right)
        self.assertEqual(before, leaf.read_bytes())

    def test_deep_graph(self):
        paths = [self.write(f"n{i}", f"n{i+1}" if i < 119 else "NEOioic") for i in range(120)]
        self.invoke("check", *paths)

    def test_random_dags_and_base_order(self):
        rng = random.Random(7331)
        graph = {f"g{i}": rng.sample([f"g{j}" for j in range(i + 1, 14)] + ["NEOioic"],
                                    min(3, 14 - i)) for i in range(14)}
        paths = [self.write(name, ",".join(bases)) for name, bases in graph.items()]
        self.invoke("generate", "--write", *reversed(paths))
        for name, path in zip(graph, paths):
            expected = []
            def visit(node):
                if node in expected:
                    return
                expected.append(node)
                for base in graph.get(node, []):
                    visit(base)
            visit(name)
            code = re.sub(r"\s+", " ", path.read_text(encoding="utf-8"))
            bindings = re.findall(r"extern void tog_(\w+)", code)
            self.assertEqual(bindings, [node if node != "NEOioic" else "neoioic" for node in expected])
            seats = {name: 1}
            def depth(node, value):
                seats[node] = max(seats.get(node, 0), value)
                for base in graph.get(node, []):
                    depth(base, value + 1)
            depth(name, 1)
            for node in expected:
                spelling = "neoioic" if node == "NEOioic" else node
                match = re.search(r"\{ MSN_" + node.upper() + r", tog_" + spelling +
                                  r", .*?, (\d+), (?:direct_\d+|NULL),", code, re.S)
                self.assertIsNotNone(match, code)
                self.assertEqual(int(match.group(1)), seats[node])
        a = self.write("order_a", "g1,g2")
        b = self.write("order_b", "g2,g1")
        self.invoke("generate", "--write", a, b, *paths)
        self.assertEqual(re.findall(r"extern void tog_(\w+)", a.read_text())[1], "g1")
        self.assertEqual(re.findall(r"extern void tog_(\w+)", b.read_text())[1], "g2")

    def test_all_tool_allocation_failures(self):
        instrumented = self.work / "instrumented.exe"
        runtime = list((ROOT.parent / "neoioic/src").glob("*.c"))
        self.compile(instrumented, [*list((ROOT / "src").rglob("*.c")), *runtime,
                                   ROOT / "tests/tool_alloc.c"],
                     ["-include", ROOT / "tests/tool_alloc.h", "-I", ROOT / "tests"])
        # Exercise late failures after another file has already been prepared,
        # and the independent new-node path, not only a single minimal file.
        path = self.write()
        other = self.write("b", "a")
        header = path.with_suffix(".h")
        header.write_bytes(b'#ifndef A_H\n#define A_H\nstruct em_a { int x; };\n#endif\n')
        host = self.work / "ho.c"
        host.write_text('int host_create_object(void) { return crt_b(); }\n')
        created = self.work / "created.c"
        for command in [["generate", "--write", path, other],
                        ["new", "created", created], [self.work],
                        ["co", "Sample", self.work]]:
            snapshots = {p: p.read_bytes() for p in [path, other, host, header]}
            result = run([instrumented, *command])
            self.assertEqual(result.returncode, 0, result.stderr)
            total = int(re.search(r"test allocations: (\d+) live: 0", result.stderr).group(1))
            for p, data in snapshots.items():
                p.write_bytes(data)
            if created.exists():
                created.unlink()
            if (self.work / "co").exists():
                shutil.rmtree(self.work / "co")
            for failure in range(1, total + 1):
                env = os.environ.copy()
                env["OPENOICC_TEST_FAIL_ALLOC"] = str(failure)
                result = subprocess.run([str(instrumented), *map(str, command)],
                                        cwd=ROOT, env=env, capture_output=True, timeout=60)
                with self.subTest(command=command[0], allocation=failure):
                    self.assertIn(b"live: 0", result.stderr)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    for p, data in snapshots.items():
                        self.assertEqual(p.read_bytes(), data)
                    self.assertFalse(created.exists())
                    self.assertFalse((self.work / "co").exists())
                    self.assertFalse(list(self.work.rglob("*.tmp")))

    def test_bootstrap_regeneration(self):
        copied = self.work / "src"
        shutil.copytree(ROOT / "src", copied)
        project = copied / "objects/project.c"
        before = project.read_bytes()
        self.invoke("generate", "--write", project)
        self.assertEqual(project.read_bytes(), before)
        tool = self.work / "bootstrap.exe"
        self.compile(tool, [*copied.rglob("*.c"), *list((ROOT.parent / "neoioic/src").glob("*.c"))])
        result = run([tool, "generate", "--write", project])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(project.read_bytes(), before)

    def test_host_lifecycle_and_interfaces(self):
        executable = self.work / "host.exe"
        files = [path for path in (ROOT / "src").rglob("*.c") if path.name != "main.c"]
        self.compile(executable, [*files, *list((ROOT.parent / "neoioic/src").glob("*.c")),
                                  ROOT / "tests/host.c"], ["-I", ROOT / "src"])
        result = run([executable])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_file_errors(self):
        self.invoke("check", self.work / "missing.c", success=False)
        self.invoke("new", "a", self.work / "missing/a.c", success=False)
        path = self.write()
        path.rename(path.with_suffix(".txt"))
        self.invoke("check", path.with_suffix(".txt"), success=False)
        directory = self.work / "directory.c"
        directory.mkdir()
        self.invoke("check", directory, success=False)

    def test_constructor_layout_boundaries(self):
        executable = self.work / "construct.exe"
        self.compile(executable, [ROOT / "tests/construct.c", ROOT / "tests/alloc.c",
                                  *list((ROOT.parent / "neoioic/src").glob("*.c"))],
                     ["-I", ROOT / "tests"])
        result = run([executable])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_exclusive_temp_and_create_collisions(self):
        executable = self.work / "files.exe"
        self.compile(executable, [ROOT / "tests/files.c", ROOT / "src/support/text.c"],
                     ["-I", ROOT / "src"])
        result = run([executable], cwd=self.work)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_reserved_root_case_variants(self):
        for name in ["NEOioic", "neoioic", "NeOiOiC", "Neoioic", "neoIOIC"]:
            path = self.work / (name + '.c')
            self.invoke("new", name, path, success=False)
            self.assertFalse(path.exists())
            path.write_bytes(source(name).encode())
            before = path.read_bytes()
            result = self.invoke("generate", "--write", path, success=False)
            self.assertIn("reserved", result.stderr)
            self.assertEqual(path.read_bytes(), before)
            path.unlink()

    def test_every_truncated_prefix_is_safe(self):
        # A definition at the end ensures no prefix is a complete input.
        data = source("a").encode("utf-8")
        end = data.index(b"}") + 1
        path = self.work / "a.c"
        for length in range(end):
            with self.subTest(length=length):
                prefix = data[:length]
                path.write_bytes(prefix)
                result = self.invoke("generate", "--write", path, success=False)
                self.assertEqual(result.returncode, 1, result.stderr)
                self.assertEqual(path.read_bytes(), prefix)
                self.assertFalse(list(self.work.glob("*.tmp")))

    def test_utf8_invalid_and_valid_boundaries(self):
        invalid = [b"\x80", b"\xbf", b"\xc0\x80", b"\xc1\xbf",
                   b"\xc2", b"\xdf", b"\xe0\x9f\xbf", b"\xe0\xa0",
                   b"\xed\xa0\x80", b"\xef\xbf", b"\xf0\x8f\xbf\xbf",
                   b"\xf0\x90\x80", b"\xf4\x90\x80\x80", b"\xf5\x80\x80\x80",
                   b"\xff", b"\xc2A", b"\xe1\x80A", b"\xf1\x80\x80A"]
        valid = [b"\xc2\x80", b"\xdf\xbf", b"\xe0\xa0\x80", b"\xed\x9f\xbf",
                 b"\xee\x80\x80", b"\xef\xbf\xbf", b"\xf0\x90\x80\x80",
                 b"\xf4\x8f\xbf\xbf"]
        path = self.work / "a.c"
        for data in invalid + valid:
            with self.subTest(data=data):
                before = b"/* " + data + b" */\n" + source("a").encode()
                path.write_bytes(before)
                result = self.invoke("check", path, success=data in valid)
                if data in invalid:
                    self.assertIn("UTF-8", result.stderr)
                    header = path.with_suffix(".h")
                    path.write_bytes(source("a").encode())
                    header.write_bytes(b"/* " + data + b" */")
                    self.invoke("generate", "--write", path, success=False)
                    self.assertEqual(path.read_bytes(), source("a").encode())
                    header.unlink()
                else:
                    self.assertEqual(path.read_bytes(), before)

    def test_marker_insertions_deletions_and_permutations(self):
        tags = ["<oicc>", "<ibn>", "</ibn>", "<crt>", "</crt>", "</oicc>"]
        original = source("a")
        for tag in tags:
            for mutation in [original.replace(tag, "", 1),
                             original.replace(tag, tag + tag, 1)]:
                with self.subTest(tag=tag, mutation=mutation):
                    path = self.write(content=mutation)
                    before = path.read_bytes()
                    self.invoke("generate", "--write", path, success=False)
                    self.assertEqual(path.read_bytes(), before)
        rng = random.Random(9219)
        for _ in range(35):
            shuffled = tags.copy()
            rng.shuffle(shuffled)
            if shuffled == tags:
                continue
            text = "\n".join("/* " + tag + " */" for tag in shuffled)
            path = self.write(content=text + '\nvoid tog_a(struct object *o) {(void)o;}\n')
            before = path.read_bytes()
            self.invoke("generate", "--write", path, success=False)
            self.assertEqual(path.read_bytes(), before)

    def test_region_and_binding_scope(self):
        for content, reason in [
                ("void business(void) {\n" + source("a") + "}\n", "file scope"),
                (source("a") + "{", "braces"),
                (source("a") + "}", "braces"),
                (source("a").replace("void tog_a", "int tog_a"), "binding"),
                (source("a").replace("void tog_a", "avoid tog_a"), "binding"),
                (source("a").replace("void tog_a", "#define DECL void tog_a"), "macro binding"),
                (source("a") + "void tog_a(struct object *o) {(void)o;}\n", "binding")]:
            with self.subTest(reason=reason, content=content):
                self.invalid(content, reason)

    def test_read_chunk_and_large_source_boundaries(self):
        for size in [8190, 8191, 8192, 8193, 16383, 16384, 1024 * 1024]:
            with self.subTest(size=size):
                prefix = '/*' + 'x' * (size - 5) + '*/\n'
                path = self.write(content=prefix + source("a") + '// end without newline')
                self.invoke("generate", "--write", path)
                after = path.read_bytes()
                self.assertTrue(after.startswith(prefix.encode()))
                self.assertTrue(after.endswith(b'// end without newline'))
                self.invoke("generate", "--write", path)
                self.assertEqual(after, path.read_bytes())

    def test_bom_blank_lines_and_no_final_newline(self):
        for prefix in [b"\xef\xbb\xbf", b"\n\n", b"\xef\xbb\xbf\n\n"]:
            path = self.work / "a.c"
            path.write_bytes(prefix + source("a").rstrip().encode())
            self.invoke("generate", "--write", path)
            after = path.read_bytes()
            self.assertTrue(after.startswith(prefix))
            self.assertFalse(after.endswith(b"\n"))
            timestamp = path.stat().st_mtime_ns
            self.invoke("generate", "--write", path)
            self.assertEqual(timestamp, path.stat().st_mtime_ns)
            self.assertEqual(after, path.read_bytes())

    def test_bare_cr_is_rejected(self):
        self.invalid(source("a").replace("\n", "\r"), "bare CR")
        self.invalid(source("a") + '// comment\rint x;', "bare CR")

    def test_header_guard_edge_cases(self):
        path = self.write()
        header = path.with_suffix(".h")
        invalid = ['#ifndef X\n#define Y\n#endif\n',
                   '#ifndef X\n#define X\n', '#endif\n',
                   '#ifndef X\n#define X\n#endif\nint outside;\n',
                   '#ifndef X\n#define X\n#if F\n#endif\n#endif\n',
                   '#ifndef X\n#define X\n#else\n#endif\n']
        for content in invalid:
            with self.subTest(content=content):
                header.write_bytes(content.encode())
                before = path.read_bytes()
                self.invoke("generate", "--write", path, success=False)
                self.assertEqual(before, path.read_bytes())

    def test_new_scaffold_compiles_and_runs(self):
        path = self.work / "boat.c"
        self.invoke("new", "boat", path)
        (self.work / "host.h").write_bytes(b'#include "neoioic.h"\n#define MSN_BOAT 2\n')
        self.invoke("generate", "--write", path)
        driver = self.work / "driver.c"
        driver.write_bytes(b'#include "neoioic.h"\n'
                          b'extern int crt_boat(uint64_t*,unsigned int,unsigned int,int,struct object**,unsigned char*);\n'
                          b'int main(void) { struct object *o; uint64_t id=10; struct vr v={1,1};\n'
                          b'if(RN(crt_boat(&id,0,0,1,&o,0)))return 1;\n'
                          b'if(RN(vo_open(o,&v))||RN(vo_interact0(o,ASK_SHARED,&v))||RN(vo_close(o,&v)))return 2;\n'
                          b'free(o);return 0;}\n')
        executable = self.work / "boat.exe"
        self.compile(executable, [path, driver, *list((ROOT.parent / "neoioic/src").glob("*.c"))])
        result = run([executable])
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_generated_model_field_limits(self):
        path = self.work / "boat.c"
        self.invoke("new", "boat", path)
        self.invoke("generate", "--write", path)
        for model in [0, -1, 65536, 4294967295]:
            with self.subTest(model=model):
                (self.work / "host.h").write_bytes(f'#define MSN_BOAT {model}\n'.encode())
                result = run([args.cc, "-std=c11", "-I", ROOT / "include",
                              "-I", ROOT.parent / "neoioic/include", "-c", path,
                              "-o", self.work / "invalid.o"])
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("model id out of range", result.stderr)

    def test_generated_overaligned_member_rejected(self):
        path = self.work / "boat.c"
        self.invoke("new", "boat", path)
        (self.work / "host.h").write_bytes(b'#include "boat.h"\n#define MSN_BOAT 2\n')
        (self.work / "boat.h").write_bytes(b'struct em_boat { _Alignas(64) int value; };\n')
        self.invoke("generate", "--write", path)
        driver = self.work / "driver.c"
        driver.write_bytes(b'#include "neoioic.h"\n'
                          b'extern int crt_boat(uint64_t*,unsigned int,unsigned int,int,struct object**,unsigned char*);\n'
                          b'int main(void){uint64_t id=10;struct object *o=(void*)1;\n'
                          b'int r=crt_boat(&id,0,0,1,&o,0);return !(r==RN_INVALIDARG && !o && id==10);}\n')
        executable = self.work / "aligned.exe"
        self.compile(executable, [path, driver, *list((ROOT.parent / "neoioic/src").glob("*.c"))])
        result = run([executable])
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_seeded_arbitrary_byte_inputs(self):
        rng = random.Random(20261015)
        path = self.work / "a.c"
        for case in range(80):
            data = bytes(rng.randrange(256) for _ in range(rng.randrange(0, 512)))
            path.write_bytes(data)
            result = self.invoke("generate", "--write", path, success=False)
            self.assertEqual(result.returncode, 1, (case, result.stderr))
            self.assertEqual(data, path.read_bytes())
            self.assertFalse(list(self.work.glob("*.tmp")))

    def test_wide_shared_graph(self):
        branches = [self.write(f"branch{i}") for i in range(96)]
        leaf = self.write("leaf", ','.join(f"branch{i}" for i in range(96)))
        self.invoke("generate", "--write", leaf, *branches)
        code = leaf.read_text(encoding="utf-8")
        self.assertEqual(code.count("extern void tog_neoioic"), 1)
        self.assertEqual(len(re.findall(r"extern void tog_", code)), 98)
        before = leaf.read_bytes()
        self.invoke("generate", "--write", *reversed(branches), leaf)
        self.assertEqual(before, leaf.read_bytes())

    def test_random_generated_graphs_execute(self):
        # Independent Python oracle checks runtime pointers, ancestors and seats
        # on several graph shapes, including redundant direct ancestor edges.
        for seed in [7, 113, 991]:
            rng = random.Random(seed)
            count = 12
            graph = {i: rng.sample(list(range(i + 1, count + 1)),
                                   min(rng.randrange(1, 5), count - i))
                     for i in range(count)}
            def order(start):
                result = []
                def visit(i):
                    if i not in result:
                        result.append(i)
                        for base in graph.get(i, []):
                            visit(base)
                visit(start)
                return result
            paths = []
            for i, bases in graph.items():
                name = f"r{i}"
                p = self.work / f"{name}.c"
                self.invoke("new", name, p, ','.join('NEOioic' if j == count else f"r{j}" for j in bases))
                paths.append(p)
            (self.work / "host.h").write_bytes((''.join(f'#define MSN_R{i} {i+2}\n' for i in range(count))).encode())
            self.invoke("generate", "--write", *paths)
            driver = ['#include "neoioic.h"', '#include <stdio.h>',
                      '#define C(x) do { if(!(x)){fprintf(stderr,"line %d\\n",__LINE__);return 1;} }while(0)']
            driver += [f'extern int crt_r{i}(uint64_t*,unsigned int,unsigned int,int,struct object**,unsigned char*);' for i in range(count)]
            driver += ['int main(void){struct object *o; uint64_t id=1; struct vr v={1,1};']
            for start in range(count):
                nodes = order(start)
                positions = {value: index for index, value in enumerate(nodes)}
                seats = {start: 1}
                for i in range(start, count):
                    if i in seats:
                        for b in graph[i]:
                            seats[b] = max(seats.get(b, 0), seats[i] + 1)
                driver += [f'C(RP(crt_r{start}(&id,0,0,1,&o,0)));', f'C(o->node_count=={len(nodes)});']
                for position, i in enumerate(nodes):
                    model = 1 if i == count else i + 2
                    driver += [f'C(o[{position}].model_id=={model});',
                               f'C(o[{position}].caller_count=={seats[i]});',
                               f'C(o[{position}].base_count=={len(order(i))-1});']
                    for j, base in enumerate(graph.get(i, [])):
                        driver += [f'C(o[{position}].direct_bases[{j}]==o+{positions[base]});']
                    for j, base in enumerate(order(i)[1:]):
                        driver += [f'C(o[{position}].bases[{j}]==o+{positions[base]});']
                driver += ['C(RP(vo_open(o,&v))); C(RP(vo_close(o,&v)));',
                           'for(unsigned i=0;i<o->node_count;i++){C(!o[i].ref_count);',
                           'for(unsigned j=0;j<o[i].caller_count;j++)C(!o[i].callers[j].cr);}', 'free(o);']
            driver += ['return 0;}']
            driver_path = self.work / "oracle.c"
            driver_path.write_bytes(('\n'.join(driver)+'\n').encode())
            executable = self.work / "oracle.exe"
            self.compile(executable, [*paths, driver_path, *list((ROOT.parent / "neoioic/src").glob("*.c"))])
            result = run([executable])
            self.assertEqual(result.returncode, 0, (seed, result.stderr))
            for path in paths:
                path.unlink()

    @unittest.skipUnless(os.name == "nt", "Windows read-only replacement behavior")
    def test_windows_commit_failure_and_recovery(self):
        a = self.write("a")
        b = self.write("b")
        before_b = b.read_bytes()
        os.chmod(b, stat.S_IREAD)
        try:
            result = self.invoke("generate", "--write", a, b, success=False)
            self.assertIn("cannot replace file", result.stderr)
            self.assertIn("committed", result.stdout)
            self.assertIn(b"crt_a", a.read_bytes())
            self.assertEqual(b.read_bytes(), before_b)
            self.assertFalse(list(self.work.glob("*.tmp")))
        finally:
            os.chmod(b, stat.S_IWRITE)
        before_a = a.read_bytes()
        self.invoke("generate", "--write", a, b)
        self.assertEqual(a.read_bytes(), before_a)
        self.assertIn(b"crt_b", b.read_bytes())

    def compile(self, output, files, extra=()):
        result = run([args.cc, "-std=c11", "-O2" if args.release else "-O0",
                      *( ["-DNDEBUG"] if args.release else ["-g"] ),
                      "-I", ROOT.parent / "neoioic/include", "-I", ROOT / "include",
                      *extra, *files, "-o", output])
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)



if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)
