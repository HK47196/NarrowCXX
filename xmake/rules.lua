rule("narrow.tidy")
    on_load(function (target)
        assert(target:name() ~= "NarrowTidy", "Do not apply narrow.tidy to its own module")
        target:add("deps", "NarrowTidy", {inherit = false})
    end)
    before_build(function (target)
        local module = assert(target:dep("NarrowTidy"), "NarrowTidy dependency not found")
        local tidy = module:data("narrow.tidy")
        local args = {"--load=" .. path.absolute(module:targetfile()),
                      "--config-file=" .. module:data("narrow.policy")}
        -- LLVM can ignore a dlopen failure. Verify on every build, even when
        -- the consuming objects are up to date, to prevent silent loss of checks.
        os.execv(tidy, table.join(args, {"--verify-config"}))
        import("incremental", {rootdir = os.scriptdir()}).check(target, module, args)
    end)
rule_end()
