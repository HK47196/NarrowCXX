#include "PolicyChecks.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprConcepts.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Analysis/Analyses/ExprMutationAnalyzer.h"
#include "clang/Basic/Builtins.h"

namespace clang::tidy::narrow {
namespace {
using namespace ast_matchers;
AST_MATCHER(Stmt, isAttributedStatement) { return isa<AttributedStmt>(Node); }
AST_MATCHER(Stmt, isVarargAccess) { return isa<VAArgExpr>(Node); }
AST_MATCHER(Stmt, isRepresentationBuiltin) {
  return isa<BuiltinBitCastExpr>(Node);
}

bool interfaceBase(const CXXRecordDecl *Base) {
  Base = Base ? Base->getDefinition() : nullptr;
  if (!Base || !Base->field_empty() || Base->getNumBases() != 0 ||
      !Base->isAbstract())
    return false;
  const CXXDestructorDecl *Destructor = Base->getDestructor();
  if (!Destructor || !Destructor->isVirtual())
    return false;
  for (const CXXMethodDecl *Method : Base->methods()) {
    if (Method->isImplicit() ||
        isa<CXXConstructorDecl, CXXDestructorDecl>(Method) ||
        Method->isStatic())
      continue;
    if (!Method->isPureVirtual())
      return false;
  }
  return true;
}

class InheritanceCheck final : public PolicyCheck {
public:
  InheritanceCheck(StringRef Name, ClangTidyContext *Context)
      : PolicyCheck(Name, Context),
        BannedOnly(Name == "narrow-no-complex-inheritance") {}
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 cxxRecordDecl(isDefinition(), unless(isImplicit()),
                               unless(isExpansionInSystemHeader()))
                     .bind("record")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const auto *RD = Result.Nodes.getNodeAs<CXXRecordDecl>("record");
    if (RD->getNumBases() == 0)
      return;
    const bool Banned = RD->getNumBases() > 1 || RD->bases_begin()->isVirtual();
    if (Banned != BannedOnly)
      return;
    StringRef Reason;
    if (RD->getNumBases() > 1)
      Reason = "multiple inheritance is banned";
    else {
      const CXXBaseSpecifier &Base = *RD->bases_begin();
      if (Base.isVirtual())
        Reason = "virtual inheritance is banned";
      else if (Base.getAccessSpecifier() != AS_public)
        Reason = "inheritance must be public";
      else if (allowedStdSpecialization(RD, *Result.SourceManager) &&
               stdTemplate(RD, "formatter") &&
               stdTemplateType(Base.getType(), "formatter"))
        return;
      else if (interfaceBase(Base.getType()->getAsCXXRecordDecl()))
        return;
      else
        Reason = "implementation or unresolved dependent inheritance requires "
                 "an external-interface boundary";
    }
    if (firstAt(RD->getBeginLoc(), *Result.SourceManager))
      diag(RD->getBeginLoc(), "%0; use one public data-free pure-virtual "
                              "interface base or composition")
          << Reason;
  }

private:
  const bool BannedOnly;
};

class UnionCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                recordDecl(isUnion(), unless(isImplicit()),
                                           unless(isExpansionInSystemHeader()))
                                    .bind("union")),
                       this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const auto *Union = Result.Nodes.getNodeAs<RecordDecl>("union");
    diag(Union->getBeginLoc(), "raw unions require a named low-level or "
                               "interoperability boundary; use std::variant");
  }
};

class ExplicitConversionsCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 cxxConstructorDecl(unless(isImplicit()),
                                    unless(isExpansionInSystemHeader()))
                     .bind("constructor")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 cxxConversionDecl(unless(isImplicit()),
                                   unless(isExpansionInSystemHeader()))
                     .bind("conversion")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *Ctor =
            Result.Nodes.getNodeAs<CXXConstructorDecl>("constructor")) {
      // Multi-argument constructors also participate in implicit copy-list
      // initialization.
      if (!Ctor->isCopyOrMoveConstructor() && Ctor->getNumParams() != 0 &&
          !Ctor->isExplicit())
        diag(Ctor->getLocation(), "converting constructors must be explicit "
                                  "except normal copy/move constructors");
    }
    if (const auto *Conversion =
            Result.Nodes.getNodeAs<CXXConversionDecl>("conversion")) {
      if (!Conversion->isExplicit())
        diag(
            Conversion->getLocation(),
            "conversion operators must be explicit; prefer a named conversion");
    }
  }
};

class GlobalStateCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                varDecl(unless(isImplicit()),
                                        unless(isExpansionInSystemHeader()))
                                    .bind("variable")),
                       this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const auto *VD = Result.Nodes.getNodeAs<VarDecl>("variable");
    if (isa<ParmVarDecl>(VD) ||
        VD->isStaticDataMember() && !VD->isThisDeclarationADefinition())
      return;
    if (VD->getTLSKind() != VarDecl::TLS_None)
      diag(VD->getLocation(),
           "thread_local state requires a runtime or platform justification; "
           "prefer explicit state parameters");
    else if (!VD->isLocalVarDecl() && !VD->getType().isConstQualified() &&
             !VD->isConstexpr())
      diag(VD->getLocation(),
           "mutable global state requires a runtime or platform justification; "
           "prefer explicit state parameters");
  }
};

bool formatterContext(const Stmt *Statement, ASTContext &Context) {
  auto Parents = Context.getParents(*Statement);
  while (!Parents.empty()) {
    if (const auto *Method = Parents[0].get<CXXMethodDecl>())
      return (Method->getNameAsString() == "parse" ||
              Method->getNameAsString() == "format") &&
             stdTemplate(Method->getParent(), "formatter") &&
             allowedStdSpecialization(Method->getParent(),
                                      Context.getSourceManager());
    if (const Stmt *Parent = Parents[0].get<Stmt>())
      Parents = Context.getParents(*Parent);
    else
      return false;
  }
  return false;
}

class ExceptionsCheck final : public PolicyCheck {
public:
  ExceptionsCheck(StringRef Name, ClangTidyContext *Context)
      : PolicyCheck(Name, Context),
        CheckThrows(Name == "narrow-no-application-throw"),
        CheckMode(Name == "narrow-exceptions-enabled") {}
  void registerMatchers(MatchFinder *Finder) override {
    if (CheckMode) {
      Finder->addMatcher(translationUnitDecl().bind("translation-unit"), this);
      return;
    }
    if (CheckThrows) {
      Finder->addMatcher(
          traverse(
              TK_IgnoreUnlessSpelledInSource,
              cxxThrowExpr(unless(isExpansionInSystemHeader())).bind("throw")),
          this);
      return;
    }
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 cxxTryStmt(unless(isExpansionInSystemHeader())).bind("try")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    if (Result.Nodes.getNodeAs<TranslationUnitDecl>("translation-unit")) {
      if (!Result.Context->getLangOpts().CXXExceptions)
        diag(Result.SourceManager->getLocForStartOfFile(
                 Result.SourceManager->getMainFileID()),
             "Narrow C++ requires exceptions enabled; remove -fno-exceptions");
    }
    if (const auto *Throw = Result.Nodes.getNodeAs<CXXThrowExpr>("throw")) {
      const Expr *Value = Throw->getSubExpr();
      const CXXRecordDecl *Type =
          Value ? Value->getType()->getAsCXXRecordDecl() : nullptr;
      if (Type && qualifiedName(Type) == "std::format_error" &&
          formatterContext(Throw, *Result.Context))
        return;
      diag(Throw->getThrowLoc(),
           "throw requires a formatter protocol or test exception; use "
           "std::expected for application errors");
    }
    if (const auto *Try = Result.Nodes.getNodeAs<CXXTryStmt>("try"))
      diag(Try->getTryLoc(), "catch exceptions only at a defined operation, "
                             "application, or task boundary, or in tests");
  }

private:
  const bool CheckThrows;
  const bool CheckMode;
};

class CoroutinesCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            coawaitExpr(unless(isExpansionInSystemHeader())).bind("coroutine")),
        this);
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            coyieldExpr(unless(isExpansionInSystemHeader())).bind("coroutine")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 coreturnStmt(unless(isExpansionInSystemHeader()))
                     .bind("coroutine")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const auto *S = Result.Nodes.getNodeAs<Stmt>("coroutine");
    if (const auto *Await = dyn_cast<CoawaitExpr>(S);
        Await && Await->isImplicit())
      return;
    const SourceManager &SM = *Result.SourceManager;
    const LangOptions &Lang = Result.Context->getLangOpts();
    if (!spellsToken(S->getBeginLoc(), "co_await", SM, Lang) &&
        !spellsToken(S->getBeginLoc(), "co_yield", SM, Lang) &&
        !spellsToken(S->getBeginLoc(), "co_return", SM, Lang))
      return;
    if (firstAt(S->getBeginLoc(), *Result.SourceManager))
      diag(S->getBeginLoc(), "application coroutines are excluded; use the "
                             "explicit task or I/O policy");
  }
};

class AssumptionsCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                stmt(isAttributedStatement(),
                                     unless(isExpansionInSystemHeader()))
                                    .bind("attributes")),
                       this);
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                declRefExpr(to(functionDecl()),
                                            unless(isExpansionInSystemHeader()))
                                    .bind("function")),
                       this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *S = Result.Nodes.getNodeAs<AttributedStmt>("attributes")) {
      for (const Attr *Attribute : S->getAttrs())
        if (isa<CXXAssumeAttr>(Attribute))
          diag(
              Attribute->getLocation(),
              "unchecked assume attributes are banned; use a checked contract");
    }
    if (const auto *Reference =
            Result.Nodes.getNodeAs<DeclRefExpr>("function")) {
      const auto *FD = cast<FunctionDecl>(Reference->getDecl());
      const std::string Name = qualifiedName(FD);
      if ((FD->getBuiltinID() && (FD->getName() == "__builtin_assume" ||
                                  FD->getName() == "__builtin_unreachable")) ||
          Name == "std::unreachable")
        diag(Reference->getBeginLoc(),
             "unchecked assumptions and unreachable declarations are banned; "
             "use a checked contract");
    }
  }
};

class VarargsCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource, typeLoc().bind("prototype")),
        this);
    // Callable templates need resolved instantiations to expose C varargs.
    Finder->addMatcher(
        callExpr(unless(isExpansionInSystemHeader())).bind("call"), this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 stmt(isVarargAccess(), unless(isExpansionInSystemHeader()))
                     .bind("argument")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *Loc = Result.Nodes.getNodeAs<TypeLoc>("prototype"))
      if (const auto Prototype = Loc->getAs<FunctionProtoTypeLoc>();
          Prototype && Prototype.getTypePtr()->isVariadic())
        if (firstAt(Prototype.getRParenLoc(), *Result.SourceManager))
          diag(Prototype.getRParenLoc(),
               "C variadic function types require a required C adapter; use "
               "explicit typed parameters");
    if (const auto *Loc = Result.Nodes.getNodeAs<TypeLoc>("prototype"))
      if (const auto Alias = Loc->getAs<TypedefTypeLoc>();
          Alias && Alias.getTypePtr()->getDecl()->getName() == "va_list" &&
          Result.SourceManager->isInSystemHeader(
              Alias.getTypePtr()->getDecl()->getLocation()))
        if (firstAt(Loc->getBeginLoc(), *Result.SourceManager))
          diag(Loc->getBeginLoc(),
               "va_list machinery requires a required C adapter");
    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call")) {
      if (Call->getCallee()->isTypeDependent())
        return;
      const FunctionDecl *FD = Call->getDirectCallee();
      const std::string Name = FD ? FD->getNameAsString() : "";
      const bool VaBuiltin =
          FD && FD->getBuiltinID() &&
          (Name == "__builtin_va_start" || Name == "__builtin_c23_va_start" ||
           Name == "__builtin_va_end" || Name == "__builtin_va_copy");
      // A nonstatic member callee has bound-member type in the expression;
      // its declaration still carries the actual function prototype.
      QualType CalleeType = FD ? FD->getType() : Call->getCallee()->getType();
      if (CalleeType->isSpecificBuiltinType(BuiltinType::BoundMember))
        CalleeType = Expr::findBoundMemberType(Call->getCallee());
      if (CalleeType.isNull())
        return;
      if (CalleeType->isPointerType())
        CalleeType = CalleeType->getPointeeType();
      const auto *Prototype = CalleeType->getAs<FunctionProtoType>();
      const bool Intrinsic =
          FD && FD->getBuiltinID() &&
          StringRef(Name).starts_with("__builtin_") &&
          (!Result.Context->BuiltinInfo.isLibFunction(FD->getBuiltinID()) ||
           Result.Context->BuiltinInfo.hasCustomTypechecking(
               FD->getBuiltinID()));
      if (VaBuiltin || (!Intrinsic && Prototype && Prototype->isVariadic())) {
        const SourceManager &SM = *Result.SourceManager;
        const SourceLocation Loc = Call->getExprLoc();
        // Public va_* macros intentionally expose system argument machinery;
        // other dependency implementation calls are not authored uses.
        if (!VaBuiltin && SM.isInSystemHeader(SM.getSpellingLoc(Loc)))
          return;
        if (userLocation(Loc, SM) &&
            SeenCalls.insert(Loc.getRawEncoding()).second)
          diag(VaBuiltin ? SM.getExpansionLoc(Loc) : Loc,
               "C variadic calls and argument machinery "
               "require a required C adapter");
      }
    }
    if (const auto *E = Result.Nodes.getNodeAs<VAArgExpr>("argument"))
      diag(Result.SourceManager->getExpansionLoc(E->getBeginLoc()),
           "C varargs access requires a required C adapter");
  }

