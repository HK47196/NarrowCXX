#include "guarded.hpp"
#include "guarded.hpp"
#include "expression_guard.hpp"

#define APPLICATION(x) ((x) + 1) // expect: narrow-no-function-macros
#define CONSTANT 42
// NOLINTNEXTLINE(narrow-no-function-macros) -- Project fail-fast check macro.
#define CHECK(x) do { if (!(x)) __builtin_trap(); } while (false)

#if 1 // expect: narrow-restricted-preprocessor
constexpr int enabled = 1;
#elif 0 // expect: narrow-restricted-preprocessor
constexpr int enabled = 0;
#endif

#ifdef CONSTANT // expect: narrow-restricted-preprocessor
constexpr int selected = CONSTANT;
#endif

// NOLINTNEXTLINE(narrow-restricted-preprocessor) -- Platform compatibility detection.
#if defined(__linux__)
constexpr bool platform = true;
#endif
