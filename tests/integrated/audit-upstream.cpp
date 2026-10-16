#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

int* no_arithmetic(int* pointer) {
    return pointer + 0; // expect-error: narrow-no-pointer-arithmetic
}
std::ptrdiff_t difference(int* left, int* right) {
    return left - right; // expect-error: narrow-no-pointer-arithmetic
}
template <typename T>
T* advance(T* pointer) {
    return pointer + 1; // expect-error: narrow-no-pointer-arithmetic
}
int unguarded_optional(std::optional<int>& value) {
    return *value; // expect: bugprone-unchecked-optional-access
}
void dangling() {
    std::string_view view = std::string("temporary"); // expect: bugprone-dangling-handle
}

unsigned char known_bad() {
    return static_cast<unsigned char>(300); // expect-error: narrow-known-narrowing
}
unsigned bad_sign() {
    return static_cast<unsigned>(-1); // expect-error: narrow-known-narrowing
}
signed char bad_positive() {
    return static_cast<signed char>(255U); // expect-error: narrow-known-narrowing
}
unsigned char known_good() { return static_cast<unsigned char>(255); }
signed char known_negative() { return static_cast<signed char>(-128); }
unsigned char guarded(unsigned value) {
    if (value > 255) { return 0; }
    return static_cast<unsigned char>(value);
}
