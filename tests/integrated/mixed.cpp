#include <cstdlib>
#include <csetjmp>
int mixed() {
    auto* allocation = std::malloc(4); // expect: narrow-legacy-c-library
    std::free(allocation); // expect: narrow-legacy-c-library
    return std::rand(); // expect-error: narrow-no-unsafe-c-library
}

int c_boundary(std::jmp_buf& buffer) {
    return setjmp(buffer); // expect-error: narrow-no-unsafe-c-library
}
int permitted_c_boundary(std::jmp_buf& buffer) {
    // NOLINTNEXTLINE(narrow-no-unsafe-c-library) -- Required C adapter protocol.
    return setjmp(buffer);
}
