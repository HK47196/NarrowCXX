# Narrow C++ clang-tidy checks

A clang-tidy module and enforcement profile for the [Narrow C++ draft](policy/narrow-cpp.md): **29 custom checks + 13 upstream checks**.

Detected bans fail analysis; restricted uses and advisory findings remain warnings. Coverage is partial: a clean run does not establish policy compliance or general lifetime/bounds safety. Start with the [check catalog](docs/custom-checks.md), [coverage inventory](docs/rule-inventory.md), and [audit](docs/audit.md).

## Build and verify

Requires a C++20 compiler, LLVM **23** development headers and shared Clang/LLVM libraries, matching `clang-tidy` and `llvm-config`, and CMake **3.25+** or xmake **3.1.1+**. Use one LLVM installation; matching major versions do not guarantee binary compatibility. Recorded verification is Linux with LLVM **23.1.1**; macOS remains unverified. A Unix host is required; cross-compilation needs separate host tools.

For multiple LLVM installations, select CMake tools with `NARROW_CLANG_TIDY`/`NARROW_LLVM_CONFIG` and packages with `LLVM_DIR`/`Clang_DIR`; xmake uses `--narrow_clang_tidy`/`--narrow_llvm_config`.

For CMake and Ninja:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests require Python **3.10+** and cover diagnostics, severity, exit status, plugin configuration, and inventory freshness. They default on standalone, off as a dependency; change with `NARROW_BUILD_TESTING`. `-DNARROW_BUILD_PLUGIN=OFF` registers inventory-only tests without LLVM.

## Run on a consuming project

Vendor this checkout at `third_party/narrow_cpp`, then opt targets in. Both adapters build/load the plugin and bundled profile without linking it into your application.

### CMake

Requires CMake **3.27+**, Ninja or Makefiles, and an existing local compiled target:

```cmake
add_subdirectory(third_party/narrow_cpp)
add_executable(app main.cpp)
narrow_enable_tidy(app)
```

The [helper](cmake/NarrowTidy.cmake) replaces that target's `CXX_CLANG_TIDY` command. Build normally with `cmake --build build`.

### Native xmake

```lua
set_xmakever("3.1.1")
includes("third_party/narrow_cpp")

target("app")
    set_kind("binary")
    add_files("src/*.cpp")
    add_rules("narrow.tidy")
```

Run `xmake f --toolchain=clang` then `xmake build`. This [native build](xmake.lua) needs neither CMake nor Python. Use ordinary builds, not `xmake check clang.tidy`. xmake-specific module/PCH/generated-source workflows remain unverified.

### Full rechecks and CI

CMake checks recompiled sources; xmake caches successful analysis using recorded inputs. Policy/plugin changes alone do not recompile CMake consumers. Added/shadowing headers, indirect configuration, response files, module/PCH/VFS inputs, or toolchain changes can require forced rechecks. Keep inputs stable during analysis.

Use a fresh build directory in CI, or recheck **every opted-in target**:

```sh
cmake --build build --clean-first
# Or, for each xmake target:
xmake build -r app
```

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
