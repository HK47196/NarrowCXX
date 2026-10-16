#include <any>
#include <clocale>
#include <cstdlib>
#include <future>
#include <vector>
#include <list>
#include <forward_list>
#include <valarray>
#include <mdspan>
#include <csetjmp>
#include <cfenv>
#include <cstring>

namespace dependency {
    // NOLINTNEXTLINE(narrow-excluded-library) -- Required legacy boundary declaration.
    using Payload = std::vector<std::any>;
    // NOLINTNEXTLINE(narrow-excluded-library) -- Required legacy boundary declaration.
    using Packed = std::vector<bool>;
    // NOLINTNEXTLINE(narrow-excluded-library) -- Required legacy boundary declaration.
    extern Packed* const pointer;
    // NOLINTNEXTLINE(narrow-excluded-library) -- Required legacy boundary declaration.
    extern Packed (*const factory)();
    struct Callable {
        // NOLINTNEXTLINE(narrow-excluded-library) -- Required legacy boundary declaration.
        Packed operator()() const;
    };
    // NOLINTNEXTLINE(narrow-excluded-library) -- Required legacy boundary declaration.
    Payload nested();
}

void deduced_uses() {
    auto a = *dependency::pointer; // expect: narrow-excluded-library
    auto b = dependency::factory(); // expect: narrow-excluded-library
    auto c = dependency::Callable{}(); // expect: narrow-excluded-library
    auto d = dependency::nested(); // expect: narrow-excluded-library
}
template <typename T>
void instantiated_container() {
    std::vector<T> values; // expect: narrow-excluded-library
}
void instantiate() { instantiated_container<bool>(); }

void restricted_types() {
    std::list<int> linked; // expect: narrow-restricted-library
    std::forward_list<int> forward; // expect: narrow-restricted-library
    std::valarray<int> numeric; // expect: narrow-restricted-library
    int data[1] = {0};
    std::mdspan<int, std::extents<std::size_t, 1>> matrix(data); // expect: narrow-restricted-library
}

int compare(const void*, const void*) { return 0; }
extern "C" int rand() noexcept;
int c_redeclaration() {
    return ::rand(); // expect: narrow-no-unsafe-c-library
}
void permitted_c_queries() {
    constexpr const char* query = nullptr;
    std::setlocale(LC_ALL, query);
    std::setlocale(LC_ALL, static_cast<const char*>(nullptr));
    int data[2] = {1, 2};
    // The policy bans C sorting callbacks; other C facilities remain allowed.
    auto* found = std::bsearch(data, data, 2, sizeof(int), compare);
}

int c_jumps(std::jmp_buf& buffer) {
    return setjmp(buffer); // expect: narrow-no-unsafe-c-library
}
int permitted_c_jump(std::jmp_buf& buffer) {
    // NOLINTNEXTLINE(narrow-no-unsafe-c-library) -- Required C adapter protocol.
    return setjmp(buffer);
}
void c_operations(void* output, const void* input, std::fenv_t* env) {
    ::memcpy(output, input, 4); // expect: narrow-legacy-c-library
    std::memmove(output, input, 4); // expect: narrow-legacy-c-library
    std::memset(output, 0, 4); // expect: narrow-legacy-c-library
    ::fesetround(FE_TONEAREST); // expect: narrow-legacy-c-library
    std::fesetround(FE_TONEAREST); // expect: narrow-legacy-c-library
    ::fesetenv(env); // expect: narrow-legacy-c-library
    std::fesetenv(env); // expect: narrow-legacy-c-library
    ::feupdateenv(env); // expect: narrow-legacy-c-library
    std::feupdateenv(env); // expect: narrow-legacy-c-library
    ::feclearexcept(FE_ALL_EXCEPT); // expect: narrow-legacy-c-library
    std::feclearexcept(FE_ALL_EXCEPT); // expect: narrow-legacy-c-library
    ::feraiseexcept(FE_INVALID); // expect: narrow-legacy-c-library
    std::feraiseexcept(FE_INVALID); // expect: narrow-legacy-c-library
    ::feholdexcept(env); // expect: narrow-legacy-c-library
    std::feholdexcept(env); // expect: narrow-legacy-c-library
    // NOLINTNEXTLINE(narrow-legacy-c-library) -- Required representation boundary.
    std::memcpy(output, input, 4);
    // NOLINTNEXTLINE(narrow-legacy-c-library) -- Required floating environment boundary.
    std::fesetround(FE_TONEAREST);
}

using std::memmove; // expect: narrow-legacy-c-library
using std::feupdateenv; // expect: narrow-legacy-c-library
