#include <memory>
std::unique_ptr<int> owned() { return std::make_unique<int>(1); }
int* resource_boundary() {
    // NOLINTNEXTLINE(narrow-no-direct-new-delete) -- Resource boundary: ownership transfers to the external API.
    return new int(1);
}
