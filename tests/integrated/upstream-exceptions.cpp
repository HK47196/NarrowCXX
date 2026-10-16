// A test stub provides a visible dependency exception for escape analysis.
void dependency_failure() {
    // NOLINTNEXTLINE(narrow-no-application-throw) -- Throwing test stub.
    throw 7;
}
void cleanup() noexcept;
void dependency_without_body();

struct EscapingDestructor {
    ~EscapingDestructor() { dependency_failure(); } // expect: bugprone-exception-escape
};
struct NonthrowingDestructor {
    ~NonthrowingDestructor() { cleanup(); }
};
void escaping_noexcept() noexcept { // expect: bugprone-exception-escape
    dependency_failure();
}
void nonthrowing_operation() noexcept { cleanup(); }

// The policy explicitly permits noexcept when escaping exceptions must terminate.
// NOLINTNEXTLINE(bugprone-exception-escape) -- Defined failure policy: a failure in this operation must terminate.
void terminate_on_failure() noexcept { dependency_failure(); }

void handled_exception() noexcept {
    // NOLINTNEXTLINE(narrow-restricted-exceptions) -- Defined operation boundary.
    try { dependency_failure(); } catch (...) { cleanup(); }
}

// The upstream analysis cannot establish what this absent dependency body does.
// This is a documented coverage limit, not evidence of exception safety.
void invisible_dependency() noexcept { dependency_without_body(); }
