#include "audit-guards.hpp"
#include "audit-comment-guard.hpp"
#include "audit-comment-guard.hpp"
#if 1 // expect: narrow-restricted-preprocessor
constexpr int enabled = 1;
#elifdef ABSENT // expect: narrow-restricted-preprocessor
const char* skipped_literal = R"tag(
#elifndef THIS_IS_STRING_CONTENT
)tag";
constexpr int other = 0;
#elifndef ABSENT // expect: narrow-restricted-preprocessor
constexpr int third = 0;
#endif
