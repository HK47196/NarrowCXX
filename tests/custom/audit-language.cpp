#include <cstdarg>
#include <cstdio>
#include <expected>
#include <utility>

union Opaque; // expect: narrow-restricted-unions

using Callback = int (*)(int, ...); // expect: narrow-restricted-c-varargs
void indirect_variadic(Callback callback) {
    callback(1, 2); // expect: narrow-restricted-c-varargs
}
void c_io() {
    std::printf("%d", 1); // expect: narrow-restricted-c-varargs
    __builtin_printf("%d", 1); // expect: narrow-restricted-c-varargs, narrow-restricted-low-level
}
void collect(int count, ...) { // expect: narrow-restricted-c-varargs
    va_list arguments; // expect: narrow-restricted-c-varargs
    va_start(arguments, count); // expect: narrow-restricted-c-varargs
    int value = va_arg(arguments, int); // expect: narrow-restricted-c-varargs
    va_end(arguments); // expect: narrow-restricted-c-varargs
}

unsigned long vla_type_expressions(int count) {
    using Array = int[count]; // expect: narrow-no-extensions
    return sizeof(int[count]); // expect: narrow-no-extensions
}

using Result = std::expected<int, int>;
using ValueReference = decltype(std::declval<Result&>().value());
static_assert(sizeof(std::declval<Result&>().value()) == sizeof(int));
static_assert(requires(Result& result) { result.value(); });
static_assert(!noexcept(std::declval<Result&>().value()));

template <typename T> using Outcome = std::expected<T, int>;
template <typename T>
Outcome<T> alias_api(); // expect: narrow-expected-usage

// Generic lambdas are simple generic algorithms; forwarding remains restricted.
void generic_lambda() {
    auto call = [](auto&& value) { return sizeof(value); }; // expect: narrow-restricted-generics
}
template <typename T>
void forward_like_api(T& value) {
    auto&& target = std::forward_like<T>(value); // expect: narrow-restricted-generics
}

struct CInterface {
    int invoke(int, ...); // expect: narrow-restricted-c-varargs
};
void member_varargs(CInterface& api) {
    api.invoke(1, 2); // expect: narrow-restricted-c-varargs
    // NOLINTNEXTLINE(narrow-restricted-c-varargs) -- Required C adapter call.
    api.invoke(1, 2);
    // NOLINTNEXTLINE(narrow-restricted-c-varargs,narrow-restricted-low-level) -- Required C adapter output.
    __builtin_printf("%d", 1);
    // Compiler intrinsics use pseudo-variadic signatures for type checking.
    int sum;
    __builtin_add_overflow(1, 2, &sum); // expect: narrow-restricted-low-level
    __builtin_isfinite(1.0); // expect: narrow-restricted-low-level
}

template <typename T>
int dependent_value(T& result) {
    return result.value(); // expect: narrow-expected-usage
}
template <typename T>
auto deduced_failure(T value) { // expect: narrow-expected-usage
    return Outcome<T>(value);
}
template <typename T>
[[nodiscard]] auto checked_failure(T value) { return Outcome<T>(value); }
template <typename T>
void dependent_queries(T& result) {
    using Value = decltype(result.value());
    static_assert(sizeof(result.value()) == sizeof(int));
    static_assert(requires { result.value(); });
    static_assert(!noexcept(result.value()));
}
template <typename T>
int permitted_value(T& result) {
    // NOLINTNEXTLINE(narrow-expected-usage) -- Required non-propagation boundary.
    return result.value();
}
struct OwnValue { int value() { return 0; } };
void instantiate_expected(Result& result, std::expected<int, long>& other) {
    dependent_value(result);
    dependent_value(other); // Same source use is diagnosed only once.
    OwnValue own;
    dependent_value(own);
    auto first = deduced_failure(1);
    auto second = deduced_failure(2L);
    auto checked = checked_failure(1);
    dependent_queries(result);
    permitted_value(result);
}
