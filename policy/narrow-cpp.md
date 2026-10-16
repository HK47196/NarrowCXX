DRAFT 0.1 · C++26 · 14 OCTOBER 2026

Use a small set of familiar C++ features. Make ownership, lifetime, and control flow clear. Add an abstraction only when it makes the whole program easier to understand.

# Scope and statuses

Apply this policy to all project-authored code, including application code, helpers, utilities, and boundary adapters. References to application code below include helpers and utilities; boundary adapters receive only the explicitly stated exceptions. Dependencies may use other features internally. Calling a standard container or enkiTS does not override restrictions on project-authored implementation techniques.

Language features and library facilities are allowed unless restricted or banned below.

| Status | Meaning |
| --- | --- |
| **Allowed** | Permitted when useful; not automatically preferred. Defaults are stated explicitly. |
| **Restricted** | Permitted only for the stated purpose, without approval for each qualifying use. |
| **Banned** | Not permitted in application code, except at an explicitly allowed boundary. |



**Boundary code** connects an external library, resource, platform, or diagnostic service to application code. Keep extra permissions local; a separate binary library is not required.

# Data and ownership

**Default:** values, direct ownership, and RAII for scope-exit cleanup, including exception unwinding.

| Type | Meaning |
| --- | --- |
| `T` | Value; its type manages owned resources |
| `T&`, `const T&` | Required borrow |
| `T*`, `const T*` | Optional borrow, never raw ownership |
| `std::span<T>`, `std::span<const T>` | Mutable or read-only contiguous borrow; `const span<T>` does not make elements const |
| `std::string_view` | Borrowed text; no null-termination guarantee |
| `std::unique_ptr<T>` | Exclusive indirect ownership when a value is unsuitable |
| `std::shared_ptr<T>`, `std::weak_ptr<T>` | **Restricted:** explicit shared lifetime; define who may retain objects and avoid cycles |

**Default data style:** `struct` records with public fields, `enum class` choices, and named result records for domain interfaces. Use constructors, private data, and destructors for ownership or real invariants. Avoid accessors that merely repeat field access. Small member and free functions are allowed. Initialize values before use.

Use implicit or defaulted special members when correct; write them only for ownership or invariants. Records need not have C-compatible layout. Prefer designated initialization for records, in declaration order; omitted fields follow aggregate initialization rules. For other types, choose the intended constructor rather than requiring braces everywhere.

```cpp
struct Window {
    int width;
    int height;
    bool visible;
};

Window window{
    .width = 1280,
    .height = 720,
    .visible = true
};
```

Use a named factory returning `std::expected<Resource, Error>` for fallible acquisition. Report fallible completion, such as commit or flush, through an explicit operation. Keep destructors nonthrowing.

**Restricted:** mutable globals and `thread_local` state for runtime or platform needs. Prefer explicit state parameters.

**Restricted:** borrowed members in explicit views or objects with a clear owner relationship. Stored callbacks need explicit lifetimes. Avoid views into a record's own movable storage.

**Banned:** raw owning pointers, direct `new`/`delete`, and manual allocation/free pairs, except at resource or allocation boundaries. Use RAII instead of repeated cleanup.

Keep every borrow within its owner's valid lifetime. Views do not establish bounds safety or extend lifetimes; container mutation can invalidate them. Establish validity before access.

# Functions and generics

| Feature | Conditions and defaults |
| --- | --- |
| Local `auto` | Relevant type clear from initializer; spell `auto&` or `const auto&` for borrows |
| Structured bindings | Short local decompositions; choose copy or reference deliberately and retain useful domain names |
| Lambdas | Short; `[&]` for clear synchronous nonescaping use, explicit captures for stored or scheduled work |
| Functions | Explicit public parameter/return types except simple generics; prefer direct calls and early returns |
| Simple templates | Same operation or structure across types or fixed sizes |
| Compile-time operations | `constexpr`, `static_assert`, and `constinit` are allowed; `consteval` for small necessarily compile-time operations |

## Callback types

**Callback defaults:** function pointer for captureless calls; `std::function_ref` for use only during the call; `std::move_only_function` for owning stored callbacks. A callable template parameter is allowed in a small generic algorithm.

Use `std::copyable_function` for new interfaces requiring copyable ownership; reserve `std::function` for existing-interface compatibility, including enkiTS `TaskSet` storage. `move_only_function` and `copyable_function` must contain a target before invocation.

## Generic programming

Simple constraints, direct type queries, and simple invocation remain allowed. Use standard or short custom concepts, simple `requires` expressions/clauses, and straightforward `if constexpr` branches with readable overload selection.

