#include <expected>
#include <optional>
#include <variant>

union Raw { int number; float real; }; // expect: narrow-restricted-unions
struct Tagged {
    union { int number; float real; }; // expect: narrow-restricted-unions
};

struct Conversion {
    Conversion(int); // expect: narrow-explicit-conversions
    Conversion(int, int); // expect: narrow-explicit-conversions
    operator bool() const; // expect: narrow-explicit-conversions
    explicit operator int() const;
    explicit Conversion(double);
    Conversion(const Conversion&) = default;
    Conversion(Conversion&&) = default;
};

int global; // expect: narrow-restricted-globals
thread_local const int per_thread = 1; // expect: narrow-restricted-globals
struct State {
    inline static int mutable_state = 0; // expect: narrow-restricted-globals
    static constexpr int fixed = 1;
};
constexpr int constant = 4;
const int readonly = 1;
int local() { int value = 0; return value; }

void failure() { throw 1; } // expect: narrow-no-application-throw
void boundary() {
    try { local(); } catch (...) {} // expect: narrow-restricted-exceptions
}

void assumptions(bool valid) {
    [[assume(valid)]]; // expect: narrow-no-assumptions
    __builtin_assume(valid); // expect: narrow-no-assumptions
}

std::expected<int, int> missing_attribute(); // expect: narrow-expected-usage
using Result = std::expected<int, int>;
Result alias_result(); // expect: narrow-expected-usage
[[nodiscard]] Result valid_result();
template <typename T>
std::expected<T, int> generic_failure(); // expect: narrow-expected-usage
std::optional<int> maybe();
int extract(Result& result) {
    return result.value(); // expect: narrow-expected-usage
}
int checked(Result& result) {
    if (!result) { return -1; }
    return *result;
}
int ordinary_value(std::optional<int>& result) { return result.value(); }

void c_adapter(int count, ...); // expect: narrow-restricted-c-varargs

int extensions(int count) {
    int values[count]; // expect: narrow-no-extensions
    int expression = ({ 1; }); // expect: narrow-no-extensions
    void* label = &&target; // expect: narrow-no-extensions
    goto *label; // expect: narrow-no-extensions
target:
    return expression;
}

// Permissions stay local and do not exempt unrelated operations.
union ResourceStorage { int handle; void* pointer; }; // NOLINT(narrow-restricted-unions) -- C ABI boundary.
void test_stub() {
    // NOLINTNEXTLINE(narrow-no-application-throw) -- Throwing stub in a test.
    throw 1;
}
