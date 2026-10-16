#include <format>
#include <functional>

template <typename T> struct Box { T value; };
template <typename T>
struct std::formatter<Box<T>> : std::formatter<T> {
    template <typename Context>
    Context::iterator format(const Box<T>& box, Context& context) const {
        return std::formatter<T>::format(box.value, context);
    }
};
template <typename T>
struct std::hash<Box<T>> {
    std::size_t operator()(const Box<T>& box) const {
        return std::hash<T>{}(box.value);
    }
};

struct Key { int value; };
template <> struct std::formatter<Key> : std::formatter<int> {
    void application_failure() {
        throw std::format_error("application failure"); // expect: narrow-no-application-throw
    }
};