**Banned:** SFINAE, `enable_if`, SFINAE-based detection idioms and detection frameworks, recursive type computation, type-list, type-dispatch, or expression-template frameworks, CRTP, and template DSLs. Concepts do not make these designs acceptable.

**Restricted:** variadic templates, perfect forwarding, custom deduction guides, `decltype(auto)`, explicit object parameters, and specialization machinery are permitted only in small adapters needed to call an external API or implement a resource wrapper. Keep these features out of ordinary application interfaces; do not expose policy frameworks. **Allowed:** small `std::formatter<T>` and `std::hash<T>` specializations for project-defined types. A formatter may delegate to, or use single public inheritance from, an existing formatter solely to reuse formatting behavior.

# Type behavior and low-level code

| Status and feature | Rule |
| --- | --- |
| **Restricted** inheritance | One public data-free pure-virtual interface base with a virtual destructor; use `override`, normally `final`. Subclasses required by external-library interfaces and the narrow formatter reuse exception are allowed. No domain hierarchy from these exceptions; prefer composition. |
| **Restricted** operators | Equality, meaningful ordering, natural arithmetic, conventional container/callable operations, and conventional bitwise `enum class` flag operators. Default comparisons only when memberwise semantics fit. |
| **Restricted** conversions | Converting constructors and conversion operators must be `explicit`, except normal copy/move constructors. Explicit Boolean validity tests need one clear meaning. Prefer named functions for other conversions. |
| **Restricted** raw unions | Including tagged unions: named low-level or interoperability boundaries only; ordinary alternatives use `std::variant`. |
| **Restricted** representation access | `reinterpret_cast`, `const_cast`, placement construction, raw byte operations, and layout attributes: named low-level boundaries with stated alignment, lifetime, aliasing, and size requirements. |
| **Banned** complex inheritance | Implementation inheritance except subclasses required by external-library interfaces and narrow formatter reuse; multiple and virtual inheritance remain banned. No derivation from standard containers to add data or change behavior. |
| **Banned** hidden expression behavior | Overloaded logical/comma operators, control-flow or pipeline operator languages, and implicit conversion chains. |
| **Banned** unsafe access | C-style casts, unchecked downcasts, unchecked narrowing, and raw pointer arithmetic. Use bounded access. A narrowing cast is allowed when the value is already known to fit the target type; no redundant checked-conversion helper is required. |
| **Banned** extensions | Computed goto, variable-length arrays, and GNU statement expressions. |

# Errors and contracts

| **Condition** | **Default response** |
| --- | --- |
| Ordinary absence | Use `std::optional<T>`. |
| Ordinary operation failure | Use `std::expected<T, Error>`. Mark fallible project APIs `[[nodiscard]]`. |
| Programming mistake | Use a checked contract or the project's fail-fast check. |
| A library exception | Handle it at a defined operation, application, or task boundary. |

Prefer named error records and explicit checks before value access. Do not use `expected::value()` for ordinary propagation. Do not use error-propagation macros. Do not use long error-combinator chains.

**Exceptions stay enabled.** No `-fno-exceptions`. Do not write `throw` or project exception classes for application errors. A formatter may throw `std::format_error` when required by the formatting protocol. Catch only at defined boundaries or in tests; tests may use throwing stubs.

`expected` does not prevent allocation or dependency exceptions. Keep resources safe to unwind. Use `noexcept` only when the full operation cannot throw or escaping exceptions must terminate.

Preserve invariants on failure; RAII does not undo mutations. Build results before committing shared changes where practical. Never report unknown failure as success. Choose allocation-failure policy at a coarse boundary.

**Contracts:** `pre`, `post`, and `contract_assert` for internal obligations where supported; ordinary error results for invalid external input. Predicates must be side-effect-free, deterministic, nonthrowing, and normally nonallocating. Use body checks in virtual functions, where pre/post specifiers are unavailable.

# Modules and compiler facilities

**Default:** named modules with small coherent interfaces. Headers remain allowed when needed by the toolchain or dependencies. Do not require dependencies to provide modules.

Use `import std;` where supported; facility restrictions still apply. It does not import macros. Put C/OS and macro-dependent headers in boundary units or global module fragments.

No module-per-class organization, deep partition trees, or macros changing public types across translation units. Restrict conditional compilation to build/platform/compatibility code; keep feature checks in build/compatibility code. Application function-like macros are banned; small compatibility/check macros and test-framework macros are allowed.

