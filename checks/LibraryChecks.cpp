#include "PolicyChecks.h"
#include "clang/AST/TypeLoc.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "llvm/ADT/StringSwitch.h"
#include <algorithm>

namespace clang::tidy::narrow {
namespace {
using namespace ast_matchers;

StringRef excludedName(StringRef Name) {
  if (Name.starts_with("std::execution::") ||
      Name.starts_with("__pstl::execution::"))
    return "standard execution facilities";
  if (Name.starts_with("std::placeholders::"))
    return "placeholder expressions";
  if (Name.starts_with("std::meta::"))
    return "application reflection";
  if (Name.starts_with("std::ranges::views::") &&
      !Name.drop_front(StringRef("std::ranges::views::").size())
           .contains("::") &&
      !Name.drop_front(StringRef("std::ranges::views::").size())
           .starts_with("_") &&
      Name != "std::ranges::views::all" &&
      Name != "std::ranges::views::all_t" &&
      Name != "std::ranges::views::empty" &&
      Name != "std::ranges::views::single" &&
      Name != "std::ranges::views::counted")
    return "lazy range adaptors";
  return llvm::StringSwitch<StringRef>(Name)
      .Cases({"std::any", "std::any_cast", "std::make_any"},
             "open-ended dynamic values")
      .Cases({"std::type_info", "std::type_index"}, "runtime type information")
      .Cases({"std::async", "std::future", "std::shared_future"},
             "standard task/future facilities")
      .Cases({"std::promise", "std::packaged_task", "std::launch"},
             "standard task/future facilities")
      .Cases({"std::future_status", "std::future_errc", "std::future_error"},
             "standard task/future facilities")
      .Cases(
          {"std::coroutine_handle", "std::coroutine_traits", "std::generator"},
          "coroutine machinery")
      .Cases(
          {"std::suspend_always", "std::suspend_never", "std::noop_coroutine"},
          "coroutine machinery")
      .Cases({"std::noop_coroutine_handle", "std::noop_coroutine_promise"},
             "coroutine machinery")
      .Case("std::bind", "std::bind")
      .Cases({"std::hazard_pointer", "std::hazard_pointer_obj_base",
              "std::make_hazard_pointer"},
             "hazard-pointer reclamation")
      .Cases(
          {"std::rcu_domain", "std::rcu_obj_base", "std::rcu_default_domain"},
          "RCU reclamation")
      .Cases({"std::rcu_synchronize", "std::rcu_barrier"}, "RCU reclamation")
      .Cases({"std::ranges::filter_view", "std::ranges::transform_view",
              "std::ranges::join_view"},
             "lazy ranges")
      .Cases({"std::ranges::join_with_view", "std::ranges::split_view",
              "std::ranges::lazy_split_view"},
             "lazy ranges")
      .Cases({"std::ranges::take_view", "std::ranges::take_while_view",
              "std::ranges::drop_view"},
             "lazy ranges")
      .Cases({"std::ranges::drop_while_view", "std::ranges::reverse_view",
              "std::ranges::common_view"},
             "lazy ranges")
      .Cases({"std::ranges::elements_view", "std::ranges::iota_view",
              "std::ranges::repeat_view"},
             "lazy ranges")
      .Cases({"std::ranges::zip_view", "std::ranges::zip_transform_view",
              "std::ranges::adjacent_view"},
             "lazy ranges")
      .Cases({"std::ranges::adjacent_transform_view", "std::ranges::chunk_view",
              "std::ranges::chunk_by_view"},
             "lazy ranges")
      .Cases({"std::ranges::slide_view", "std::ranges::stride_view",
              "std::ranges::cartesian_product_view"},
             "lazy ranges")
      .Cases({"std::ranges::as_rvalue_view", "std::ranges::as_const_view",
              "std::ranges::concat_view"},
             "lazy ranges")
      .Case("std::ranges::basic_istream_view", "lazy ranges")
      .Default("");
}

StringRef excludedRecord(const CXXRecordDecl *RD) {
  if (!RD)
    return "";
  if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(RD)) {
    StringRef Reason =
        excludedName(qualifiedName(Spec->getSpecializedTemplate()));
    if (!Reason.empty())
      return Reason;
    if (stdTemplate(Spec, "vector") && Spec->getTemplateArgs().size() &&
        Spec->getTemplateArgs()[0].getKind() == TemplateArgument::Type &&
        Spec->getTemplateArgs()[0].getAsType()->isBooleanType())
      return "std::vector<bool>";
  }
  return excludedName(qualifiedName(RD));
}

StringRef sfinaeName(StringRef Name) {
  return llvm::StringSwitch<StringRef>(Name)
      .Cases({"std::enable_if", "std::enable_if_t"}, "enable_if")
      .Cases({"std::experimental::is_detected",
              "std::experimental::is_detected_v"},
             "detection frameworks")
      .Cases(
          {"std::experimental::detected_t", "std::experimental::detected_or"},
          "detection frameworks")
      .Cases({"std::experimental::is_detected_exact",
              "std::experimental::is_detected_convertible"},
             "detection frameworks")
      .Cases({"std::experimental::is_detected_exact_v",
              "std::experimental::is_detected_convertible_v"},
             "detection frameworks")
      .Case("std::experimental::detected_or_t", "detection frameworks")
      .Default("");
}

// The same semantic use sites feed independent diagnostic families, so a local
// permission can name exactly the rule being justified.
class LibraryCheck : public PolicyCheck {
public:
  using PolicyCheck::PolicyCheck;
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource, typeLoc().bind("type")), this);
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            declRefExpr(unless(isExpansionInSystemHeader())).bind("reference")),
        this);
    // Resolved project template instances can introduce excluded facilities
    // that were not identifiable in their dependent source pattern.
    Finder->addMatcher(varDecl(unless(isExpansionInSystemHeader()))
                           .bind("instantiated-variable"),
                       this);
    Finder->addMatcher(fieldDecl(unless(isExpansionInSystemHeader()))
                           .bind("instantiated-field"),
                       this);
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            memberExpr(unless(isExpansionInSystemHeader())).bind("member")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 usingDecl(unless(isExpansionInSystemHeader())).bind("using")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 unresolvedLookupExpr(unless(isExpansionInSystemHeader()))
                     .bind("lookup")),
        this);
    Finder->addMatcher(
        traverse(
            TK_IgnoreUnlessSpelledInSource,
            callExpr(unless(isExpansionInSystemHeader())).bind("call-result")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    const SourceManager &SM = *Result.SourceManager;
    if (const auto *Loc = Result.Nodes.getNodeAs<TypeLoc>("type")) {
      if (!userLocation(Loc->getBeginLoc(), SM) || Loc->getAs<AutoTypeLoc>() ||
          (!Loc->getNextTypeLoc().isNull() &&
           !Loc->getType().hasLocalQualifiers()))
        return;
      if (const auto TemplateLoc = Loc->getAs<TemplateSpecializationTypeLoc>())
        if (const TemplateDecl *TD =
                TemplateLoc.getTypePtr()->getTemplateName().getAsTemplateDecl())
          namedUse(TD, Loc->getBeginLoc(), Result);
      recordUse(Loc->getType()->getAsCXXRecordDecl(), Loc->getBeginLoc(),
                Result);
      if (Loc->getAs<TypedefTypeLoc>() || Loc->getAs<UsingTypeLoc>())
        emit(typeReason(Loc->getType()), Loc->getBeginLoc(), Result);
      if (const auto *Tag = Loc->getType()->getAsTagDecl())
        namedUse(Tag, Loc->getBeginLoc(), Result);
    }
    if (const auto *Ref = Result.Nodes.getNodeAs<DeclRefExpr>("reference"))
      if (spelledName(Ref->getDecl(), Ref->getLocation(), Result))
        namedUse(Ref->getDecl(), Ref->getLocation(), Result);
    if (const auto *Member = Result.Nodes.getNodeAs<MemberExpr>("member"))
      if (spelledName(Member->getMemberDecl(), Member->getMemberLoc(), Result))
        namedUse(Member->getMemberDecl(), Member->getMemberLoc(), Result);
    if (const auto *Using = Result.Nodes.getNodeAs<UsingDecl>("using"))
      for (const UsingShadowDecl *Shadow : Using->shadows())
        namedUse(Shadow->getTargetDecl(), Using->getLocation(), Result);
    if (const auto *Lookup =
            Result.Nodes.getNodeAs<UnresolvedLookupExpr>("lookup"))
      for (const NamedDecl *Candidate : Lookup->decls())
        namedUse(Candidate->getUnderlyingDecl(), Lookup->getNameLoc(), Result);
    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call-result")) {
      const FunctionDecl *FD = Call->getDirectCallee();
      const Expr *Callee = Call->getCallee()->IgnoreParenImpCasts();
      SourceLocation Loc;
      const NamedDecl *Spelled = nullptr;
      if (const auto *Reference = dyn_cast<DeclRefExpr>(Callee)) {
        Loc = Reference->getLocation();
        Spelled = Reference->getDecl();
      } else if (const auto *Member = dyn_cast<MemberExpr>(Callee)) {
        Loc = Member->getMemberLoc();
        Spelled = Member->getMemberDecl();
      }
      if (const auto *Operator = dyn_cast<CXXOperatorCallExpr>(Call)) {
        // Standard ranges machinery is an implementation of the separately
        // checked public adaptor use, not another authored facility use.
        if (FD && qualifiedName(FD).starts_with("std::ranges::"))
          return;
        Loc = Operator->getOperatorLoc();
      } else if (!Spelled || Loc.isInvalid() ||
                 !spelledName(Spelled, Loc, Result))
        return;
      emit(typeReason(Call->getType()), Loc, Result);
    }
    const DeclaratorDecl *Instance =
        Result.Nodes.getNodeAs<VarDecl>("instantiated-variable");
    if (!Instance)
      Instance = Result.Nodes.getNodeAs<FieldDecl>("instantiated-field");
    if (Instance && instantiatedContext(Instance->getDeclContext()) &&
        Instance->getTypeSourceInfo())
      emit(typeReason(Instance->getType()),
           Instance->getTypeSourceInfo()->getTypeLoc().getBeginLoc(), Result);
  }

