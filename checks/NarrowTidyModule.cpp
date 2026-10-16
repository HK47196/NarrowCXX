#include "PolicyChecks.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

namespace clang::tidy::narrow {
using namespace ast_matchers;

AST_MATCHER(Stmt, isTypeidExpression) { return isa<CXXTypeidExpr>(Node); }

// This tooling adapter follows Clang's extension interfaces. Each exception is
// local: users may use a named NOLINT with a boundary reason at the actual use.
class NoDirectNewDeleteCheck final : public PolicyCheck {
public:
  NoDirectNewDeleteCheck(StringRef Name, ClangTidyContext *Context)
      : PolicyCheck(Name, Context),
        PlacementOnly(Name == "narrow-restricted-placement-new") {}

  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 cxxNewExpr(unless(isExpansionInSystemHeader())).bind("new")),
        this);
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            cxxDeleteExpr(unless(isExpansionInSystemHeader())).bind("delete")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 declRefExpr(to(functionDecl(
                                 anyOf(hasOverloadedOperatorName("new"),
                                       hasOverloadedOperatorName("new[]"),
                                       hasOverloadedOperatorName("delete"),
                                       hasOverloadedOperatorName("delete[]")))),
                             unless(isExpansionInSystemHeader()))
                     .bind("allocation-function")),
        this);
  }

  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *New = Result.Nodes.getNodeAs<CXXNewExpr>("new")) {
      const bool Placement = New->getNumPlacementArgs() != 0 &&
                             placementFunction(New->getOperatorNew());
      if (Placement != PlacementOnly)
        return;
      if (PlacementOnly) {
        diag(New->getBeginLoc(), "placement construction requires a named "
                                 "low-level boundary with explicit storage, "
                                 "alignment, and lifetime requirements");
        return;
      }
      diag(New->getBeginLoc(), "direct new requires a resource or allocation "
                               "boundary; use a value or RAII factory");
    }
    if (const auto *Delete = Result.Nodes.getNodeAs<CXXDeleteExpr>("delete")) {
      if (PlacementOnly)
        return;
      diag(
          Delete->getBeginLoc(),
          "direct delete requires a resource or allocation boundary; use RAII");
    }
    if (const auto *Reference =
            Result.Nodes.getNodeAs<DeclRefExpr>("allocation-function")) {
      if (placementFunction(cast<FunctionDecl>(Reference->getDecl())) !=
          PlacementOnly)
        return;
      if (spellsToken(Reference->getLocation(), "operator",
                      *Result.SourceManager, Result.Context->getLangOpts()))
        diag(Reference->getBeginLoc(),
             PlacementOnly
                 ? "placement functions require a named low-level boundary "
                   "with explicit storage, alignment, and lifetime requirements"
                 : "direct allocation/deallocation functions require a "
                   "resource or allocation boundary; use RAII");
    }
  }

private:
  static bool placementFunction(const FunctionDecl *FD) {
    return FD && FD->getNumParams() > 1 &&
           FD->getParamDecl(FD->getNumParams() - 1)
               ->getType()
               ->isVoidPointerType();
  }
  const bool PlacementOnly;
};

class NoRttiCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;

  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 cxxDynamicCastExpr(unless(isExpansionInSystemHeader()))
                     .bind("cast")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 stmt(isTypeidExpression(), unless(isExpansionInSystemHeader()))
                     .bind("typeid")),
        this);
  }

  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *Cast = Result.Nodes.getNodeAs<CXXDynamicCastExpr>("cast")) {
      diag(Cast->getBeginLoc(), "dynamic_cast is excluded; use a record, "
                                "variant, or explicit interface");
    }
    if (const auto *TypeId = Result.Nodes.getNodeAs<CXXTypeidExpr>("typeid")) {
      diag(TypeId->getBeginLoc(),
           "typeid is excluded; use an explicit interface");
    }
  }
};

class NoLogicalCommaOverloadCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;

  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 functionDecl(unless(isImplicit()),
                              unless(isExpansionInSystemHeader()),
                              anyOf(hasOverloadedOperatorName("&&"),
                                    hasOverloadedOperatorName("||"),
                                    hasOverloadedOperatorName("!"),
                                    hasOverloadedOperatorName(",")))
                     .bind("operator")),
        this);
  }

  void check(const MatchFinder::MatchResult &Result) override {
    const auto *Operator = Result.Nodes.getNodeAs<FunctionDecl>("operator");
    diag(Operator->getLocation(), "overloaded logical and comma operators hide "
                                  "expression behavior; use a named function");
  }
};

class NoCStyleCastsCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            cStyleCastExpr(unless(isExpansionInSystemHeader())).bind("cast")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const auto *Cast = Result.Nodes.getNodeAs<CStyleCastExpr>("cast");
    diag(Cast->getBeginLoc(),
         "C-style casts are banned, including casts to void; use a named C++ "
         "conversion or [[maybe_unused]]");
  }
};

class NarrowModule final : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &Factories) override {
    Factories.registerCheck<NoDirectNewDeleteCheck>(
        "narrow-no-direct-new-delete");
    Factories.registerCheck<NoDirectNewDeleteCheck>(
        "narrow-restricted-placement-new");
    Factories.registerCheck<NoRttiCheck>("narrow-no-rtti");
    Factories.registerCheck<NoLogicalCommaOverloadCheck>(
        "narrow-no-logical-comma-overload");
    Factories.registerCheck<NoCStyleCastsCheck>("narrow-no-c-style-casts");
    registerLanguageChecks(Factories);
    registerLibraryChecks(Factories);
    registerGenericChecks(Factories);
    registerPreprocessorChecks(Factories);
  }
};

static ClangTidyModuleRegistry::Add<NarrowModule>
    Registration("narrow-module", "Experimental Narrow C++ checks");
} // namespace clang::tidy::narrow
