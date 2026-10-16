#include <format>
#include <functional>

struct Key { int value; };
template <> struct std::hash<Key> {
    std::size_t operator()(const Key& key) const noexcept {
        return std::hash<int>{}(key.value);
    }
};

template <> struct std::formatter<Key> : std::formatter<int> {
    constexpr std::format_parse_context::iterator parse(std::format_parse_context& context) {
        if (context.begin() == context.end()) {
            throw std::format_error("missing format specification");
        }
        return context.begin();
    }
    template <typename Context>
    Context::iterator format(const Key& key, Context& context) const {
        return std::formatter<int>::format(key.value, context);
    }
};

void outside_protocol() {
    throw std::format_error("application error"); // expect: narrow-no-application-throw
}

// Arbitrary adapter methods do not receive the formatter throw permission.
struct Adapter {
    void parse() { throw std::format_error("application error"); } // expect: narrow-no-application-throw
};
