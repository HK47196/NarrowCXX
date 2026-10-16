#include <array>

struct Window { int width; int height; };
Window positional() {
    return Window{1280, 720}; // expect: modernize-use-designated-initializers
}
Window designated() { return Window{.width = 1280, .height = 720}; }
Window omitted_field() { return Window{.width = 1280}; }
struct Identifier { int value; };
Identifier single_field() {
    return Identifier{7}; // expect: modernize-use-designated-initializers
}
std::array<int, 2> fixed_array() { return {1, 2}; }

class Invariant {
public:
    explicit Invariant(int value) : value(value) {}
private:
    int value;
};
Invariant constructor() { return Invariant(7); }

struct Defaultable {
    Defaultable() {} // expect: modernize-use-equals-default
    ~Defaultable() {} // expect: modernize-use-equals-default
};
struct AlreadyDefaulted {
    AlreadyDefaulted() = default;
    ~AlreadyDefaulted() = default;
};
void release_resource(int handle) noexcept;
struct Resource {
    int handle;
    ~Resource() { release_resource(handle); }
};
