#include "system_api.hpp"
#include <memory>
#include <cstring>
#include <csetjmp>
#include <cstdlib>
#include <expected>
#include <cstdio>

void dependency_implementation(int value) {
    EXTERNAL_DISCARD(value);
    std::unique_ptr<int> owned(EXTERNAL_ALLOCATE());
}

// NOLINTNEXTLINE(narrow-no-function-macros) -- Project compatibility macro.
#define PROJECT_DISCARD(value) ((void)(value))
void project_implementation(int value) {
    PROJECT_DISCARD(value); // expect: narrow-no-c-style-casts
}

void dependency_c_implementation(void* output, const void* input, std::jmp_buf& buffer) {
    EXTERNAL_COPY(output, input);
    EXTERNAL_JUMP(buffer);
    EXTERNAL_DISCARD(setjmp(buffer)); // expect: narrow-no-unsafe-c-library
    EXTERNAL_COPY(std::malloc(4), input); // expect: narrow-legacy-c-library
    EXTERNAL_DISCARD(
        // NOLINTNEXTLINE(narrow-no-unsafe-c-library) -- Required C adapter protocol.
        setjmp(buffer));
}

void distinct_expected_uses(std::expected<int, int>& first, std::expected<int, int>& second) {
    EXTERNAL_DISCARD(first.value() + second.value()); // expect: narrow-expected-usage, narrow-expected-usage
}

void dependency_varargs(int value) {
    EXTERNAL_PRINT(value);
    external_emit(std::printf, value);
    EXTERNAL_DISCARD(std::printf("%d", value) + std::printf("%d", value)); // expect: narrow-restricted-c-varargs, narrow-restricted-c-varargs
}
