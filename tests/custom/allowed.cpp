#include <memory>

struct Value {
    int value;
    bool operator==(const Value&) const = default;
    Value operator+(Value other) const {
        return Value{.value = value + other.value};
    }
};

bool ordinary_logic(bool a, bool b) {
    return a && b;
}

std::unique_ptr<int> owned() {
    return std::make_unique<int>(1);
}

// A resource adapter must justify each extra permission locally.
int* external_resource_allocate() {
    // NOLINTNEXTLINE(narrow-no-direct-new-delete) -- Resource boundary: ownership transfers to external C API.
    return new int(1);
}
void external_resource_release(int* pointer) {
    // NOLINTNEXTLINE(narrow-no-direct-new-delete) -- Resource boundary: C API transfers ownership back here.
    delete pointer;
}
