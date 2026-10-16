# Full-policy audit — 15 October 2026

The audit uses the complete Narrow C++ draft 0.1: 57 policy blocks from Page sequence 11. At the audit date, the Page metadata was rechecked and reported sequence 11. The preserved policy and block hashes are unchanged.

Current coverage and evidence live in the generated [inventory](rule-inventory.md); diagnostic behavior and permissions live in the [check catalog](custom-checks.md). This audit retains regression history, rationale and evidence limits.

## Fixed false positives and missed cases

The regression fixtures were exercised against the previous implementation to establish failures before fixing them. The harness compares diagnostic names, locations, severity, and process status, and rejects parse failures and crashes. Allowed examples remain warning-free under the checks exercised by their suite.

| Area | Correction | Regression evidence |
| --- | --- | --- |
| Raw pointer arithmetic | Added `narrow-no-pointer-arithmetic` because the upstream check missed pointer subtraction and dependent template arithmetic. Covers addition, subtraction, compound assignments, increment/decrement, and raw pointer indexing, including resolved project instantiations. Preserves direct array and span indexing. Removed the overlapping upstream check from the combined profile. | [audit-pointer.cpp](../tests/custom/audit-pointer.cpp), [audit-upstream.cpp](../tests/integrated/audit-upstream.cpp) |
| Unchecked narrowing | Added `narrow-known-narrowing` for integral static casts whose constant operands provably do not fit. The upstream check silently accepts these explicit casts. Known-fit constants and guarded runtime casts remain permitted. | [audit-upstream.cpp](../tests/integrated/audit-upstream.cpp) |
| Dependent aliases and generic lambdas | Recognizes dependent aliases of expected, forwarding generic-lambda parameters, and `std::forward_like`. Small dependent formatter/hash specializations for project types retain their policy exception. | [audit-language.cpp](../tests/custom/audit-language.cpp), [audit-formatter.cpp](../tests/custom/audit-formatter.cpp) |
| Formatter behavior | Allows dependent formatter-base reuse. Limits the automatic format_error exception to parse/format members; unrelated helper members no longer inherit that permission. | [audit-formatter.cpp](../tests/custom/audit-formatter.cpp) |
| Unevaluated queries | Stops reporting expected value queries in decltype, sizeof, noexcept and requires contexts. Evaluated calls still direct review of ordinary propagation. | [audit-language.cpp](../tests/custom/audit-language.cpp) |
| C varargs | Finds variadic function-pointer types, indirect calls, printf calls, va_list, and va_* operations. Diagnostics from the public C macros appear at the caller's expansion site. | [audit-language.cpp](../tests/custom/audit-language.cpp) |
| Unions and extensions | Includes union forward declarations and VLA aliases/type operands, rather than only definitions and variable declarations. | [audit-language.cpp](../tests/custom/audit-language.cpp) |
| Low-level builtins | Recognizes builtin_bit_cast, whose AST is not an ordinary function call. | [low-level.cpp](../tests/custom/low-level.cpp) |
| Library identity and type propagation | Inspects recursive nested types, pointers/references, indirect/callable return types, and selected project template instantiations. Adds actual restricted list, forward_list, valarray and mdspan examples. | [audit-library.cpp](../tests/custom/audit-library.cpp) |
| C library exceptions | Keeps libc identity across project redeclarations. Permits constant-evaluated null locale queries, including typed casts. Removes bsearch from the banned operations: the policy excludes C sorting callbacks and permits other C facilities. | [audit-library.cpp](../tests/custom/audit-library.cpp) |
| Preprocessor conditions | Handles selected and skipped elifdef/elifndef and nested skipped conditionals. Recognizes guards containing comments, but checks nested reuse of a guard name. Raw string/comment contents do not become fake directives. | [audit-preprocessor.cpp](../tests/custom/audit-preprocessor.cpp), [audit-guards.hpp](../tests/custom/audit-guards.hpp), [audit-comment-guard.hpp](../tests/custom/audit-comment-guard.hpp) |

## Inventory rationale