private:
  // Instantiations share an authored token; distinct macro arguments do not.
  llvm::DenseSet<unsigned> SeenCalls;
};

AST_MATCHER(Stmt, isIndirectGoto) { return isa<IndirectGotoStmt>(Node); }
class ExtensionsCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                stmt(anyOf(stmtExpr(), stmt(isIndirectGoto()),
                                           addrLabelExpr()),
                                     unless(isExpansionInSystemHeader()))
                                    .bind("extension")),
                       this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource, typeLoc().bind("array-type")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *S = Result.Nodes.getNodeAs<Stmt>("extension"))
      diag(S->getBeginLoc(),
           "GNU statement expressions and computed goto machinery are banned");
    if (const auto *Loc = Result.Nodes.getNodeAs<TypeLoc>("array-type"))
      if (Loc->getType()->isVariableArrayType() &&
          firstAt(Loc->getBeginLoc(), *Result.SourceManager))
        diag(Loc->getBeginLoc(),
             "variable-length arrays and variably modified types are banned");
  }
};

bool unevaluatedExpectedUse(const Expr *E, ASTContext &Context) {
  if (ExprMutationAnalyzer::isUnevaluated(E, Context))
    return true;
  // Clang's mutation analysis handles decltype/sizeof/noexcept, but does not
  // classify requires-expression operands as unevaluated in this LLVM version.
  llvm::SmallVector<std::pair<DynTypedNode, unsigned>, 8> Work;
  Work.push_back({DynTypedNode::create(*E), 0});
  while (!Work.empty()) {
    auto [Node, Depth] = Work.pop_back_val();
    if (Depth > 64)
      continue;
    for (const DynTypedNode &Parent : Context.getParents(Node)) {
      if (Parent.get<RequiresExpr>())
        return true;
      if (!Parent.get<FunctionDecl>() && !Parent.get<LambdaExpr>())
        Work.push_back({Parent, Depth + 1});
    }
  }
  return false;
}

class ExpectedCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    // Keep resolved template instantiations: dependent value members and
    // deduced returns cannot be classified from the source pattern alone.
    Finder->addMatcher(
        functionDecl(unless(isImplicit()), unless(isExpansionInSystemHeader()))
            .bind("function"),
        this);
    Finder->addMatcher(memberExpr(member(cxxMethodDecl(hasName("value"))),
                                  unless(isExpansionInSystemHeader()))
                           .bind("value"),
                       this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *FD = Result.Nodes.getNodeAs<FunctionDecl>("function")) {
      QualType Return = FD->getReturnType();
      if (!Return->isReferenceType() && stdTemplateType(Return, "expected") &&
          !FD->hasAttr<WarnUnusedResultAttr>() &&
          userLocation(FD->getLocation(), *Result.SourceManager) &&
          SeenUses.insert(FD->getLocation().getRawEncoding()).second)
        diag(FD->getLocation(), "fallible APIs returning std::expected must be "
                                "marked [[nodiscard]]");
    }
    if (const auto *Member = Result.Nodes.getNodeAs<MemberExpr>("value")) {
      const auto *Method = cast<CXXMethodDecl>(Member->getMemberDecl());
      if (stdTemplate(Method->getParent(), "expected") &&
          !unevaluatedExpectedUse(Member, *Result.Context) &&
          userLocation(Member->getMemberLoc(), *Result.SourceManager) &&
          SeenUses.insert(Member->getMemberLoc().getRawEncoding()).second)
        diag(Member->getMemberLoc(),
             "std::expected::value must not implement ordinary propagation; "
             "use explicit checks and error results");
    }
  }

