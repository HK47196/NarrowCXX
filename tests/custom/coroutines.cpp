#include <coroutine>

// Required external coroutine adapter: the individual machinery uses are local.
struct Coroutine {
    struct promise_type {
        Coroutine get_return_object();
        // NOLINTNEXTLINE(narrow-excluded-library) -- Required coroutine compatibility adapter.
        std::suspend_never initial_suspend() noexcept;
        // NOLINTNEXTLINE(narrow-excluded-library) -- Required coroutine compatibility adapter.
        std::suspend_never final_suspend() noexcept;
        void return_void();
        void unhandled_exception();
        // NOLINTNEXTLINE(narrow-excluded-library) -- Required coroutine compatibility adapter.
        std::suspend_never yield_value(int);
    };
};
Coroutine application() {
    co_await std::suspend_never{}; // expect: narrow-no-coroutines, narrow-excluded-library
    co_yield 1; // expect: narrow-no-coroutines
    co_return; // expect: narrow-no-coroutines
}

void machinery() {
    std::coroutine_handle<> handle; // expect: narrow-excluded-library
}
