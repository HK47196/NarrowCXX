# Custom Narrow C++ checks

The LLVM 23 module registers 29 checks. The combined profile makes diagnosed bans errors and restricted uses warnings; the generated [inventory](rule-inventory.md) shows severity for every enabled check. Use [the root .clang-tidy](../.clang-tidy) with the loaded module to combine them with 13 upstream checks without duplicating equivalent constructor, allocation, bind, or C-style-cast diagnostics. Root discovery is used by the integrated suite; the custom module must still be loaded explicitly.

All checks apply to project-authored source and header uses. System-header implementation details are excluded. Types and functions are resolved through Clang declarations, including typedefs, namespace aliases, using declarations, and recognized dependent lookups. A named-module fixture verifies that an imported alias of `std::any` is diagnosed at its consumer use. A full `import std` build remains outside the verified scope.

## Diagnostic families

| Check | Implemented behavior | Important limit or permission |
| --- | --- | --- |
| `narrow-no-direct-new-delete` | Scalar/array allocating new/delete, including nothrow allocation; explicit allocation/deallocation function references | Resource/allocation adapters need a local justification; arbitrary ownership is not inferred |
| `narrow-restricted-placement-new` | Placement construction and explicit placement allocation/deallocation functions | Restricted warning; storage, alignment, lifetime, and purpose need local justification |
| `narrow-no-rtti` | `dynamic_cast` and `typeid` expressions | Keep RTTI enabled while checking; necessary compatibility uses need local permission |
| `narrow-no-logical-comma-overload` | Declarations of logical AND, OR, negation, and comma operators | General pipeline/DSL intent remains review; conventional arithmetic/equality remain allowed |
| `narrow-no-c-style-casts` | All C-style cast expressions, including numeric and void casts | No automatic rewrite; constructor intent and checked narrowing still need assessment |
| `narrow-known-narrowing` | Constant integral static_cast operands provably outside the target range | Preserves known-fit constants and runtime casts. No general runtime range proof or explicit floating/enum-cast coverage |
| `narrow-no-pointer-arithmetic` | Raw pointer addition/subtraction, compound assignment, increment/decrement and indexing, including project template instantiations | Preserves direct C-array and bounded-view indexing; does not establish their bounds |
| `narrow-inheritance` | One public, nonvirtual, data-free abstract interface base with virtual destructor and pure virtual instance methods; otherwise reports the base shape | Single public formatter reuse for a project type is allowed. External-interface adapters and unresolved dependent bases need local permission. Multiple/virtual inheritance are handled by the separate banned check |
| `narrow-no-complex-inheritance` | Multiple and virtual inheritance | Banned error; the remaining interface-shape diagnostics remain warnings |
| `narrow-restricted-unions` | Union forward declarations and named/anonymous definitions | Low-level/interoperability purpose is reviewed locally; `std::variant` is preserved |
| `narrow-explicit-conversions` | Parameterized constructors and conversion operators must be explicit | Normal copy/move and zero-parameter constructors are exempt. The meaning of a Boolean conversion is not proved |
| `narrow-restricted-globals` | Mutable namespace/static-member state and all `thread_local` declarations | Runtime/platform purpose and transitive mutability are not inferred; ordinary local variables are allowed |
| `narrow-restricted-exceptions` | Try/catch sites | Restricted warning; defined operation/application/task or test purpose needs local justification |
| `narrow-no-application-throw` | Throw sites outside the recognized formatter exception | Banned error; permits format_error in parse/format members for project types. Throwing test stubs need local justification; exception-class intent remains review |
| `narrow-exceptions-enabled` | Translation units with C++ exceptions disabled | Banned error for -fno-exceptions |
| `narrow-no-coroutines` | Spelled `co_await`, `co_yield`, `co_return` | Skips compiler-generated coroutine implementation details; library machinery is checked separately |
| `narrow-no-assumptions` | Assume attributes, builtin assume/unreachable, and `std::unreachable` references | Checked contract/fail-fast design remains the caller's responsibility |
| `narrow-restricted-c-varargs` | C variadic function types, declarations and direct/member/indirect calls, including resolved callable-template bodies and builtin library calls; `va_list`, `va_arg`, and selected va_* builtins | Excluded facility: an error despite the historical restricted check name. Required C adapters need local permission; compiler pseudo-variadic intrinsics are preserved and argument types are not proved |
| `narrow-no-extensions` | GNU statement expressions, indirect goto/label-address machinery, and VLA type spellings, including aliases and sizeof type operands | Compiler parsing remains a prerequisite; a parse error never counts as policy enforcement |
| `narrow-expected-usage` | `[[nodiscard]]` on by-value `std::expected` returns, including aliases/dependent returns and deduced instantiated template returns; reports evaluated `expected::value` calls including resolved dependent calls, preserving decltype/sizeof/noexcept/requires queries | Does not infer whether an operation should return expected, whether a value call is ordinary propagation, or whether dereference is checked. Non-propagation value uses need local justification |
| `narrow-restricted-low-level` | Selected layout attributes, restrict qualifiers, selected callable compiler builtins and builtin_bit_cast, common x86 intrinsic families | Preconditions, alignment, aliasing, and performance evidence remain review. It does not cover every layout pragma or platform intrinsic |
| `narrow-excluded-library` | Recognized dynamic-value/RTTI types, future/task/execution facilities, coroutine machinery, lazy range facilities, vector of bool, bind/placeholders, and known reclamation/reflection names | Necessary compatibility and explicitly stated boundary exceptions are local. New C++26 facility names are recognized where available; their complete library integration is not established |
| `narrow-no-sfinae` | Standard enable_if and recognized experimental detection facilities | Arbitrary user-written SFINAE and framework designs remain gaps; ordinary concepts, requires, and direct type traits are preserved |
| `narrow-restricted-library` | Known std::function/tuple, stable-container, pmr/scoped-allocation, numerical, representation/lifetime, and locale facilities | Reports the need for a qualifying purpose; does not prove that purpose. Ordinary containers and default allocators remain allowed |
| `narrow-legacy-c-library` | Known C allocation/byte operations, six cfenv mutators, and mutating setlocale; owns these operations for global C declarations, std imports and std-owned declarations | Distinguishes library identity from same-named project functions. Library redeclarations retain their identity. Constant-evaluated null-pointer locale queries, bsearch, abs and div are allowed |
| `narrow-no-unsafe-c-library` | Unchecked copying, rand/srand, C sorting callbacks and nonlocal jumps, including public glibc setjmp macro calls at the caller expansion | Banned error; required C/compatibility permissions remain local, and bsearch is preserved |
| `narrow-restricted-generics` | Parameter packs, forwarding references (including generic lambdas), forward/forward_like calls, deduction guides, decltype(auto), explicit object parameters, explicit specializations and instantiations | Permits hash/formatter class specializations for project types. Does not infer adapter size/purpose or detect all recursive metaprogramming |
| `narrow-restricted-preprocessor` | Conditional compilation directives | Exempts actual compiler-recognized header guards, including comment-bearing guards; nested reuse of guard names is checked. Skipped conditionals are checked; inactive C++ bodies are not parsed. Platform/build conditions need local permission; cross-TU consistency is not proved |
| `narrow-no-function-macros` | Active project function-like macro definitions | Banned error; qualifying compatibility/check/test macros need local justification. Inactive definitions are not scanned; other configurations require relevant build coverage |

