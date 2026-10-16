# Narrow C++ clang-tidy checks

A clang-tidy module and enforcement profile for the [Narrow C++ draft](policy/narrow-cpp.md). The module has **29 custom checks**, combined with **13 upstream checks**. Detected bans are errors; restricted uses and other advisory findings remain warnings. The checks cover documented mechanical subsets, not full policy compliance.

Start with the [check catalog](docs/custom-checks.md) for behavior, the [coverage inventory](docs/rule-inventory.md) for evidence and gaps, and the [audit](docs/audit.md) for verified limitations.

## Build and verify

The module targets LLVM **23** and was tested on Linux with LLVM/Clang/clang-tidy **23.1.1**. Building it needs the LLVM and Clang development libraries, a C++20 compiler, and either CMake 3.25+ or xmake 3.1.1+. Regression tests also need Python 3.10+. The consuming-project CMake helper requires CMake 3.27+ and a Ninja or Makefiles generator. Use the same LLVM installation for the module and clang-tidy; both builds check the major version, but patch-level binary compatibility is not promised. Ninja is used below; [native xmake instructions](#native-xmake) follow the CMake integration example.

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DLLVM_DIR="$(llvm-config --cmakedir)" \
  -DClang_DIR="$(llvm-config --cmakedir)/../clang"
cmake --build build
ctest --test-dir build --output-on-failure
```

The default configuration provides five CTest suites: custom, integrated, and module diagnostics; combined configuration; and inventory/registry validation, including generated Markdown freshness. The integrated suite includes ban/restriction severity, exit status, and local suppression cases. The diagnostic suites cover 32 translation units/interfaces and their project headers. All 42 enabled checks have diagnostic fixtures. The harness rejects missing or unexpected diagnostics, incorrect severity/status, compiler errors, and tool failures. Expected policy errors must match exactly.

For static inventory validation only, configure with `-DNARROW_BUILD_PLUGIN=OFF`; this registers one suite. It needs Python but does not build or load the module or require LLVM/clang-tidy.

Diagnostic fixtures use `-std=c++2c`, exceptions, and RTTI so the checks can inspect forbidden syntax. Dedicated negative cases disable exceptions. Module tests precompile a named interface and check an imported alias; full C++26 library, contracts, reflection, and `import std` integration remain unverified.

## Run on a consuming project

### CMake

Add this project as a build-tool dependency and opt individual targets in:

```cmake
cmake_minimum_required(VERSION 3.27)
project(MyApp LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(narrow_cpp
  SOURCE_DIR "/absolute/path/to/NarrowC++"
)
FetchContent_MakeAvailable(narrow_cpp)

add_executable(app main.cpp)
narrow_enable_tidy(app)
```

For a remote dependency, replace `SOURCE_DIR` with `GIT_REPOSITORY` and a pinned `GIT_TAG`. A vendored checkout works with `add_subdirectory(third_party/narrow_cpp)` followed by the same helper call.

The [helper](cmake/NarrowTidy.cmake) builds `NarrowTidy`, verifies that clang-tidy loaded it, and then checks the selected target's C++ sources through `CXX_CLANG_TIDY`. It supplies the generated module path and bundled policy automatically. Verification runs before compilation, including on otherwise up-to-date builds, so an ignored plugin-loading failure still fails the build. The module is a build tool and is not linked into the application. Other application targets are unchanged, and the module's own compilation does not inherit the consumer's lint command.

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

clang-tidy runs alongside compilation, so incremental builds analyze only sources that CMake recompiles. Changes to the policy contents or plugin alone do not force unchanged application sources to recompile. Newly added optional headers or headers that shadow existing includes can also require a forced recheck. Plugin verification still runs on every build, but does not analyze those sources.

In CI, use a fresh build directory or force a clean rebuild that includes every opted-in target:

```sh
cmake --build build --clean-first
```

Use the same command locally when a full recheck is needed. Keep the source tree stable while checks run.

CMake tests default **on** for a standalone build and **off** when added through `FetchContent` or `add_subdirectory`. Set `NARROW_BUILD_TESTING=ON` before adding the dependency to opt in, or `OFF` to disable standalone tests. Python is only needed when these tests are enabled. Fetching this project does not install LLVM or clang-tidy; matching LLVM 23 development tools must already be available. `NARROW_CLANG_TIDY`, `NARROW_LLVM_CONFIG`, `LLVM_DIR`, and `Clang_DIR` can select a particular installation.

The helper applies to existing local executable or compiled-library targets, requires `NARROW_BUILD_PLUGIN=ON`, and replaces the selected target's `CXX_CLANG_TIDY` command. This integration builds the plugin with the current toolchain for a native host build. Cross-compilation requires a separate host-tool setup.

### Native xmake

Vendor this checkout and include its native build definition:

```lua
set_xmakever("3.1.1")
includes("third_party/narrow_cpp")

target("app")
    set_kind("binary")
    add_files("src/*.cpp")
    add_rules("narrow.tidy")
```

Configure and build normally, for example with Clang:

```sh
xmake f --toolchain=clang --cc=/usr/bin/clang --cxx=/usr/bin/clang++ -o build/xmake
xmake build
```

The [native target](xmake.lua) builds the plugin directly; it needs neither CMake nor Python. LLVM 23 development headers, shared `clang-cpp`/LLVM libraries, and matching clang-tidy must already be installed. Select tools with `xmake f --narrow_llvm_config=/path/to/llvm-config --narrow_clang_tidy=/path/to/clang-tidy`. The plugin is a build dependency and is not linked into the application. Including the checkout registers no tests and only builds the plugin when a selected target needs it. From this checkout itself, `xmake build NarrowTidy` builds the standalone module; `xmake show -t NarrowTidy` reports its output path.

The [enforcement rule](xmake/rules.lua) loads the generated plugin and bundled policy and analyzes the selected target's C++ source batches with their target and per-file compilation options. C sources compile normally. Restricted warnings stay visible without `-v`; bans and tool failures stop the build. Other targets opt in separately.

Successful analysis is cached per translation unit using the source and transitive headers reported by clang-tidy, including system headers. Changes to those files, target/per-file compiler flags, selected compiler/tidy executables, include-path environment variables, the bundled policy, plugin, or enforcement implementation invalidate cached results. Recorded file identities include content hashes and modification times, so edits with preserved timestamps are detected. Unchanged units skip analysis without a separate preprocessing pass or LLVM library scan. Failed checks are never cached. Warnings appear when analysis runs; plugin-load verification runs on every build.

This is a local development cache with known limits. Newly added optional headers (`__has_include`), higher-priority headers that shadow existing includes, indirect inputs such as response-file contents, inherited configuration, module/PCH/VFS data, and toolchain/runtime upgrades can require a forced recheck. Inputs must remain stable while checks run.

In CI, use a fresh build directory or force fresh checks for every opted-in target:

```sh
xmake build -r app
```

When dependencies are already built and current, `xmake build -r --shallow app` rechecks every application source without rebuilding the plugin. Use the same commands locally after changing inputs outside the cache's recorded dependencies.

The native adapter was verified on Linux/Clang with xmake 3.1.1. The build includes macOS shared-library settings, but that platform and xmake-specific module/PCH/generated-source workflows remain unverified. Cross-compilation is rejected by the native plugin build; it needs a separate host-tool setup. The integration uses ordinary xmake builds; the built-in `xmake check clang.tidy` command does not accept the plugin's `--load` option in xmake 3.1.1.

### Manual analysis and editors

For manual analysis, first require successful plugin/configuration verification, then analyze with the same plugin and profile using the consuming project's compilation database:

```sh
clang-tidy --verify-config \
  --config-file=/absolute/path/to/NarrowC++/.clang-tidy \
  --load=/absolute/path/to/NarrowC++/build/libNarrowTidy.so && \
clang-tidy /absolute/path/to/source.cpp -p /absolute/path/to/project/build \
  --config-file=/absolute/path/to/NarrowC++/.clang-tidy \
  --load=/absolute/path/to/NarrowC++/build/libNarrowTidy.so
```

Keep the `&&` in manual/CI recipes: clang-tidy can report a plugin-loading error yet continue analysis successfully. The CMake and xmake adapters already require verification before analysis.

The [root enforcement profile](.clang-tidy) promotes 19 ban checks to errors. An unsuppressed promoted diagnostic makes clang-tidy exit 1; restricted-only warnings leave it at 0. Run the invocation as a required build or CI step to block a build on those failures. Keep the supplied severity settings: `--warnings-as-errors='*'` would also make restricted warnings fail. No fixes are applied automatically; the upstream downcast fix suggests `dynamic_cast`, which Narrow bans.

clang-tidy automatically discovers the root [.clang-tidy](.clang-tidy) for source files in this checkout, so `--config-file` can be omitted here. Loading the matching custom module with `--load` is still required; the configuration does not load it automatically. For a separate consuming project, keep the explicit `--config-file` above or install the profile at that project's root. The build directory's compilation database describes the plugin itself; applications need their own compilation database and module/BMI setup. An editor integration must support loading the matching external module.

## Restricted uses and local permissions

Restricted means permitted for the policy's stated purpose, without approval for each qualifying use. Checks validate the conditions they can establish and flag other uses for local review. Record a qualifying exception with an exact check name and concrete reason:

```cpp
// NOLINTNEXTLINE(narrow-restricted-library) -- Existing external callback interface requires std::function.
std::function<void()> callback;

// NOLINTNEXTLINE(narrow-no-direct-new-delete) -- Resource boundary: ownership transfers to the external API.
return new Handle;
```

Suppressions record a claim; reviewers assess whether the policy permits it. No directory or filename grants blanket boundary/test permissions. Each independently restricted feature needs its own applicable justification. The [check catalog](docs/custom-checks.md#diagnostic-name-migrations) lists diagnostic names moved between families; update old exact suppressions accordingly.

Project-authored headers are checked. Classify dependencies as system/external headers, and adapt the broad header filter to the consuming repository's source ownership. clang-tidy normally selects configuration from the main translation unit; a separate configuration beside an included adapter header is not a reliable local exception mechanism.

## Coverage and next step

The snapshot preserves 57 policy blocks and hashes from draft 0.1, Page sequence 11, retrieved 15 October 2026. [source.json](policy/source.json) stores the original blocks; [rules.json](policy/rules.json) maps 85 grouped policy items and accounts for every source block.

The [generated inventory](docs/rule-inventory.md) records item-specific fixture evidence and the remaining gaps. These are not compliance percentages. Design intent, borrow validity, shared lifetime, callback/task safety, runtime range proofs, and some C++26 integration remain incomplete.

The next useful step is a bounded trial on a representative real project. Inspect both the errors and warnings before making it a required CI step, fix concrete noise or missed cases, and add specialized analysis only when that trial demonstrates a need. Completing every inventory row is not a prerequisite for using the checks.

## Maintain the workspace

Keep policy source blocks/hashes in `policy/source.json` and enforcement mappings in `policy/rules.json`. Generate the readable policy and inventory from those files, then run verification:

```sh
python3 tools/inventory.py --write
cmake --build build
ctest --test-dir build --output-on-failure
```

CTest includes inventory validation and checks that both generated Markdown files are current. To run that validation separately, including the loaded registry:

```sh
python3 tools/inventory.py --plugin build/libNarrowTidy.so
```

`build/` contains generated build/test artifacts and is ignored; it can be removed and recreated with the build commands above.
