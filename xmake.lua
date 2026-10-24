set_project("neocc")
set_languages("c11")
add_rules("mode.debug", "mode.release")
if is_mode("release") then
    add_defines("NDEBUG")
end
set_targetdir("build/$(plat)/$(arch)/$(mode)")
set_objectdir("build/.objs/$(target)/$(plat)/$(arch)/$(mode)")

includes("third_party/neoioic/src")

-- The first migration milestone validates the native x86-64 PE combination.
-- Other backends will be added with their own ABI and runtime checks.
local function compiler_settings()
    add_includedirs("src/compiler")
    add_defines("TCC_TARGET_X86_64", "TCC_TARGET_PE")
    on_load(function (target)
        assert(is_plat("mingw") and is_arch("x86_64"),
               "this milestone requires mingw/x86_64")
    end)
end

target("neocc")
    set_kind("binary")
    add_deps("neocc_compiler")
    add_files("src/main.c")
    set_rundir(".")

target("neocc_compiler")
    set_kind("static")
    compiler_settings()
    add_deps("neoioic")
    add_defines("TCC_TARGET_X86_64", "TCC_TARGET_PE",
                {public = true})
    add_includedirs("include", "src", "src/compiler",
                    "third_party/openoicc/include", {public = true})
    add_files("src/objects/*.c", "src/host/*.c")
    add_files("src/compiler/libtcc.c", "src/compiler/tccpp.c",
              "src/compiler/tccgen.c", "src/compiler/tccdbg.c",
              "src/compiler/tccasm.c", "src/compiler/tccelf.c",
              "src/compiler/tccrun.c", "src/compiler/tccpe.c",
              "src/compiler/x86_64-gen.c", "src/compiler/x86_64-link.c",
              "src/compiler/i386-asm.c")

-- The reference tree is optional for building and self-hosting NeoCC.
if os.isdir("tinycc") then
    target("tinycc_baseline")
        set_kind("binary")
        set_default(false)
        compiler_settings()
        add_files("tinycc/tcc.c")
        set_rundir(".")
end

target("openoicc")
    set_kind("binary")
    set_default(false)
    add_deps("neoioic")
    add_includedirs("third_party/openoicc/include")
    add_files("third_party/openoicc/src/**.c")
    set_rundir(".")

target("runtime_dispatch_test")
    set_kind("binary")
    add_deps("neoioic")
    add_files("third_party/neoioic/tests/dispatch.c")
    add_tests("default")

target("runtime_robustness_test")
    set_kind("binary")
    add_includedirs("third_party/neoioic/include")
    add_files("third_party/neoioic/tests/robustness.c",
              "third_party/neoioic/src/object.c")
    add_files("third_party/neoioic/examples/multiple_inheritance/objects/general/**.c")
    add_ldflags("-Wl,--wrap=calloc", "-Wl,--wrap=free", {force = true})
    add_tests("default")

target("neocc_runtime")
    set_kind("phony")
    add_deps("neocc")
    on_build(function (target)
        local compiler = target:dep("neocc"):targetfile()
        local out = path.join(path.directory(compiler), "runtime")
        os.mkdir(path.join(out, "lib"))
        os.cp("runtime/include", out)
        os.cp("runtime/windows/*.def", path.join(out, "lib"))
        local sources = {"runtime/common/libtcc1.c",
                         "runtime/windows/crt1.c", "runtime/windows/crt1w.c",
                         "runtime/windows/wincrt1.c", "runtime/windows/wincrt1w.c",
                         "runtime/windows/dllcrt1.c", "runtime/windows/dllmain.c",
                         "runtime/windows/winex.c", "runtime/windows/chkstk.S",
                         "runtime/common/alloca.S", "runtime/common/alloca-bt.S",
                         "runtime/common/stdatomic.c", "runtime/common/atomic.S",
                         "runtime/common/builtin.c"}
        local objects = {}
        local objdir = path.join(out, "obj")
        os.mkdir(objdir)
        for _, source in ipairs(sources) do
            local object = path.join(objdir, path.basename(source) .. ".o")
            os.vrunv(compiler, {"-B" .. out, "-c", source, "-o", object})
            table.insert(objects, object)
        end
        local args = {"-ar", "rcs", path.join(out, "lib/libtcc1.a")}
        table.join2(args, objects)
        os.vrunv(compiler, args)
        os.vrunv(compiler, {"-B" .. out, "-c", "runtime/common/runmain.c",
                           "-o", path.join(out, "lib/runmain.o")})
    end)

target("source_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/source.c", "src/objects/*.c", "src/host/host.c")
    set_rundir(".")
    add_tests("default")

target("source_fault_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/source_fault.c", "src/objects/*.c", "src/host/host.c")
    add_ldflags("-Wl,--wrap=calloc", "-Wl,--wrap=malloc", "-Wl,--wrap=free",
                {force = true})
    set_rundir(".")
    add_tests("default")

target("tokens_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/tokens.c", "src/objects/*.c", "src/host/host.c")
    set_rundir(".")
    add_tests("default")

