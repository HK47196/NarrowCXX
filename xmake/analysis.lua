-- Shared entry point for checkout and installed-package rules.
function check(target, settings)
    local args = {"--load=" .. settings.plugin, "--config-file=" .. settings.policy}
    -- LLVM can ignore a dlopen failure. Verify on every build, even when
    -- the consuming objects are up to date, to prevent silent loss of checks.
    os.execv(settings.tidy, table.join(args, {"--verify-config"}))
    import("incremental", {rootdir = os.scriptdir()}).check(target, settings, args)
end
