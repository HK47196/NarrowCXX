package("narrow-cpp")
    set_homepage("https://github.com/HK47196/NarrowCXX")
    set_description("Narrow C++ clang-tidy plugin and enforcement profile")
    -- A host analysis tool: never link the plugin into the consuming program.
    set_kind("binary")
    -- Shared Clang/LLVM ABI compatibility depends on the local installation.
    set_policy("package.precompiled", false)
    set_urls("https://github.com/HK47196/NarrowCXX.git")
    add_configs("llvm_config", {description = "Matching LLVM 23 llvm-config", default = "llvm-config", type = "string"})
    add_configs("clang_tidy", {description = "Matching clang-tidy 23", default = "clang-tidy", type = "string"})

    on_install("linux", "macosx", function (package)
        import("package.tools.xmake").install(package, {
            narrow_llvm_config = package:config("llvm_config"),
            narrow_clang_tidy = package:config("clang_tidy")
        }, {targets = "NarrowTidy"})
    end)

    on_fetch(function (package, opt)
        if opt.system then
            return
        end
        local extension = package:is_plat("macosx") and ".dylib" or ".so"
        if os.isfile(package:installdir("lib", "libNarrowTidy" .. extension)) and
           os.isfile(package:installdir("share", "narrow-cpp", ".clang-tidy")) then
            return {version = package:version_str()}
        end
    end)

    on_test(function (package)
        import("lib.detect.find_tool")
        local tidy = assert(find_tool("clang-tidy", {program = package:config("clang_tidy"), norun = true}),
                            "clang-tidy not found")
        local extension = package:is_plat("macosx") and ".dylib" or ".so"
        os.vrunv(tidy.program, {
            "--load=" .. package:installdir("lib", "libNarrowTidy" .. extension),
            "--config-file=" .. package:installdir("share", "narrow-cpp", ".clang-tidy"),
            "--verify-config"
        })
    end)