private:
  // Instantiations reuse a source token; separate macro argument tokens must
  // remain separate uses even when they share the outer expansion location.
  llvm::DenseSet<unsigned> SeenUses;
};

class LowLevelCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 decl(unless(isImplicit()), unless(isExpansionInSystemHeader()))
                     .bind("declaration")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource, typeLoc().bind("type")), this);
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                stmt(isRepresentationBuiltin(),
                                     unless(isExpansionInSystemHeader()))
                                    .bind("builtin-expression")),
                       this);
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                declRefExpr(to(functionDecl()),
                                            unless(isExpansionInSystemHeader()))
                                    .bind("function")),
                       this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const SourceManager &SM = *Result.SourceManager;
    if (const auto *Builtin =
            Result.Nodes.getNodeAs<Stmt>("builtin-expression"))
      if (firstAt(Builtin->getBeginLoc(), SM))
        diag(Builtin->getBeginLoc(), "builtin representation access requires a "
                                     "named low-level boundary");
    if (const auto *D = Result.Nodes.getNodeAs<Decl>("declaration"))
      for (const Attr *A : D->attrs())
        if (!A->isImplicit() &&
            isa<AlignedAttr, PackedAttr, NoUniqueAddressAttr,
                MaxFieldAlignmentAttr>(A))
          if (firstAt(A->getLocation(), SM))
            diag(A->getLocation(),
                 "layout attributes require a named low-level boundary with "
                 "explicit layout, alignment, and lifetime requirements");
    if (const auto *Loc = Result.Nodes.getNodeAs<TypeLoc>("type"))
      if (Loc->getType().isRestrictQualified() &&
          firstAt(Loc->getBeginLoc(), SM))
        diag(Loc->getBeginLoc(),
             "restrict qualifiers require a reviewed low-level implementation "
             "with explicit aliasing requirements");
    if (const auto *Ref = Result.Nodes.getNodeAs<DeclRefExpr>("function")) {
      const auto *FD = cast<FunctionDecl>(Ref->getDecl());
      if (!FD->getIdentifier())
        return;
      StringRef Name = FD->getName();
      if (!spellsToken(Ref->getLocation(), Name, SM,
                       Result.Context->getLangOpts()))
        return;
      if (Name == "__builtin_assume" || Name == "__builtin_unreachable" ||
          Name == "__builtin_setjmp" || Name == "__builtin_longjmp" ||
          Name == "__builtin_va_start" || Name == "__builtin_c23_va_start" ||
          Name == "__builtin_va_end" || Name == "__builtin_va_copy")
        return;
      if ((FD->getBuiltinID() && Name.starts_with("__builtin_")) ||
          (SM.isInSystemHeader(FD->getLocation()) &&
           (Name.starts_with("_mm_") || Name.starts_with("_mm256_") ||
            Name.starts_with("_mm512_"))))
        diag(Ref->getLocation(),
             "intrinsics and compiler builtins require a reviewed low-level "
             "implementation with explicit preconditions and evidence");
    }
  }
};

class KnownNarrowingCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 cxxStaticCastExpr(unless(isExpansionInSystemHeader()))
                     .bind("cast")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const auto *Cast = Result.Nodes.getNodeAs<CXXStaticCastExpr>("cast");
    QualType Target = Cast->getType();
    const Expr *Operand = Cast->getSubExpr()->IgnoreParenImpCasts();
    if (Target->isDependentType() || !Target->isIntegerType() ||
        Target->isBooleanType() || !Operand->getType()->isIntegerType() ||
        Operand->isValueDependent())
      return;
    Expr::EvalResult Evaluation;
    if (!Operand->EvaluateAsInt(Evaluation, *Result.Context) ||
        Evaluation.HasSideEffects || Evaluation.HasUndefinedBehavior)
      return;
    const llvm::APSInt &Value = Evaluation.Val.getInt();
    unsigned Bits = Result.Context->getIntWidth(Target);
    bool Fits = Target->isUnsignedIntegerType()
                    ? !Value.isNegative() && Value.getActiveBits() <= Bits
                    : (Value.isNegative() ? Value.isSignedIntN(Bits)
                                          : Value.getActiveBits() < Bits);
    if (!Fits)
      diag(Cast->getBeginLoc(),
           "explicit integral conversion has a constant operand outside the "
           "target range; establish that the value fits");
  }
};

