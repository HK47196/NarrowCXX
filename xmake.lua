set_xmakever("3.1.1")

option("narrow_llvm_config")
    set_default("llvm-config")
    set_showmenu(true)
    set_description("llvm-config from the LLVM 23 development installation")
option_end()

option("narrow_clang_tidy")
    set_default("clang-tidy")
    set_showmenu(true)
    set_description("Matching clang-tidy 23 executable")
option_end()

includes("xmake/rules.lua")

target("NarrowTidy")
    set_kind("shared")
    set_languages("cxx20")
    set_default(os.scriptdir() == os.projectdir())
    -- Finish linking before any consuming target starts its enforcement hook.
    set_policy("build.fence", true)
    add_files("checks/NarrowTidyModule.cpp", "checks/LanguageChecks.cpp",
              "checks/LibraryChecks.cpp", "checks/GenericChecks.cpp",
              "checks/PreprocessorChecks.cpp")
    on_load(function (target)
        import("xmake.llvm", {rootdir = os.scriptdir()}).configure(target)
        target:data_set("narrow.policy", path.join(os.scriptdir(), ".clang-tidy"))
    end)
target_end()
