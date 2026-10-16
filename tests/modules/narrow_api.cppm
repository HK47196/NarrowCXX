module;
#include <any>
export module narrow_api;
// NOLINTNEXTLINE(narrow-excluded-library) -- Export a legacy dependency type to test imported alias resolution.
export using Dynamic = std::any;
