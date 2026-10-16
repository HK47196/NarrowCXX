# Narrow C++ clang-tidy checks

Check your C++ project against the [Narrow C++ draft](policy/narrow-cpp.md) during normal builds. Banned features fail the build; restricted uses produce warnings for review.

## Use in your project

Requires LLVM **23** development headers and shared Clang/LLVM libraries, matching `clang-tidy` and `llvm-config`, a C++20 compiler, Git, and a Unix host. Let your build system fetch Narrow C++ and enable it on your targets; the integrations build and load the plugin and bundled profile automatically.

### CMake

Requires CMake **3.27+** and Ninja or Makefiles. Add this to your project's `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(narrow_cpp
    GIT_REPOSITORY https://github.com/HK47196/NarrowCXX.git
    GIT_TAG master
)
FetchContent_MakeAvailable(narrow_cpp)

add_executable(app main.cpp)
narrow_enable_tidy(app)
```

The example follows `master`; use a full commit hash for `GIT_TAG` to pin your checks. For an existing target, call `narrow_enable_tidy(app)` after its definition. The [helper](cmake/NarrowTidy.cmake) replaces that target's `CXX_CLANG_TIDY` command.

Configure and build your project:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

### xmake

Requires xmake **3.1.1+**. Add this to your project's `xmake.lua`:

```lua
set_xmakever("3.1.1")
add_repositories("narrow-cpp https://github.com/HK47196/NarrowCXX.git")
add_requires("narrow-cpp master", {system = false})

target("app")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("narrow-cpp")
    add_rules("@narrow-cpp/tidy")
```

Then configure and build:

```sh
xmake f --toolchain=clang
xmake build
```

The [package recipe](packages/n/narrow-cpp/xmake.lua) fetches and builds the plugin from Git. Replace `master` in `add_requires` with a full commit hash to pin its source. The native build needs neither CMake nor Python. Use ordinary builds: `xmake check clang.tidy` does not accept the plugin's `--load` option in xmake 3.1.1.

### Use a local checkout instead

If you already keep this repository at `third_party/narrow_cpp`, replace CMake's `FetchContent` block with `add_subdirectory(third_party/narrow_cpp)`. For xmake, replace the repository/package declarations with `includes("third_party/narrow_cpp")` and use `add_rules("narrow.tidy")` without `add_packages("narrow-cpp")`.

## Understand and handle diagnostics

The bundled [.clang-tidy](.clang-tidy) enables **29 custom checks and 13 upstream checks**, promoting 19 ban checks to errors. An unsuppressed promoted diagnostic makes clang-tidy exit 1; restricted-only warnings leave it at 0. Keep the supplied severity settings: `--warnings-as-errors='*'` would also make restricted warnings fail. No fixes are applied automatically; the upstream downcast fix suggests `dynamic_cast`, which Narrow bans.

See the [check catalog](docs/custom-checks.md) for diagnostic meanings and allowed uses. Coverage is partial: a clean run does not establish policy compliance or general lifetime/bounds safety. The [coverage inventory](docs/rule-inventory.md) records tested behavior and remaining gaps. Try the checks on a representative project and inspect both errors and warnings before requiring them in CI.

### Restricted uses and local permissions

Restricted features are permitted for the policy's stated purpose, without approval for each qualifying use. Checks flag conditions they cannot establish for local review. Record a qualifying exception with the exact check name and a concrete reason:

```cpp
// NOLINTNEXTLINE(narrow-restricted-library) -- Existing external callback interface requires std::function.
std::function<void()> callback;

// NOLINTNEXTLINE(narrow-no-direct-new-delete) -- Resource boundary: ownership transfers to the external API.
return new Handle;
```

Suppressions record a claim; reviewers assess whether the policy permits it. No directory or filename grants blanket boundary/test permissions. Each independently restricted feature needs its own applicable justification. See [diagnostic name migrations](docs/custom-checks.md#diagnostic-name-migrations) when updating old exact suppressions.

Project-authored headers are checked. Classify dependencies as system/external headers, and adapt the broad header filter to your repository's source ownership. clang-tidy normally selects configuration from the main translation unit; a separate configuration beside an included adapter header is not a reliable local exception mechanism.

## Full rechecks and CI

Run the checks as a required build or CI step to block builds on bans. Use a fresh build directory in CI, or recheck **every opted-in target**:

```sh
cmake --build build --clean-first
# Or, for each xmake target:
xmake build -r app
```

CMake checks recompiled sources; policy/plugin changes alone do not recompile consumers. xmake caches successful analysis using recorded inputs. Added/shadowing headers, indirect configuration, response files, module/PCH/VFS inputs, or toolchain changes can require forced rechecks. Keep inputs stable during analysis.

## Build the plugin separately

For manual clang-tidy use or an editor integration, clone and enter the repository first:

```sh
git clone https://github.com/HK47196/NarrowCXX.git
cd NarrowCXX
```

Build the plugin from this checkout. Standalone CMake builds require CMake **3.25+**:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DNARROW_BUILD_TESTING=OFF
cmake --build build
```

For a native xmake build, run `xmake f --toolchain=clang` and `xmake build`. See [Contributing](CONTRIBUTING.md#build-and-verify) to build and run the regression tests.

### LLVM selection and platform support

Use one LLVM installation; matching major versions do not guarantee binary compatibility. For multiple installations, select CMake tools with `NARROW_CLANG_TIDY`/`NARROW_LLVM_CONFIG` and packages with `LLVM_DIR`/`Clang_DIR`; local xmake builds use `--narrow_clang_tidy`/`--narrow_llvm_config`. For the xmake package, pass `configs = {clang_tidy = "/path/to/clang-tidy", llvm_config = "/path/to/llvm-config"}` in `add_requires`.

Recorded verification is Linux with LLVM **23.1.1** and xmake **3.1.1**. macOS shared-library settings are included, but macOS and xmake-specific module/PCH/generated-source workflows remain unverified. Cross-compilation needs separate host tools and is rejected by the native xmake plugin build.

## Manual analysis and editors

First verify plugin loading and configuration, then analyze using your project's compilation database. This CMake-built plugin example uses Linux paths:

```sh
clang-tidy --verify-config \
  --config-file=/absolute/path/to/NarrowC++/.clang-tidy \
  --load=/absolute/path/to/NarrowC++/build/libNarrowTidy.so && \
clang-tidy /absolute/path/to/source.cpp -p /absolute/path/to/project/build \
  --config-file=/absolute/path/to/NarrowC++/.clang-tidy \
  --load=/absolute/path/to/NarrowC++/build/libNarrowTidy.so
```

Keep the `&&`: clang-tidy can report a plugin-loading error yet continue analysis successfully. The CMake and xmake integrations already require verification before analysis.

clang-tidy automatically discovers [.clang-tidy](.clang-tidy) for sources in this checkout, so `--config-file` can be omitted here. For another project, keep the explicit path or install the profile at that project's root. Loading the matching module with `--load` is always required; the configuration does not load it automatically.

Your application needs its own compilation database and module/BMI setup; this checkout's build database describes the plugin. An editor integration must support loading the matching external module.

## Contributing

See [Contributing](CONTRIBUTING.md) for regression tests, policy regeneration, and coverage maintenance, and the [audit](docs/audit.md) for implementation history.
