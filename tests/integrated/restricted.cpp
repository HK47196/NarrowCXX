#include <cstdlib>
#include <functional>
#include <new>

#if 1 // expect: narrow-restricted-preprocessor
constexpr int platform_value = 1;
#endif
union Storage { int number; float real; }; // expect: narrow-restricted-unions
int global_state; // expect: narrow-restricted-globals
struct Implementation { int state; };
struct Adapter : Implementation {}; // expect: narrow-inheritance
int* placement(void* storage) {
    return new (storage) int(1); // expect: narrow-restricted-placement-new
}
void* placement_function(void* storage) {
    return ::operator new(sizeof(int), storage); // expect: narrow-restricted-placement-new
}
void restricted_uses(const int* pointer) {
    std::function<void()> callback; // expect: narrow-restricted-library
    auto* mutable_pointer = const_cast<int*>(pointer); // expect: cppcoreguidelines-pro-type-const-cast
    auto* bytes = reinterpret_cast<const unsigned char*>(pointer); // expect: cppcoreguidelines-pro-type-reinterpret-cast
    auto* allocation = std::malloc(4); // expect: narrow-legacy-c-library
    std::free(allocation); // expect: narrow-legacy-c-library
    try {} catch (...) {} // expect: narrow-restricted-exceptions
}