class PointerArithmeticCheck final : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    // Include resolved project instantiations, then verify that the operator
    // itself was written. This finds T arithmetic instantiated with pointers.
    Finder->addMatcher(
        binaryOperator(unless(isExpansionInSystemHeader())).bind("binary"),
        this);
    Finder->addMatcher(
        unaryOperator(unless(isExpansionInSystemHeader())).bind("unary"), this);
    Finder->addMatcher(arraySubscriptExpr(unless(isExpansionInSystemHeader()))
                           .bind("subscript"),
                       this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    SourceLocation Loc;
    StringRef Token;
    if (const auto *Binary = Result.Nodes.getNodeAs<BinaryOperator>("binary")) {
      if (Binary->getOpcode() != BO_Add && Binary->getOpcode() != BO_Sub &&
          Binary->getOpcode() != BO_AddAssign &&
          Binary->getOpcode() != BO_SubAssign)
        return;
      if (!Binary->getLHS()->getType()->isPointerType() &&
          !Binary->getRHS()->getType()->isPointerType())
        return;
      Loc = Binary->getOperatorLoc();
      Token = Binary->getOpcodeStr();
    }
    if (const auto *Unary = Result.Nodes.getNodeAs<UnaryOperator>("unary")) {
      if (!Unary->isIncrementDecrementOp() ||
          !Unary->getSubExpr()->getType()->isPointerType())
        return;
      Loc = Unary->getOperatorLoc();
      Token = UnaryOperator::getOpcodeStr(Unary->getOpcode());
    }
    if (const auto *Subscript =
            Result.Nodes.getNodeAs<ArraySubscriptExpr>("subscript")) {
      if (!Subscript->getBase()
               ->IgnoreParenImpCasts()
               ->getType()
               ->isPointerType())
        return;
      Loc = Subscript->getRBracketLoc();
      Token = "]";
    }
    if (Loc.isValid() &&
        spellsToken(Loc, Token, *Result.SourceManager,
                    Result.Context->getLangOpts()) &&
        firstAt(Loc, *Result.SourceManager))
      diag(Loc, "raw pointer arithmetic and indexing are banned; use a bounded "
                "view or an explicit borrow");
  }
};
} // namespace

void registerLanguageChecks(ClangTidyCheckFactories &Factories) {
  Factories.registerCheck<InheritanceCheck>("narrow-inheritance");
  Factories.registerCheck<InheritanceCheck>("narrow-no-complex-inheritance");
  Factories.registerCheck<UnionCheck>("narrow-restricted-unions");
  Factories.registerCheck<ExplicitConversionsCheck>(
      "narrow-explicit-conversions");
  Factories.registerCheck<GlobalStateCheck>("narrow-restricted-globals");
  Factories.registerCheck<ExceptionsCheck>("narrow-restricted-exceptions");
  Factories.registerCheck<ExceptionsCheck>("narrow-no-application-throw");
  Factories.registerCheck<ExceptionsCheck>("narrow-exceptions-enabled");
  Factories.registerCheck<CoroutinesCheck>("narrow-no-coroutines");
  Factories.registerCheck<AssumptionsCheck>("narrow-no-assumptions");
  Factories.registerCheck<VarargsCheck>("narrow-restricted-c-varargs");
  Factories.registerCheck<ExtensionsCheck>("narrow-no-extensions");
  Factories.registerCheck<ExpectedCheck>("narrow-expected-usage");
  Factories.registerCheck<LowLevelCheck>("narrow-restricted-low-level");
  Factories.registerCheck<KnownNarrowingCheck>("narrow-known-narrowing");
  Factories.registerCheck<PointerArithmeticCheck>(
      "narrow-no-pointer-arithmetic");
}
} // namespace clang::tidy::narrow
