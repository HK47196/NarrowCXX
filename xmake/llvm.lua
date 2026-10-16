-- Build the module against the same shared LLVM/Clang used by the tidy host.
function configure(target)
    import("core.project.config")
    import("lib.detect.find_tool")

    assert(target:is_plat("linux", "macosx") and not target:is_cross(),
           "NarrowTidy currently requires a native Linux or macOS build")
    -- The queries below validate the tools; discovery need not run them too.
    local llvm = assert(find_tool("llvm-config", {program = config.get("narrow_llvm_config") or "llvm-config", norun = true}),
                        "llvm-config not found")
    local tidy = assert(find_tool("clang-tidy", {program = config.get("narrow_clang_tidy") or "clang-tidy", norun = true}),
                        "clang-tidy not found")
    -- Batch queries to avoid paying xmake's subprocess wait overhead for each.
    -- Keep empty lines: some LLVM installations have no extra linker flags.
    local version, includedir, libdir, cxxflags, ldflags, libraries = table.unpack(
        os.iorunv(llvm.program, {"--version", "--includedir", "--libdir", "--cxxflags",
                               "--ldflags", "--link-shared", "--libfiles"})
            :split("\n", {plain = true, strict = true}))
    version = version:trim()
    local tidy_version = os.iorunv(tidy.program, {"--version"})
    assert(version:match("^23%.") and tidy_version:match("LLVM version%s+23%."),
           "NarrowTidy requires LLVM 23 and a matching clang-tidy 23 executable")

    includedir = includedir:trim()
    libdir = libdir:trim()
    local minor = version:match("^(%d+%.%d+)")
    local clang_cpp
    for _, name in ipairs({"libclang-cpp.so." .. minor, "libclang-cpp.so.23",
                           "libclang-cpp." .. minor .. ".dylib", "libclang-cpp.23.dylib",
                           "libclang-cpp.so", "libclang-cpp.dylib"}) do
        local candidate = path.join(libdir, name)
        if os.isfile(candidate) then
            clang_cpp = candidate
            break
        end
    end
    assert(clang_cpp, "Shared clang-cpp library not found in " .. libdir)
    -- Promoting the native /usr/include to an explicit -isystem path moves it
    -- before libstdc++ and breaks its #include_next <stdlib.h> on Linux.
    if includedir ~= "/usr/include" then
        target:add("sysincludedirs", includedir)
    end
    -- Keep LLVM's ABI definitions/options, but use this target's C++20 mode and
    -- system include path rather than llvm-config's older language standard.
    local skip_include = false
    for _, flag in ipairs(os.argv(cxxflags)) do
        if skip_include then
            skip_include = false
        elseif flag == "-I" then
            skip_include = true
        elseif not flag:startswith("-std=") and not flag:startswith("-I") then
            target:add("cxflags", flag, {force = true})
        end
    end
    target:add("shflags", os.argv(ldflags), {force = true})
    -- Absolute shared-library paths avoid accidentally linking static tidy
    -- archives (which would duplicate the host's checker registry).
    target:add("shflags", clang_cpp, {force = true})
    target:add("shflags", os.argv(libraries), {force = true})
    target:add("rpathdirs", libdir)
    if target:is_plat("macosx") then
        target:add("shflags", "-Wl,-undefined,dynamic_lookup", {force = true})
    end
    target:data_set("narrow.tidy", tidy.program)
end