protected:
  bool instantiatedContext(const DeclContext *DC) const {
    for (; DC; DC = DC->getParent()) {
      if (const auto *FD = dyn_cast<FunctionDecl>(DC);
          FD && FD->isTemplateInstantiation())
        return true;
      if (const auto *RD = dyn_cast<ClassTemplateSpecializationDecl>(DC);
          RD && RD->getSpecializationKind() == TSK_ImplicitInstantiation)
        return true;
    }
    return false;
  }
  StringRef typeReason(QualType Type) const {
    llvm::DenseSet<const clang::Type *> Seen;
    return typeReason(Type, Seen);
  }
  StringRef typeReason(QualType Type,
                       llvm::DenseSet<const clang::Type *> &Seen) const {
    if (Type.isNull() || !Seen.insert(Type.getTypePtr()).second)
      return "";
    if (Type->isPointerType() || Type->isReferenceType())
      return typeReason(Type->getPointeeType(), Seen);
    if (const auto *Array =
            dyn_cast<ArrayType>(Type.getCanonicalType().getTypePtr()))
      return typeReason(Array->getElementType(), Seen);
    const CXXRecordDecl *Record = Type->getAsCXXRecordDecl();
    if (StringRef Reason = recordReason(Record); !Reason.empty())
      return Reason;
    if (const auto *Spec =
            dyn_cast_or_null<ClassTemplateSpecializationDecl>(Record))
      for (const TemplateArgument &Argument : Spec->getTemplateArgs().asArray())
        if (StringRef Reason = argumentReason(Argument, Seen); !Reason.empty())
          return Reason;
    return "";
  }
  StringRef argumentReason(const TemplateArgument &Argument,
                           llvm::DenseSet<const clang::Type *> &Seen) const {
    if (Argument.getKind() == TemplateArgument::Type)
      return typeReason(Argument.getAsType(), Seen);
    if (Argument.getKind() == TemplateArgument::Pack)
      for (const TemplateArgument &Element : Argument.pack_elements())
        if (StringRef Reason = argumentReason(Element, Seen); !Reason.empty())
          return Reason;
    return "";
  }
  bool spelledName(const NamedDecl *ND, SourceLocation Loc,
                   const MatchFinder::MatchResult &Result) {
    return ND->getIdentifier() &&
           spellsToken(Loc, ND->getName(), *Result.SourceManager,
                       Result.Context->getLangOpts());
  }
  virtual StringRef nameReason(StringRef Name) const = 0;
  virtual StringRef recordReason(const CXXRecordDecl *) const { return ""; }
  virtual StringRef message() const = 0;
  void namedUse(const NamedDecl *ND, SourceLocation Loc,
                const MatchFinder::MatchResult &Result) {
    if (!ND)
      return;
    StringRef Reason = nameReason(qualifiedName(ND));
    for (const DeclContext *DC = ND->getDeclContext(); Reason.empty() && DC;
         DC = DC->getParent())
      if (const auto *RD = dyn_cast<CXXRecordDecl>(DC))
        Reason = recordReason(RD);
    if (Reason.empty())
      if (const auto *VD = dyn_cast<ValueDecl>(ND))
        Reason = typeReason(VD->getType());
    emit(Reason, Loc, Result);
  }
  void recordUse(const CXXRecordDecl *RD, SourceLocation Loc,
                 const MatchFinder::MatchResult &Result) {
    emit(recordReason(RD), Loc, Result);
  }
  void emit(StringRef Reason, SourceLocation Loc,
            const MatchFinder::MatchResult &Result) {
    if (!Reason.empty() && firstAt(Loc, *Result.SourceManager))
      diag(Loc, message()) << Reason;
  }
};

