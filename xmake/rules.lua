rule("narrow.tidy")
    on_load(function (target)
        assert(target:name() ~= "NarrowTidy", "Do not apply narrow.tidy to its own module")
        target:add("deps", "NarrowTidy", {inherit = false})
    end)
    before_build(function (target)
        local module = assert(target:dep("NarrowTidy"), "NarrowTidy dependency not found")
        import("analysis", {rootdir = os.scriptdir()}).check(target, {
            tidy = module:data("narrow.tidy"),
            plugin = path.absolute(module:targetfile()),
            policy = module:data("narrow.policy")
        })
    end)
rule_end()
