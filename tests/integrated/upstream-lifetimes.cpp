#include <memory>
#include <mutex>
#include <utility>

void protected_work();
void discarded_lock(std::mutex& mutex) {
    std::lock_guard<std::mutex>{mutex}; // expect: bugprone-unused-raii
    protected_work();
}
void retained_lock(std::mutex& mutex) {
    std::lock_guard<std::mutex> lock{mutex};
    protected_work();
}

int invalid_move_use() {
    auto source = std::make_unique<int>(7);
    auto destination = std::move(source);
    return *source; // expect: bugprone-use-after-move
}
int reinitialized_move() {
    auto source = std::make_unique<int>(7);
    auto destination = std::move(source);
    source = std::make_unique<int>(9);
    return *source;
}
int checked_moved_owner() {
    auto source = std::make_unique<int>(7);
    auto destination = std::move(source);
    // The upstream analysis is not path-sensitive to the null guard. unique_ptr
    // guarantees a null source after this move, so the dereference is unreachable.
    // NOLINTNEXTLINE(bugprone-use-after-move) -- The condition establishes a non-null owner; this moved-from source is null.
    return source ? *source : *destination;
}
bool moved_owner_is_empty() {
    auto source = std::make_unique<int>(7);
    auto destination = std::move(source);
    return source == nullptr;
}
int disjoint_branches(bool transfer) {
    auto source = std::make_unique<int>(7);
    if (transfer) {
        auto destination = std::move(source);
        return *destination;
    } else {
        return *source;
    }
}
