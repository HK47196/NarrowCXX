#include <any>
#include <execution>
#include <future>
#include <functional>
#include <ranges>
#include <span>
#include <typeindex>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdlib>

int increment(int value) { return value + 1; }

void excluded_types() {
    std::any dynamic; // expect: narrow-excluded-library
    using Anything = std::any; // expect: narrow-excluded-library
    Anything alias; // expect: narrow-excluded-library
    std::vector<bool> bits; // expect: narrow-excluded-library
    using Packed = std::vector<bool>; // expect: narrow-excluded-library
    Packed packed_alias; // expect: narrow-excluded-library
    std::future<int> future; // expect: narrow-excluded-library
    std::promise<int> promise; // expect: narrow-excluded-library
    auto pending = std::async([] { return 1; }); // expect: narrow-excluded-library
    using std::bind; // expect: narrow-excluded-library
    auto bound = bind(increment, 1); // expect: narrow-excluded-library
    auto placeholder = std::placeholders::_1; // expect: narrow-excluded-library
    std::vector<int> data{1, 2, 3};
    auto lazy = data | std::views::filter([](int value) { return value > 1; }); // expect: narrow-excluded-library
    auto lazy_transform = std::views::transform(increment); // expect: narrow-excluded-library
    auto execution = std::execution::seq; // expect: narrow-excluded-library
}

// Match canonical declarations, preserving unrelated project symbols.
namespace project {
    struct any {};
    template <typename T> struct future {};
    int bind(int value) { return value; }
}
namespace external {
    // NOLINTNEXTLINE(narrow-excluded-library) -- Existing dependency result type.
    std::any acquire();
    // NOLINTNEXTLINE(narrow-excluded-library) -- Existing dependency borrow type.
    std::any* borrow();
}
void deduced_dependency_results() {
    auto value = external::acquire(); // expect: narrow-excluded-library
    auto* borrowed = external::borrow(); // expect: narrow-excluded-library
}
void ordinary_library() {
    std::vector<int> data{3, 1, 2};
    std::ranges::sort(data);
    std::span<int> span(data);
    std::ranges::ref_view ref(data);
    std::ranges::subrange sub(data.begin(), data.end());
    auto all = std::views::all(data);
    std::mutex lock;
    std::lock_guard guard(lock);
    std::atomic<int> atomic{1};
    std::thread worker([] {});
    worker.join();
    auto absolute = std::abs(-2);
    auto division = std::div(7, 3);
    project::any dynamic;
    project::future<int> future;
    auto bound = project::bind(1);
}

void compatibility() {
    // NOLINTNEXTLINE(narrow-excluded-library) -- Required external API accepts a future.
    std::future<int> external;
    // NOLINTNEXTLINE(narrow-restricted-library) -- Existing callback interface requires copyable std::function.
    std::function<int(int)> callback = increment;
}
