set_project("neoioic-tests")
set_languages("c11")
set_targetdir(path.join(os.scriptdir(), "build/$(plat)/$(arch)/$(mode)"))
set_objectdir(path.join(os.scriptdir(), "build/.objs/$(target)/$(plat)/$(arch)/$(mode)"))
add_rules("mode.debug", "mode.release")

-- Keep tests independent of the root library and example build entries.
includes("../src")

target("robustness")
    set_kind("binary")
    add_files("robustness.c", "../src/object.c")
    add_files("../examples/multiple_inheritance/objects/general/**.c")
    add_includedirs("../include")
    add_ldflags("-Wl,--wrap=calloc", "-Wl,--wrap=free", {force = true})
    set_warnings("all")
    add_tests("default")

target("dispatch")
    set_kind("binary")
    add_files("dispatch.c")
    add_deps("neoioic")
    set_warnings("all")
    add_tests("default")
