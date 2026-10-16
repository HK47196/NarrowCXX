#pragma clang system_header

// Model a standard library that owns these declarations in std instead of
// importing global C declarations. No platform headers are needed in this TU.
namespace std {
void* memcpy(void*, const void*, decltype(sizeof(0)));
void* memmove(void*, const void*, decltype(sizeof(0)));
void* memset(void*, int, decltype(sizeof(0)));
struct fenv_t;
int fesetround(int);
int fesetenv(const fenv_t*);
int feupdateenv(const fenv_t*);
int feclearexcept(int);
int feraiseexcept(int);
int feholdexcept(fenv_t*);
}
