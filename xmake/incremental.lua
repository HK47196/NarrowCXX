-- Cache successful analysis independently of xmake's compilation cache.
function _identity(file, identities)
    file = path.absolute(file)
    if identities[file] == nil then
        identities[file] = os.isfile(file) and (tostring(os.mtime(file)) .. ":" .. hash.sha256(file)) or false
    end
    return identities[file]
end

function _matches(state, key, identities)
    if type(state) ~= "table" or state.key ~= key or type(state.files) ~= "table" or table.empty(state.files) then
        return false
    end
    for file, previous in pairs(state.files) do
        if type(file) ~= "string" or type(previous) ~= "string" or _identity(file, identities) ~= previous then
            return false
        end
    end
    return true
end

function check(target, settings, args)
    import("core.tool.compiler")
    import("core.base.json")
    import("core.base.bytes")
    local parse_deps = import("core.tools.gcc.parse_deps", {anonymous = true})
    local identities = {}
    local tidy = settings.tidy
    local program = compiler.load("cxx", {target = target}):program()
    local inputs = {4, os.curdir(), program, args}
    local cacheable = true
    local tools = {program, tidy, settings.plugin, settings.policy,
                   path.join(os.scriptdir(), "rules.lua"), path.join(os.scriptdir(), "incremental.lua"),
                   path.join(os.scriptdir(), "llvm.lua"), path.join(os.scriptdir(), "analysis.lua")}
    for _, file in ipairs(tools) do
        local identity = _identity(file, identities)
        cacheable = cacheable and identity ~= false
        table.insert(inputs, {file, identity})
    end
    for _, name in ipairs({"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "SDKROOT", "MACOSX_DEPLOYMENT_TARGET"}) do
        table.insert(inputs, {name, os.getenv(name) or ""})
    end

    local checked = {}
    for _, batch in pairs(target:sourcebatches()) do
        if batch.sourcekind == "cxx" then
            for _, sourcefile in ipairs(batch.sourcefiles) do
                if not checked[sourcefile] then
                    checked[sourcefile] = true
                    local flags = compiler.compflags(sourcefile, {target = target})
                    local key = hash.sha256(bytes(json.encode({inputs, path.absolute(sourcefile), flags})))
                    local cachefile = target:dependfile(target:objectfile(sourcefile)) .. ".narrow-tidy"
                    local state = try {function () return io.load(cachefile) end}
                    if target:is_rebuilt() or not cacheable or not _matches(state, key, identities) then
                        -- Never let a failed/interrupted retry retain an earlier success.
                        os.tryrm(cachefile)
                        os.mkdir(path.directory(cachefile))
                        local depfile = cachefile .. ".headers"
                        local temporary = cachefile .. ".tmp"
                        os.tryrm(depfile)
                        try {
                            function ()
                                cprint("${color.build.object}narrow.tidy %s", sourcefile)
                                -- ClangTool strips ordinary -MD/-MF arguments.
                                -- Joined -Xclang options reach cc1 and collect the
                                -- exact transitive inputs, including system headers.
                                local dependency_flags = {"-Xclang=-dependency-file", "-Xclang=" .. depfile,
                                                          "-Xclang=-MT", "-Xclang=narrow-tidy",
                                                          "-Xclang=-sys-header-deps"}
                                os.execv(tidy, table.join(args, {sourcefile, "--"}, flags, dependency_flags))
                                local dependencies = parse_deps(io.readfile(depfile, {continuation = "\\"}), {target = target})
                                local files = {}
                                for _, file in ipairs(dependencies) do
                                    file = path.absolute(file)
                                    files[file] = assert(_identity(file, {}), "clang-tidy dependency disappeared: " .. file)
                                end
                                assert(files[path.absolute(sourcefile)], "clang-tidy did not report its source dependency")
                                if cacheable then
                                    io.save(temporary, {key = key, files = files})
                                    os.mv(temporary, cachefile)
                                end
                            end,
                            catch {function (errors)
                                raise(errors)
                            end},
                            finally {function ()
                                os.tryrm(depfile)
                                os.tryrm(temporary)
                            end}
                        }
                    end
                end
            end
        end
    end
end