class ExcludedLibraryCheck final : public LibraryCheck {
public:
  using LibraryCheck::LibraryCheck;
  StringRef nameReason(StringRef Name) const override {
    return excludedName(Name);
  }
  StringRef recordReason(const CXXRecordDecl *RD) const override {
    return excludedRecord(RD);
  }
  StringRef message() const override {
    return "%0 are excluded from application code; only necessary "
           "compatibility or explicitly permitted boundaries may use them";
  }
};

class SfinaeCheck final : public LibraryCheck {
public:
  using LibraryCheck::LibraryCheck;
  StringRef nameReason(StringRef Name) const override {
    return sfinaeName(Name);
  }
  StringRef recordReason(const CXXRecordDecl *RD) const override {
    if (const auto *Spec =
            dyn_cast_or_null<ClassTemplateSpecializationDecl>(RD))
      return sfinaeName(qualifiedName(Spec->getSpecializedTemplate()));
    return "";
  }
  StringRef message() const override {
    return "%0 are banned; use simple concepts and requires clauses";
  }
};

class RestrictedLibraryCheck final : public LibraryCheck {
public:
  using LibraryCheck::LibraryCheck;
  StringRef nameReason(StringRef Name) const override {
    if (Name.starts_with("std::pmr::"))
      return "polymorphic allocation facilities";
    if (Name.starts_with("std::linalg::"))
      return "linear algebra facilities";
    return llvm::StringSwitch<StringRef>(Name)
        .Case("std::function", "std::function compatibility storage")
        .Cases({"std::list", "std::forward_list", "std::hive"},
               "linked/stable container facilities")
        .Cases({"std::mdspan", "std::valarray"},
               "specialized numerical facilities")
        .Cases({"std::tuple", "std::make_tuple", "std::tie"},
               "tuple-based generic adapters")
        .Cases({"std::scoped_allocator_adaptor", "std::set_new_handler",
                "std::get_new_handler"},
               "allocation facilities")
        .Cases({"std::allocate_shared", "std::allocate_shared_for_overwrite"},
               "allocation facilities")
        .Cases(
            {"std::construct_at", "std::destroy_at", "std::start_lifetime_as"},
            "raw lifetime operations")
        .Cases({"std::start_lifetime_as_array", "std::assume_aligned"},
               "raw lifetime/alignment operations")
        .Cases({"std::bit_cast", "std::as_bytes", "std::as_writable_bytes"},
               "representation access")
        .Case("std::locale::global", "global locale mutation")
        .Default("");
  }
  StringRef recordReason(const CXXRecordDecl *RD) const override {
    if (const auto *Spec =
            dyn_cast_or_null<ClassTemplateSpecializationDecl>(RD))
      return nameReason(qualifiedName(Spec->getSpecializedTemplate()));
    return RD ? nameReason(qualifiedName(RD)) : StringRef();
  }
  StringRef message() const override {
    return "%0 are restricted; document the qualifying purpose locally under "
           "Narrow C++";
  }
};

