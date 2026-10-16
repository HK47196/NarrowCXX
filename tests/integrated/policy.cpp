#include <any>
#include <expected>
#include <memory>
#include <span>
#include <vector>

std::expected<int, int> failure(); // expect: narrow-expected-usage
[[nodiscard]] std::expected<int, int> checked_failure();
int* allocation() { return new int(1); } // expect-error: narrow-no-direct-new-delete
int cast(double value) { return (int)value; } // expect-error: narrow-no-c-style-casts
int* advance(int* pointer) { return pointer + 1; } // expect-error: narrow-no-pointer-arithmetic
int narrow(double value) { int result = value; return result; } // expect-error: bugprone-narrowing-conversions
void dynamic_value() {
    std::any value; // expect-error: narrow-excluded-library
}

struct Window { int width; int height; };
Window record() { return Window{.width = 1280, .height = 720}; }
std::unique_ptr<int> owned() { return std::make_unique<int>(1); }
int bounded(std::span<const int> values) { return values.empty() ? 0 : values.front(); }
