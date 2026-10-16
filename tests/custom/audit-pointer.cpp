#include <span>

int raw(int* values, int index) {
    return values[index]; // expect: narrow-no-pointer-arithmetic
}
void increment(int* pointer) {
    ++pointer; // expect: narrow-no-pointer-arithmetic
    pointer -= 1; // expect: narrow-no-pointer-arithmetic
}
template <typename T>
T generic_advance(T value) {
    return value + 1; // expect: narrow-no-pointer-arithmetic
}
void instantiate(int* pointer) { generic_advance(pointer); }
int bounded(std::span<const int> values) { return values[0]; }
int known_array() {
    int values[2] = {1, 2};
    return values[1];
}
