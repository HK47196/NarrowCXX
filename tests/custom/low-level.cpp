#include <utility>

struct alignas(32) Aligned { int data; }; // expect: narrow-restricted-low-level
struct __attribute__((packed)) Packed { int data; }; // expect: narrow-restricted-low-level
struct Layout {
    [[no_unique_address]] Aligned empty; // expect: narrow-restricted-low-level
};
int alias_contract(int* __restrict pointer) { return *pointer; } // expect: narrow-restricted-low-level
bool overflow(int a, int b, int* output) {
    return __builtin_add_overflow(a, b, output); // expect: narrow-restricted-low-level
}
unsigned representation(float input) {
    return __builtin_bit_cast(unsigned, input); // expect: narrow-restricted-low-level
}
void unreachable() {
    std::unreachable(); // expect: narrow-no-assumptions
    __builtin_unreachable(); // expect: narrow-no-assumptions
}

// NOLINTNEXTLINE(narrow-restricted-low-level) -- Platform ABI requires 32-byte alignment.
struct alignas(32) AbiAligned { int data; };