class LegacyCallsCheck final : public PolicyCheck {
public:
  LegacyCallsCheck(StringRef Name, ClangTidyContext *Context)
      : PolicyCheck(Name, Context),
        UnsafeOnly(Name == "narrow-no-unsafe-c-library") {}
  void registerMatchers(MatchFinder *Finder) override {
    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                declRefExpr(to(functionDecl()),
                                            unless(isExpansionInSystemHeader()))
                                    .bind("function")),
                       this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 callExpr(unless(isExpansionInSystemHeader())).bind("call")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 usingDecl(unless(isExpansionInSystemHeader())).bind("using")),
        this);
    Finder->addMatcher(
        traverse(TK_IgnoreUnlessSpelledInSource,
                 unresolvedLookupExpr(unless(isExpansionInSystemHeader()))
                     .bind("lookup")),
        this);
  }
  void check(const MatchFinder::MatchResult &Result) override {
    if (const auto *Ref = Result.Nodes.getNodeAs<DeclRefExpr>("function"))
      functionUse(Ref->getDecl(), Ref->getLocation(), Result);
    // C operations have one classification owner for both std-owned and
    // global declarations imported into std, including non-call uses.
    if (const auto *Using = Result.Nodes.getNodeAs<UsingDecl>("using"))
      for (const UsingShadowDecl *Shadow : Using->shadows())
        functionUse(Shadow->getTargetDecl(), Using->getLocation(), Result);
    if (const auto *Lookup =
            Result.Nodes.getNodeAs<UnresolvedLookupExpr>("lookup"))
      for (const NamedDecl *Candidate : Lookup->decls())
        functionUse(Candidate->getUnderlyingDecl(), Lookup->getNameLoc(),
                    Result);
    if (const auto *Call = Result.Nodes.getNodeAs<CallExpr>("call")) {
      if (UnsafeOnly)
        return;
      const FunctionDecl *FD = Call->getDirectCallee();
      if (!FD || Call->getNumArgs() < 2 || FD->getNameAsString() != "setlocale")
        return;
      if (qualifiedName(FD) != "std::setlocale" &&
          !cLibraryFunction(FD, *Result.SourceManager))
        return;
      Expr::EvalResult Evaluated;
      const Expr *Argument = Call->getArg(1);
      const bool NullQuery =
          Argument->isNullPointerConstant(*Result.Context,
                                          Expr::NPC_ValueDependentIsNotNull) ||
          (Argument->EvaluateAsRValue(Evaluated, *Result.Context) &&
           Evaluated.Val.isLValue() && Evaluated.Val.isNullPointer());
      if (!NullQuery)
        diag(Call->getExprLoc(),
             "mutating setlocale requires an initialization or locale "
             "boundary; null-pointer queries remain allowed");
    }
  }

