#include <cstdlib>
#include <new>

#define APPLICATION(x) ((x) + 1) // expect-error: narrow-no-function-macros
struct First { int first; };
struct Second { int second; };
struct Multiple : First, Second {}; // expect-error: narrow-no-complex-inheritance
int* allocation() { return new int(1); } // expect-error: narrow-no-direct-new-delete
int cast(double value) { return (int)value; } // expect-error: narrow-no-c-style-casts
void application_failure() { throw 1; } // expect-error: narrow-no-application-throw
int random_value() { return std::rand(); } // expect-error: narrow-no-unsafe-c-library
int* nothrow_allocation() { return new (std::nothrow) int(1); } // expect-error: narrow-no-direct-new-delete
void c_varargs(int, ...); // expect-error: narrow-restricted-c-varargs
