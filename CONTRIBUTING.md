# Contributing

See the [README](README.md) for build prerequisites and consuming-project setup. The [check catalog](docs/custom-checks.md) describes diagnostic behavior; the [audit](docs/audit.md) records regression history and implementation rationale.

## Build and verify

Use the CMake build to run the regression suites. Tests require Python **3.10+** in addition to the [build prerequisites](README.md#build-the-plugin-separately). Run from the repository root:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DNARROW_BUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests default on standalone and off as a dependency; change with `NARROW_BUILD_TESTING`. CTest covers diagnostics, severity, exit status, plugin configuration, and inventory freshness. The custom, integrated, and named-module suites use fixtures in `tests/custom`, `tests/integrated`, and `tests/modules`; the harness rejects compiler errors and tool failures as policy evidence.

For inventory-only validation without LLVM:

```sh
cmake -S . -B build-inventory -DNARROW_BUILD_PLUGIN=OFF -DNARROW_BUILD_TESTING=ON
ctest --test-dir build-inventory --output-on-failure
```

### Build-system integration

After changing CMake integration or xmake packaging/rules, run the consumer checks with the corresponding build tools installed (CMake uses Ninja):

```sh
python3 tools/check_integration.py --adapter cmake
python3 tools/check_integration.py --adapter xmake
python3 tools/check_integration.py --adapter xmake-checkout
```

These checks use a temporary Git snapshot of the current working files, build a consuming application, and verify allowed code, visible restricted warnings, ban failures, and verification on unchanged sources. The xmake checks also verify successful-analysis caching. The package check uses a local package repository and Git source URL; the checkout check exercises `includes` directly. All checks isolate package caches, need no network access, and remove temporary files after the run.

The xmake package recipe lives in `packages/n/narrow-cpp`; its exported `tidy` rule and the local `narrow.tidy` rule share `xmake/analysis.lua` and `xmake/incremental.lua`. Install the plugin, profile, and helper scripts together. The plugin remains a host tool and must not become an application link dependency.

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

## Policy source and coverage

The snapshot preserves 57 policy blocks and hashes from draft 0.1, Page sequence 11, retrieved 15 October 2026. [source.json](policy/source.json) stores the original blocks; [rules.json](policy/rules.json) maps 85 grouped policy items and accounts for every source block.

The [generated inventory](docs/rule-inventory.md) records item-specific fixture evidence and the remaining gaps. These are not compliance percentages. Design intent, borrow validity, shared lifetime, callback/task safety, runtime range proofs, and some C++26 integration remain incomplete.

The next useful step is a bounded trial on a representative real project. Inspect both the errors and warnings before making it a required CI step, fix concrete noise or missed cases, and add specialized analysis only when that trial demonstrates a need. Completing every inventory row is not a prerequisite for using the checks.