private:
  const bool UnsafeOnly;
  // Deduplicate lookup candidates without letting a hidden dependency macro
  // body consume the diagnostic for an authored argument at the same expansion.
  llvm::DenseSet<unsigned> SeenUses;
  enum class COperation {
    None,
    Allocation,
    Bytes,
    Environment,
    Unsafe,
    Sorting,
    Jump
  };
  static COperation classify(StringRef Name) {
    return llvm::StringSwitch<COperation>(Name)
        .Cases({"malloc", "calloc", "realloc", "free", "aligned_alloc"},
               COperation::Allocation)
        .Cases({"memcpy", "memmove", "memset"}, COperation::Bytes)
        .Cases({"fesetround", "fesetenv", "feupdateenv", "feclearexcept",
                "feraiseexcept", "feholdexcept"},
               COperation::Environment)
        .Cases({"rand", "srand", "strcpy", "strcat", "gets", "sprintf",
                "vsprintf"},
               COperation::Unsafe)
        .Case("qsort", COperation::Sorting)
        .Cases({"setjmp", "longjmp", "_setjmp", "__sigsetjmp"},
               COperation::Jump)
        .Default(COperation::None);
  }
  void functionUse(const NamedDecl *ND, SourceLocation Loc,
                   const MatchFinder::MatchResult &Result) {
    const auto *FD = dyn_cast<FunctionDecl>(ND);
    if (!FD)
      return;
    const SourceManager &SM = *Result.SourceManager;
    const std::string Name = qualifiedName(FD);
    StringRef Base = Name;
    COperation Operation = COperation::None;
    if (Base.consume_front("std::") || cLibraryFunction(FD, SM))
      Operation = classify(Base);
    if (FD->getBuiltinID() &&
        (Name == "__builtin_setjmp" || Name == "__builtin_longjmp"))
      Operation = COperation::Jump;
    if (Operation == COperation::None)
      return;
    const bool Unsafe = Operation == COperation::Unsafe ||
                        Operation == COperation::Sorting ||
                        Operation == COperation::Jump;
    if (Unsafe != UnsafeOnly)
      return;
    // Public setjmp expands to a system implementation such as _setjmp.
    // Diagnose that public macro at the caller, retaining ordinary filtering
    // for dependency implementation macros that happen to call C facilities.
    if (Operation == COperation::Jump && Loc.isMacroID() &&
        Lexer::getImmediateMacroName(Loc, SM, Result.Context->getLangOpts()) ==
            "setjmp") {
      SourceLocation Macro = Loc;
      while (SM.isMacroArgExpansion(Macro))
        Macro = SM.getImmediateSpellingLoc(Macro);
      const SourceLocation Caller = SM.getImmediateMacroCallerLoc(Macro);
      if (!SM.isInSystemHeader(SM.getSpellingLoc(Caller)))
        Loc = Caller;
    }
    if (!userLocation(Loc, SM) || !SeenUses.insert(Loc.getRawEncoding()).second)
      return;
    StringRef Reason;
    switch (Operation) {
    case COperation::Allocation:
      Reason = "raw allocation/free";
      break;
    case COperation::Bytes:
      Reason = "raw byte operations";
      break;
    case COperation::Environment:
      Reason = "floating-point environment mutation";
      break;
    case COperation::Unsafe:
      Reason = "unsafe legacy operations";
      break;
    case COperation::Sorting:
      Reason = "C-style sorting callbacks";
      break;
    case COperation::Jump:
      Reason = "nonlocal jumps";
      break;
    case COperation::None:
      return;
    }
    diag(Loc, UnsafeOnly ? "%0 are banned in application code; only explicitly "
                           "permitted compatibility/C boundaries may use them"
                         : "%0 require a qualifying resource, representation, "
                           "locale, or subsystem boundary")
        << Reason;
  }
  static bool cLibraryFunction(const FunctionDecl *FD,
                               const SourceManager &SM) {
    return FD->getDeclContext()->getRedeclContext()->isTranslationUnit() &&
           std::any_of(FD->redecls_begin(), FD->redecls_end(),
                       [&](const FunctionDecl *Decl) {
                         return SM.isInSystemHeader(Decl->getLocation());
                       });
  }
};
} // namespace

void registerLibraryChecks(ClangTidyCheckFactories &Factories) {
  Factories.registerCheck<ExcludedLibraryCheck>("narrow-excluded-library");
  Factories.registerCheck<SfinaeCheck>("narrow-no-sfinae");
  Factories.registerCheck<RestrictedLibraryCheck>("narrow-restricted-library");
  Factories.registerCheck<LegacyCallsCheck>("narrow-legacy-c-library");
  Factories.registerCheck<LegacyCallsCheck>("narrow-no-unsafe-c-library");
}
} // namespace clang::tidy::narrow
