#include <bit>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory_resource>
#include <tuple>

void restricted_operations(void* storage, const void* input) {
    std::function<void()> callback; // expect: narrow-restricted-library
    std::tuple<int, int> result; // expect: narrow-restricted-library
    std::pmr::monotonic_buffer_resource resource; // expect: narrow-restricted-library
    auto representation = std::bit_cast<unsigned>(1.0F); // expect: narrow-restricted-library
    std::memcpy(storage, input, 4); // expect: narrow-legacy-c-library
    void* allocated = std::malloc(4); // expect: narrow-legacy-c-library
    std::free(allocated); // expect: narrow-legacy-c-library
    auto random = std::rand(); // expect: narrow-no-unsafe-c-library
    std::setlocale(LC_ALL, "C"); // expect: narrow-legacy-c-library
    std::setlocale(LC_ALL, nullptr);
}

namespace own {
    int rand() { return 1; }
    void setlocale(int, const char*) {}
}
void unrelated_symbols() {
    auto random = own::rand();
    own::setlocale(0, "local");
}
