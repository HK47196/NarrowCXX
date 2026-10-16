#include "project_header.hpp"
#include <new>
#include <typeinfo>

struct Base {
    virtual ~Base() = default;
    virtual void operate() = 0;
};
struct Derived final : Base { void operate() override {} };

Derived* cast(Base* base) {
    return dynamic_cast<Derived*>(base); // expect: narrow-no-rtti
}
const std::type_info& inspect(Base& base) { // expect: narrow-excluded-library
    return typeid(base); // expect: narrow-no-rtti
}

struct Choice {};
bool operator&&(Choice, Choice); // expect: narrow-no-logical-comma-overload
bool operator||(Choice, Choice); // expect: narrow-no-logical-comma-overload
Choice operator,(Choice, Choice); // expect: narrow-no-logical-comma-overload
bool operator!(Choice); // expect: narrow-no-logical-comma-overload

int* allocate() {
    return new int(1); // expect: narrow-no-direct-new-delete
}
int* allocate_array() {
    return new int[4]; // expect: narrow-no-direct-new-delete
}
void release(int* pointer) {
    delete pointer; // expect: narrow-no-direct-new-delete
}
void release_array(int* pointer) {
    delete[] pointer; // expect: narrow-no-direct-new-delete
}
int* placement(void* storage) {
    return new (storage) int(3); // expect: narrow-restricted-placement-new
}
void* allocation_function() {
    return ::operator new(4); // expect: narrow-no-direct-new-delete
}
void deallocation_function(void* storage) {
    ::operator delete(storage); // expect: narrow-no-direct-new-delete
}
int c_style(double value) {
    return (int)value; // expect: narrow-no-c-style-casts
}
void discard(int value) {
    (void)value; // expect: narrow-no-c-style-casts
}

template <typename T>
T* create() {
    return new T(); // expect: narrow-no-direct-new-delete
}
int* instantiate() {
    return create<int>();
}

#define ALLOCATE_INTEGER new int(4)
int* macro_allocation() {
    return ALLOCATE_INTEGER; // expect: narrow-no-direct-new-delete
}