The audit separated enforcement route, assessment and verification so a tested example no longer implies that every API or semantic clause in its row is covered. It added explicit review obligations for simplicity, local boundary permissions, preferring existing facilities, and necessary compatibility exceptions. The generated [inventory](rule-inventory.md) holds the current evidence and gaps; the [README](../README.md#maintain-the-workspace) explains regeneration and validation.

## Upstream regression follow-up

Four combined-profile fixtures were added for the ten upstream checks that previously lacked dedicated evidence. Expectations come from the Narrow policy and the upstream contracts, and include both diagnostics and permitted code. The initial run exposed profile mismatches before the options were corrected; the strengthened inventory validator also rejected the ten missing evidence entries before they were added.

| Upstream check | Verified behavior | Fixture |
| --- | --- | --- |
| `bugprone-unused-raii` | Reports a discarded lock temporary before protected work; preserves a named lock guard | [upstream-lifetimes.cpp](../tests/integrated/upstream-lifetimes.cpp) |
| `bugprone-use-after-move` | Reports dereferencing a moved-from owner; preserves reinitialization, disjoint branches, and null-state queries | [upstream-lifetimes.cpp](../tests/integrated/upstream-lifetimes.cpp) |
| `bugprone-exception-escape` | Reports visible indirect throws escaping destructors/noexcept functions; preserves noexcept cleanup and caught exceptions; tests a local suppression for intentional termination | [upstream-exceptions.cpp](../tests/integrated/upstream-exceptions.cpp) |
| `cppcoreguidelines-pro-type-const-cast` | Reports qualifier removal and addition; preserves implicit qualification conversions | [upstream-type-behavior.cpp](../tests/integrated/upstream-type-behavior.cpp) |
| `cppcoreguidelines-pro-type-reinterpret-cast` | Reports representation casts; tests a local permission for a byte boundary | [upstream-type-behavior.cpp](../tests/integrated/upstream-type-behavior.cpp) |
| `cppcoreguidelines-pro-type-static-cast-downcast` | Reports pointer/reference downcasts; preserves upcasts and ordinary numeric casts | [upstream-type-behavior.cpp](../tests/integrated/upstream-type-behavior.cpp) |
| `cppcoreguidelines-virtual-class-destructor` | Reports a public nonvirtual destructor on an abstract interface; the custom inheritance check also rejects deriving from that interface | [upstream-type-behavior.cpp](../tests/integrated/upstream-type-behavior.cpp) |
| `modernize-use-designated-initializers` | Reports positional record initialization, including single-field records; preserves designators, omitted fields, std::array and explicit constructors | [upstream-records.cpp](../tests/integrated/upstream-records.cpp) |
| `modernize-use-equals-default` | Reports empty default constructor/destructor bodies; preserves defaulted members and actual resource cleanup | [upstream-records.cpp](../tests/integrated/upstream-records.cpp) |
| `modernize-use-override` | Reports a missing override; preserves override final and virtual override spellings | [upstream-type-behavior.cpp](../tests/integrated/upstream-type-behavior.cpp) |

The combined profile now sets const-cast `StrictMode` to true because Narrow restricts every const_cast, whereas the [upstream default](https://clang.llvm.org/extra/clang-tidy/checks/cppcoreguidelines/pro-type-const-cast.html) only diagnoses qualifier removal. It sets `IgnoreSingleElementAggregates` to false so the record preference also covers the single-field case skipped by [default](https://clang.llvm.org/extra/clang-tidy/checks/modernize/use-designated-initializers.html). Both `AllowOverrideAndFinal` and `AllowVirtualAndOverride` are true: Narrow asks for override, normally final, and does not ban spelling virtual again. The [upstream defaults](https://clang.llvm.org/extra/clang-tidy/checks/modernize/use-override.html) report these otherwise compatible declarations.

Two important limits remain explicit. First, the initial run confirmed that upstream use-after-move flags a null-guarded moved-from unique_ptr dereference even though the guard prevents invalid access. That case has an exact named suppression with its validity reason; the checker remains enabled. Its analysis is [flow-sensitive rather than path-sensitive](https://clang.llvm.org/extra/clang-tidy/checks/bugprone/use-after-move.html). Second, exception-escape leaves absent dependency bodies unresolved under the current profile; the fixture labels that silence as a coverage limit. The policy also allows noexcept when escaping exceptions must terminate, so that valid use has its own local suppression rather than an assertion that it cannot throw.

The [downcast check](https://clang.llvm.org/extra/clang-tidy/checks/cppcoreguidelines/pro-type-static-cast-downcast.html) suggests dynamic_cast, which violates the Narrow RTTI rule. The combined profile now promotes this banned-downcast diagnostic to an error; do not apply its fix automatically. A project validity proof still needs local review. The [exception check](https://clang.llvm.org/extra/clang-tidy/checks/bugprone/exception-escape.html) excludes explicitly noexcept(false) destructors, and the [default-member check](https://clang.llvm.org/extra/clang-tidy/checks/modernize/use-equals-default.html) does not replace move-member implementations. These remain limitations, not verified policy enforcement.

## Ban errors and restricted warnings

The severity split made bans fail a required analysis step while retaining warnings for restricted uses. See the [catalog migration table](custom-checks.md#diagnostic-name-migrations) for the resulting diagnostic names and suppression changes, and the [manual analysis guide](../README.md#manual-analysis-and-editors) for required plugin verification and CI invocation.

The severity fixtures in [tests/integrated](../tests/integrated) independently specify error/warning expectations and process status: banned-only, restricted-only, mixed, exceptions-disabled, and allowed/locally suppressed cases. They demonstrated failure against the previous advisory profile before the promotion settings were added. Placement construction/functions remain warnings, while new(std::nothrow) remains an allocating-new error. The harness now checks exact severity and status as well as names/locations; a compiler parse error never counts as an expected policy failure.

## Prioritized follow-up — 16 October 2026

The focused regressions reproduced missed member C-varargs calls, builtin printf calls, glibc setjmp diagnostics, evaluated dependent expected::value calls and deduced expected template returns on LLVM/Clang/clang-tidy 23.1.1 before their repairs. Controls preserve pseudo-variadic compiler intrinsics, unevaluated queries, unrelated value methods, local boundary suppressions and dependency implementation macros. See [audit-language.cpp](../tests/custom/audit-language.cpp), [audit-library.cpp](../tests/custom/audit-library.cpp), [dependency-macros.cpp](../tests/custom/dependency-macros.cpp) and [mixed.cpp](../tests/integrated/mixed.cpp).

Global/imported C declarations and genuinely std-owned declarations were tested before consolidating byte/environment operations into one legacy-C diagnostic family. The std-owned controls reproduced duplicate cfenv diagnostics. [c-library-identity.cpp](../tests/custom/c-library-identity.cpp) also protects using declarations and unresolved lookup, which were covered by the former restricted-library traversal. The [catalog](custom-checks.md#diagnostic-name-migrations) records suppression migration.

The unusual syntax cases remain regression backlog, not a general rewrite mandate: object-qualified allocation functions, volatile rvalue-reference classification, conditional explicit handling and complex indirect-call return classification. For the latter, an unused `auto result = (*factory)()` may miss its declaration/call diagnostic, but later references to result are separately inspected by resolved type. The originally reported dependency-body/caller-argument collision in the shared LibraryCheck remains unconfirmed on LLVM 23. These findings do not establish unrestricted invisible use of excluded values.

Review of this pass reproduced macro collisions introduced by expansion-location deduplication in the expected and legacy-C checkers: two authored value calls could collapse into one diagnostic, and a hidden dependency memcpy could consume the diagnostic for an authored malloc argument. These checkers now retain distinct macro token locations while merging repeated visits to the same template use. Public setjmp diagnostics also retain the caller token location so a local permission on a multiline macro argument applies to that call. [dependency-macros.cpp](../tests/custom/dependency-macros.cpp) protects these cases; [c-library-identity.cpp](../tests/custom/c-library-identity.cpp) verifies that old restricted-library byte suppressions require migration to legacy-C.

## Integration rationale and limits

Use the [README](../README.md#run-on-a-consuming-project) for CMake/xmake setup, forced rechecks and platform/cache limits. Both adapters keep plugin-load verification unconditional because clang-tidy can continue after a loading failure. The xmake rule has a build fence to finish the plugin before consumer hooks, and streams successful warning output because quiet execution helpers would hide restrictions. Its dependency extraction uses joined cc1 arguments because ClangTool strips ordinary -MD/-MF flags; atomic cache writes and removal of previous successes before retries protect against recording failed analysis.

The [catalog limitations](custom-checks.md#verification-and-remaining-work) and per-item [inventory](rule-inventory.md) remain the current account of unproved behavior. This audit's LLVM 23.1.1 Linux results do not establish macOS, full C++26 library, contracts/reflection or import-std integration.
