#include "c-library-declarations.hpp"

void std_owned_operations(void* output, const void* input, std::fenv_t* env) {
    std::memcpy(output, input, 4); // expect: narrow-legacy-c-library
    std::memmove(output, input, 4); // expect: narrow-legacy-c-library
    std::memset(output, 0, 4); // expect: narrow-legacy-c-library
    std::fesetround(0); // expect: narrow-legacy-c-library
    std::fesetenv(env); // expect: narrow-legacy-c-library
    std::feupdateenv(env); // expect: narrow-legacy-c-library
    std::feclearexcept(0); // expect: narrow-legacy-c-library
    std::feraiseexcept(0); // expect: narrow-legacy-c-library
    std::feholdexcept(env); // expect: narrow-legacy-c-library
    // NOLINTNEXTLINE(narrow-legacy-c-library) -- Required representation boundary.
    std::memcpy(output, input, 4);
    // NOLINTNEXTLINE(narrow-legacy-c-library) -- Required floating environment boundary.
    std::fesetround(0);
    // NOLINTNEXTLINE(narrow-restricted-library) -- Previous byte-operation permission needs migration.
    std::memcpy(output, input, 4); // expect: narrow-legacy-c-library
}

using std::memcpy; // expect: narrow-legacy-c-library
using std::fesetround; // expect: narrow-legacy-c-library
template <typename T>
void unresolved_byte_operation(T* output) {
    memcpy(output, output, 4); // expect: narrow-legacy-c-library
}
namespace own {
void* memcpy(void*, const void*, decltype(sizeof(0)));
int fesetround(int);
}
void project_operations(void* output) {
    own::memcpy(output, output, 4);
    own::fesetround(0);
}
