#include <type_traits>
#include <utility>

template <typename... Types> struct Pack {}; // expect: narrow-restricted-generics
template <int... Sizes> struct Extents {}; // expect: narrow-restricted-generics
template <template <typename> typename... Templates> struct TemplatePack {}; // expect: narrow-restricted-generics

template <typename T>
void forward_reference(T&& value) { // expect: narrow-restricted-generics
    auto&& forwarded = std::forward<T>(value); // expect: narrow-restricted-generics
}
template <typename T>
void not_forwarding(const T&& value) {}
template <typename T>
struct Holder {
    void move(T&& value); // A class template's T&& is not a forwarding reference.
};

decltype(auto) identity(int& value) { return (value); } // expect: narrow-restricted-generics
struct ExplicitObject {
    int value;
    int read(this const ExplicitObject& self) { return self.value; } // expect: narrow-restricted-generics
};

template <typename T> struct Box { T value; };
Box(int) -> Box<int>; // expect: narrow-restricted-generics
template <> struct Box<bool> { bool value; }; // expect: narrow-restricted-generics
template <typename T> struct Box<T*> { T* value; }; // expect: narrow-restricted-generics
template <typename T> int operation(T) { return 1; }
template <> int operation(int) { return 2; } // expect: narrow-restricted-generics

template <typename T>
std::enable_if_t<std::is_integral_v<T>, T> sfinae(T value) { return value; } // expect: narrow-no-sfinae
using Enabler = std::enable_if<true, int>; // expect: narrow-no-sfinae
using Result = Enabler::type; // expect: narrow-no-sfinae
using std::enable_if; // expect: narrow-no-sfinae
template <typename T>
concept Integral = std::is_integral_v<T>;
template <Integral T>
constexpr T constrained(T value) { if constexpr (sizeof(T) > 1) { return value; } return T{}; }
static_assert(constrained(2) == 2);
