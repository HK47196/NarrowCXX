rule("tidy")
    before_build(function (target)
        import("lib.detect.find_tool")
        local package = assert(target:pkg("narrow-cpp"), 'Add add_packages("narrow-cpp") to this target')
        local tidy = assert(find_tool("clang-tidy", {
            program = package:requireconf("configs", "clang_tidy") or "clang-tidy", norun = true
        }), "clang-tidy not found")
        local root = path.join(package:installdir(), "share", "narrow-cpp")
        local extension = os.host() == "macosx" and ".dylib" or ".so"
        import("analysis", {rootdir = path.join(root, "xmake")}).check(target, {
            tidy = tidy.program,
            plugin = path.join(package:installdir(), "lib", "libNarrowTidy" .. extension),
            policy = path.join(root, ".clang-tidy")
        })
    end)