The library classifier preserves `std::vector<int>`, `span`, `string_view`, passive `views::all`/`ref_view`/`subrange`, eager `ranges` algorithms, ordinary threads/locks/atomics, and unrelated project symbols. Nested template arguments, pointer/reference types, indirect/callable results, and selected project template instantiations are inspected too. These distinctions are exercised in the fixtures. Explicit uses of excluded aliases are diagnosed even if the alias was introduced elsewhere. Compiler-generated references, such as coroutine allocation and await machinery, are filtered out.

## Local permissions

Use an exact named suppression with a concrete reason at the qualifying use:

```cpp
// NOLINTNEXTLINE(narrow-restricted-library) -- Existing external callback interface requires std::function.
std::function<void()> callback;
```

A suppression is a diagnostic mechanism, not policy authorization. It cannot make a banned feature acceptable where the policy has no exception. No directory name grants blanket permissions, and no filename automatically grants test permissions. If an expression uses several independently restricted features, each requires its own applicable permission.

## Verification and remaining work

See the [contributor verification guide](../CONTRIBUTING.md#build-and-verify) for commands and current suite coverage, and the [README](../README.md#use-in-your-project) for CMake/xmake setup and [CI rechecks](../README.md#full-rechecks-and-ci). Every enabled check has diagnostic evidence linked from the [inventory](rule-inventory.md). The harness checks names, locations, severity and process status, rejecting compiler errors and tool failures. Fixtures verify their examples, not general policy compliance.

The current checks do not establish general lifetime/bounds safety, shared-lifetime intent, callback escape safety, nonempty owning callbacks, exception safety, task exception containment, failure-state guarantees, accurate noexcept/annotation contracts, contract predicate effects, arbitrary SFINAE/framework classification, error-combinator length, or scheduler architecture. C++26 reflection/expansion syntax and contracts need toolchain-specific work. Recognizing a facility's name is not evidence that every unavailable C++26 header or module has been integrated.

See the [full-policy audit](audit.md) for the defects fixed and the evidence-qualified [inventory](rule-inventory.md) for each policy clause.

No fixes are applied automatically. Existing upstream checks provide complementary diagnostics for casts, implicit narrowing, optional access, dangling handles, and visible exception escape. The [upstream regression follow-up](audit.md#upstream-regression-follow-up) documents policy-specific options and limits. In particular, do not blindly apply the downcast check's dynamic_cast fix: Narrow bans RTTI.

See [diagnostic name migrations](#diagnostic-name-migrations) for split diagnostic names and suppression compatibility. A build must run the combined profile as a required step to enforce its failing exit status.

## Diagnostic name migrations

The combined profile promotes bans to errors while restricted uses remain warnings. The earlier severity split moved mixed families into the following names; exact NOLINT suppressions must name the diagnostic emitted at the use:

| Previous diagnostic name | Error diagnostic | Warning diagnostic |
| --- | --- | --- |
| narrow-no-direct-new-delete | narrow-no-direct-new-delete for allocating new/delete, including nothrow new | narrow-restricted-placement-new for placement construction/functions |
| narrow-inheritance | narrow-no-complex-inheritance for multiple/virtual inheritance | narrow-inheritance for other interface-shape/boundary review |
| narrow-restricted-exceptions | narrow-no-application-throw; narrow-exceptions-enabled | narrow-restricted-exceptions for catch boundaries |
| narrow-restricted-preprocessor | narrow-no-function-macros | narrow-restricted-preprocessor for conditional compilation |
| narrow-legacy-c-library | narrow-no-unsafe-c-library for unsafe legacy APIs/nonlocal jumps | narrow-legacy-c-library for raw C allocation/bytes, locale and environment mutation |

C byte operations (`memcpy`, `memmove`, `memset`) and the six floating-environment mutators (`fesetround`, `fesetenv`, `feupdateenv`, `feclearexcept`, `feraiseexcept`, `feholdexcept`) now consistently use `narrow-legacy-c-library`. Global C declarations and declarations imported into std already used that family. Genuinely std-owned declarations previously used `narrow-restricted-library` for bytes and could emit both families for cfenv; migrate those local permissions to `narrow-legacy-c-library`. Both check names remain registered for their other facilities. A boundary using a compiler builtin can also need the independent low-level permission.

## Implementation scope

The Clang adapter shares semantic traversal through `LibraryCheck`, with independent overrides for excluded-library, SFINAE and restricted-library diagnostics. This keeps declaration/type handling consistent while allowing a local permission to identify the precise rule. C-library classification has a single owner in the legacy-C checker, including using declarations and unresolved lookups.
