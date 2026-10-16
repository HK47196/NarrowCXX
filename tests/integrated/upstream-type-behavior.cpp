int* remove_const(const int* pointer) {
    return const_cast<int*>(pointer); // expect: cppcoreguidelines-pro-type-const-cast
}
const int* add_const(int* pointer) {
    return const_cast<const int*>(pointer); // expect: cppcoreguidelines-pro-type-const-cast
}
const int* ordinary_qualification(int* pointer) { return pointer; }
int* remove_volatile(volatile int* pointer) {
    return const_cast<int*>(pointer); // expect: cppcoreguidelines-pro-type-const-cast
}
unsigned char* representation(int* pointer) {
    return reinterpret_cast<unsigned char*>(pointer); // expect: cppcoreguidelines-pro-type-reinterpret-cast
}
unsigned char* representation_boundary(int* pointer) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) -- Byte boundary: caller retains ownership and guarantees size, alignment and lifetime.
    return reinterpret_cast<unsigned char*>(pointer);
}

struct Interface {
    virtual ~Interface() = default;
    virtual void run() = 0;
};
struct MissingOverride final : Interface {
    void run() {} // expect: modernize-use-override
};
struct CorrectOverride final : Interface {
    void run() override {}
};
struct ExplicitFinal final : Interface {
    void run() override final {}
};
struct RedundantVirtual final : Interface {
    virtual void run() override {}
};
struct MissingVirtualDestructor { // expect: cppcoreguidelines-virtual-class-destructor
    virtual void run() = 0;
};
struct BrokenImplementation final : MissingVirtualDestructor { // expect: narrow-inheritance
    void run() override {}
};

MissingOverride* unchecked_downcast(Interface* pointer) {
    return static_cast<MissingOverride*>(pointer); // expect-error: cppcoreguidelines-pro-type-static-cast-downcast
}
MissingOverride& unchecked_reference(Interface& value) {
    return static_cast<MissingOverride&>(value); // expect-error: cppcoreguidelines-pro-type-static-cast-downcast
}
Interface* upcast(MissingOverride* pointer) { return pointer; }
int numeric_cast(short value) { return static_cast<int>(value); }