| Status and facility | Rule |
| --- | --- |
| Lifetime and lock annotations | Keep annotations accurate; `lifetimebound` does not extend lifetime |
| **Restricted** effect annotations | Clang `nonallocating`/`nonblocking` only when their full contracts hold; neither implicitly specifies `noexcept` |
| **Restricted** low-level optimization | Intrinsics, checked-overflow builtins, `__restrict__`, special layout: small reviewed implementations with explicit preconditions and evidence |
| **Banned** unchecked assumptions | No `[[assume]]`, `__builtin_assume`, or `std::unreachable()` in application code |

# Standard library

Rules classify facilities, not includes. A header never overrides a feature restriction.

Use fixed-width numeric types when representation matters and they are available. Choose associative containers by lookup, ordering, and invalidation behavior. Use `std::pair` for generic protocols and named records for domain results.

Use explicit parsing and one formatting style. Output may throw. Use explicit filesystem errors. Numeric and bit facilities do not override the representation-access rules.

## Restricted facilities

Prefer an existing facility that fits the rules.

| Family | Permitted use |
| --- | --- |
| `<list>`, `<forward_list>`, `<hive>` | A concrete operation or stability need; keep local, without per-container project wrappers |
| `<tuple>` | Generic adapters or existing APIs; named domain records remain the default |
| `<memory_resource>`, `<scoped_allocator>`, and low-level allocation operations | Allocation/resource boundaries; no default allocator-aware records. From `<new>`, this restricts direct allocation/deallocation, placement construction, and allocation-handler management. Naming `std::bad_alloc` at error boundaries and using hardware-interference constants remain allowed |
| `<mdspan>`, `<linalg>` | Numerical code with explicit layout/result storage; natural operators, no expression types in general interfaces |
| `<cfenv>` | Subsystem policy with explicit floating-point environment changes |
| Custom weak-memory-order protocols and lock-free machinery | Synchronization boundaries only |
| `<exception>`, `<stdexcept>`, `<contracts>`, `<cassert>`, `<stacktrace>`, `<debugging>` | Error boundaries, tests, diagnostics, and the contract rules above; no normal exception-based API or release-check reliance on `assert` |
| Stream headers | Existing interfaces or specific subsystem needs; prefer project formatting/I/O, no application-wide stream protocol |
| `<valarray>` | A stated need; avoid overlapping numerical models |
| Global locale mutation | `std::locale::global` and mutating `setlocale` calls: explicit initialization or locale boundaries only; no global mutation in ordinary application code |
| C raw allocation and unsafe legacy operations | Raw allocation/free pairs only at resource or allocation boundaries; no unchecked string copying, `rand`, or C-style sorting callbacks. Other C-library facilities, including `std::abs` and `std::div`, remain allowed under the rules above |

## Excluded facilities

Banned in application code. Dependencies may use these internally; a necessary compatibility boundary may use only the part it requires.

| Excluded facilities | Rule or exception |
| --- | --- |
| `<any>`, `<typeinfo>`, `<typeindex>` and RTTI | No open-ended dynamic values, `dynamic_cast`, `typeid`, or runtime-type registries; use records, variants, or explicit interfaces |
| `<future>`, `<execution>` | No `async`, promises, packaged tasks, futures, execution-policy overloads, sender/receiver composition, or standard execution tasks. Use enkiTS completion and explicit result storage |
| `<coroutine>`, `<generator>` | No application coroutines or machinery; asynchronous I/O requires a separate explicit policy |
| Lazy ranges and iterator frameworks | No lazy adaptors (including `filter`/`transform`), view pipelines, custom range frameworks, or iterator frameworks. Ordinary views, passive wrappers describing existing data (`std::ranges::subrange`, `std::ranges::ref_view`), simple iterator utilities, and eager standard algorithms (including `std::ranges::sort`) remain allowed |
| `<meta>` and expansion statements | No application reflection or generic record-generation system; a small reviewed code-generation utility may use them internally |
| `<csetjmp>`, `<cstdarg>` | No nonlocal jumps or C varargs except required C adapters |
| `<hazard_pointer>`, `<rcu>` | No application reclamation model; required implementations belong in reviewed synchronization boundaries |
| Other excluded symbols | `std::vector<bool>`, `std::bind`, placeholder expressions, custom allocator policy families |

# enkiTS

Use one enkiTS scheduler for CPU task parallelism; prefer synchronous functions when parallel work is not useful. No default custom `Task<T>`, executor hierarchy, task-group framework, or second CPU task scheduler. This rule does not limit ordinary thread or synchronization use.

**Default surface:** `AddTaskSetToPipe` and `WaitforTask` for finite batches; `ITaskSet` and `TaskSetPartition` for range work; `enki::TaskSet` when its callable storage is acceptable, or a final `ITaskSet` adapter.

**No exception may escape a task callback.**

**Restricted:** pinned tasks, dependency graphs, priorities, and custom scheduler allocation require a concrete need.
