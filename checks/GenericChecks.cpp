#include "PolicyChecks.h"
#include "clang/AST/TypeLoc.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

namespace clang::tidy::narrow {
namespace {
using namespace ast_matchers;
AST_MATCHER(Decl, isVariableSpecialization) {
  return isa<VarTemplateSpecializationDecl>(Node);
}

class GenericsCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                namedDecl(anyOf(templateTypeParmDecl(),
                                                nonTypeTemplateParmDecl(),
                                                templateTemplateParmDecl()),
                                          unless(isExpansionInSystemHeader()))
                                    .bind("parameter")),
                       this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 functionDecl(unless(isImplicit()),
                              unless(isExpansionInSystemHeader()))
                     .bind("function")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource, typeLoc().bind("type")), this);
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            lambdaExpr(unless(isExpansionInSystemHeader())).bind("lambda")),
        this);
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                classTemplateSpecializationDecl(
                                    unless(isExpansionInSystemHeader()))
                                    .bind("specialization")),
                       this);
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                decl(isVariableSpecialization(),
                                     unless(isExpansionInSystemHeader()))
                                    .bind("variable-specialization")),
                       this);
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                declRefExpr(to(functionDecl()),
                                            unless(isExpansionInSystemHeader()))
                                    .bind("reference")),
                       this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 unresolvedLookupExpr(unless(isExpansionInSystemHeader()))
                     .bind("lookup")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const SourceManager &SM = *Result.SourceManager;
    if (const auto *Lambda = Result.Nodes.getNodeAs<LambdaExpr>("lambda")) {
      for (const ParmVarDecl *Parameter :
           Lambda->getCallOperator()->parameters()) {
        if (Parameter->isExplicitObjectParameter())
          report(Parameter->getBeginLoc(), SM, "explicit object parameters");
        const auto *Reference =
            Parameter->getType()->getAs<RValueReferenceType>();
        if (!Lambda->isGenericLambda() || !Reference ||
            Reference->getPointeeType().isConstQualified())
          continue;
        if (Reference->getPointeeType()->getContainedAutoType())
          report(Parameter->getLocation(), SM, "forwarding references");
        if (const auto *Type =
                Reference->getPointeeType()->getAs<TemplateTypeParmType>())
          for (const NamedDecl *TP : *Lambda->getTemplateParameterList())
            if (TP == Type->getDecl())
              report(Parameter->getLocation(), SM, "forwarding references");
      }
    }
    if (const auto *Parameter =
            Result.Nodes.getNodeAs<NamedDecl>("parameter")) {
      if (Parameter->isParameterPack())
        report(Parameter->getLocation(), SM, "variadic templates");
    }
    if (const auto *FD = Result.Nodes.getNodeAs<FunctionDecl>("function")) {
      if (isa<CXXDeductionGuideDecl>(FD))
        report(FD->getLocation(), SM, "custom deduction guides");
      if (FD->getTemplateSpecializationKind() == TSK_ExplicitSpecialization ||
          FD->getTemplateSpecializationKind() ==
              TSK_ExplicitInstantiationDeclaration ||
          FD->getTemplateSpecializationKind() ==
              TSK_ExplicitInstantiationDefinition) {
        // A specialization's member is not itself a function specialization.
        report(FD->getLocation(), SM,
               "function specialization or explicit instantiation");
      }
      for (const ParmVarDecl *Param : FD->parameters()) {
        if (Param->isExplicitObjectParameter())
          report(Param->getBeginLoc(), SM, "explicit object parameters");
        const auto *Reference = Param->getType()->getAs<RValueReferenceType>();
        if (!Reference || Reference->getPointeeType().isConstQualified())
          continue;
        const auto *TypeParameter =
            Reference->getPointeeType()->getAs<TemplateTypeParmType>();
        const FunctionTemplateDecl *Template =
            FD->getDescribedFunctionTemplate();
        if (TypeParameter && Template)
          for (const NamedDecl *TP : *Template->getTemplateParameters())
            if (TP == TypeParameter->getDecl())
              report(Param->getLocation(), SM, "forwarding references");
        if (const AutoType *Auto =
                Reference->getPointeeType()->getContainedAutoType();
            Auto && !Auto->isDecltypeAuto())
          report(Param->getLocation(), SM, "forwarding references");
      }
    }
    if (const auto *Loc = Result.Nodes.getNodeAs<TypeLoc>("type")) {
      if (const AutoTypeLoc Auto = Loc->getAs<AutoTypeLoc>();
          Auto && Auto.isDecltypeAuto())
        report(Auto.getBeginLoc(), SM, "decltype(auto)");
    }
    if (const auto *Spec =
            Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>(
                "specialization")) {
      if (Spec->getSpecializationKind() != TSK_ImplicitInstantiation &&
          !allowedStdSpecialization(Spec, SM))
        report(Spec->getBeginLoc(), SM,
               "class specialization or explicit instantiation");
    }
    if (const auto *Spec =
            Result.Nodes.getNodeAs<VarTemplateSpecializationDecl>(
                "variable-specialization"))
      if (Spec->getSpecializationKind() != TSK_ImplicitInstantiation)
        report(Spec->getBeginLoc(), SM,
               "variable template specialization or explicit instantiation");
    if (const auto *Ref = Result.Nodes.getNodeAs<DeclRefExpr>("reference"))
      if (qualifiedName(Ref->getDecl()) == "std::forward" ||
          qualifiedName(Ref->getDecl()) == "std::forward_like")
        report(Ref->getBeginLoc(), SM, "perfect forwarding");
    if (const auto *Lookup =
            Result.Nodes.getNodeAs<UnresolvedLookupExpr>("lookup"))
      for (const NamedDecl *Candidate : Lookup->decls())
        if (qualifiedName(Candidate->getUnderlyingDecl()) == "std::forward" ||
            qualifiedName(Candidate->getUnderlyingDecl()) ==
                "std::forward_like")
          report(Lookup->getBeginLoc(), SM, "perfect forwarding");
  }

private:
  void report(SourceLocation Loc, const SourceManager &SM, StringRef Feature) {
    if (firstAt(Loc, SM))
      diag(Loc, "%0 require a small external-API adapter or resource wrapper")
          << Feature;
  }
};
} // namespace

void registerGenericChecks(ClangTidyCheckFactories &Factories) {
  Factories.registerCheck<GenericsCheck>("narrow-restricted-generics");
}
} // namespace clang::tidy::narrow