target("tokens_fault_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_defines("NEOCC_TEST_TOKEN_FAULTS")
    add_files("tests/tokens.c", "src/objects/*.c", "src/host/host.c")
    add_ldflags("-Wl,--wrap=malloc", "-Wl,--wrap=calloc",
                "-Wl,--wrap=realloc", "-Wl,--wrap=free", {force = true})
    set_rundir(".")
    add_tests("default")

target("session_test")
    set_kind("binary")
    add_deps("neocc_compiler", "neocc_runtime")
    add_files("tests/session.c")
    set_rundir(".")
    on_load(function (target)
        target:add("tests", "default", {runargs = {
            path.absolute(path.join(target:targetdir(), "runtime"))
        }})
    end)

target("session_fault_test")
    set_kind("binary")
    add_deps("neocc_compiler", "neocc_runtime")
    add_files("tests/session_fault.c")
    add_ldflags("-Wl,--wrap=calloc", "-Wl,--wrap=malloc",
                "-Wl,--wrap=realloc", "-Wl,--wrap=free", {force = true})
    set_rundir(".")
    on_load(function (target)
        target:add("tests", "default", {runargs = {
            path.absolute(path.join(target:targetdir(), "runtime"))
        }})
    end)

target("directory_test")
    set_kind("binary")
    add_includedirs("third_party/openoicc/src")
    add_files("tests/directory.c", "third_party/openoicc/src/support/text.c")
    set_rundir(".")
    add_tests("default")

target("preprocessor_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/preprocessor.c", "src/objects/*.c", "src/host/host.c")
    set_rundir(".")
    add_tests("default")

for _, name in ipairs({"preprocessor_resources_test", "preprocessor_resources_fault_test"}) do
    target(name)
        set_kind("binary")
        add_deps("neoioic")
        add_includedirs("include", "src", "third_party/openoicc/include")
        add_files("tests/preprocessor_resources.c", "src/objects/*.c", "src/host/host.c")
        if name == "preprocessor_resources_fault_test" then
            add_defines("NEOCC_TEST_FAULT")
            add_ldflags("-Wl,--wrap=malloc", "-Wl,--wrap=calloc",
                        "-Wl,--wrap=realloc", "-Wl,--wrap=free", {force = true})
        end
        set_rundir(".")
        add_tests("default")
end

target("expressions_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/expressions.c", "src/objects/*.c", "src/host/host.c")
    set_rundir(".")
    add_tests("default")

target("functions_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/functions.c", "src/objects/*.c", "src/host/host.c")
    set_rundir(".")
    add_tests("default")

for _, name in ipairs({"scopes_test", "scopes_fault_test"}) do
    target(name)
        set_kind("binary")
        add_deps("neoioic")
        add_includedirs("include", "src", "third_party/openoicc/include")
        add_files("tests/scopes.c", "src/objects/*.c", "src/host/host.c")
        if name == "scopes_fault_test" then
            add_defines("NEOCC_TEST_SCOPE_FAULTS")
            add_ldflags("-Wl,--wrap=malloc", "-Wl,--wrap=calloc",
                        "-Wl,--wrap=free", {force = true})
        end
        set_rundir(".")
        add_tests("default")
end

for _, name in ipairs({"symbols_test", "symbols_fault_test"}) do
    target(name)
        set_kind("binary")
        add_deps("neoioic")
        add_includedirs("include", "src", "third_party/openoicc/include")
        add_files("tests/symbols.c", "src/objects/*.c", "src/host/host.c")
        if name == "symbols_fault_test" then
            add_defines("NEOCC_TEST_SYMBOL_FAULTS")
            add_ldflags("-Wl,--wrap=malloc", "-Wl,--wrap=calloc",
                        "-Wl,--wrap=free", {force = true})
        end
        set_rundir(".")
        add_tests("default")
end

for _, name in ipairs({"switches_test", "switches_fault_test"}) do
    target(name)
        set_kind("binary")
        add_deps("neoioic")
        add_includedirs("include", "src", "third_party/openoicc/include")
        add_files("tests/switches.c", "src/objects/*.c", "src/host/host.c")
        if name == "switches_fault_test" then
            add_defines("NEOCC_TEST_SWITCH_FAULTS")
            add_ldflags("-Wl,--wrap=malloc", "-Wl,--wrap=calloc",
                        "-Wl,--wrap=realloc", "-Wl,--wrap=free", {force = true})
        end
        set_rundir(".")
        add_tests("default")
end

target("initializers_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/initializers.c", "src/objects/*.c", "src/host/host.c")
    set_rundir(".")
    add_tests("default")

for _, fault in ipairs({false, true}) do
    target(fault and "temporaries_fault_test" or "temporaries_test")
        set_kind("binary")
        add_deps("neoioic")
        add_includedirs("include", "src", "third_party/openoicc/include")
        add_files("tests/temporaries.c", "src/objects/*.c", "src/host/host.c")
        if fault then
            add_defines("NEOCC_TEST_FAULT")
            add_ldflags("-Wl,--wrap=malloc", "-Wl,--wrap=realloc", {force = true})
        end
        set_rundir(".")
        add_tests("default")
end

target("translation_test")
    set_kind("binary")
    add_deps("neoioic")
    add_includedirs("include", "src", "third_party/openoicc/include")
    add_files("tests/translation.c", "src/objects/*.c", "src/host/host.c")
    set_rundir(".")
    add_tests("default")

for _, fault in ipairs({false, true}) do
    target(fault and "inlines_fault_test" or "inlines_test")
        set_kind("binary")
        add_deps("neoioic")
        add_includedirs("include", "src", "third_party/openoicc/include")
        add_files("tests/inlines.c", "src/objects/*.c", "src/host/host.c")
        if fault then
            add_defines("NEOCC_TEST_FAULT")
            add_ldflags("-Wl,--wrap=malloc", "-Wl,--wrap=calloc",
                        "-Wl,--wrap=realloc", "-Wl,--wrap=free", {force = true})
        end
        set_rundir(".")
        add_tests("default")
end
