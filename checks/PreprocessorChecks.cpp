#include "PolicyChecks.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"

namespace clang::tidy::narrow {
namespace {
class PreprocessorCheck final : public PolicyCheck {
public:
  PreprocessorCheck(StringRef Name, ClangTidyContext *Context)
      : PolicyCheck(Name, Context),
        MacrosOnly(Name == "narrow-no-function-macros") {}
  const bool MacrosOnly;
  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
};

class PolicyCallbacks final : public PPCallbacks {
public:
  PolicyCallbacks(PreprocessorCheck &Check, Preprocessor &PP)
      : Check(Check), PP(PP), SM(PP.getSourceManager()) {}

  void MacroDefined(const Token &Name,
                    const MacroDirective *Directive) override {
    if (Check.MacrosOnly && userLocation(Name.getLocation(), SM) &&
        Directive->getMacroInfo()->isFunctionLike())
      Check.diag(
          Name.getLocation(),
          "application function-like macros are banned; compatibility, "
          "checked-contract, and test macros need a local justification");
  }
  void If(SourceLocation Loc, SourceRange Condition,
          ConditionValueKind) override {
    if (!Check.MacrosOnly && userLocation(Loc, SM)) {
      auto [File, Begin] = SM.getDecomposedLoc(Condition.getBegin());
      StringRef Buffer = SM.getBufferData(File);
      Lexer Raw(SM.getLocForStartOfFile(File), PP.getLangOpts(), Buffer.begin(),
                Buffer.begin() + Begin, Buffer.end());
      Raw.SetCommentRetentionState(false);
      const unsigned End = SM.getFileOffset(Condition.getEnd());
      Token Tok;
      std::string Text;
      while (!Raw.LexFromRawLexer(Tok) &&
             SM.getFileOffset(Tok.getLocation()) <= End)
        Text += Lexer::getSpelling(Tok, SM, PP.getLangOpts());
      Pending.push_back({Loc, nullptr, std::move(Text)});
    }
  }
  void Ifdef(SourceLocation Loc, const Token &,
             const MacroDefinition &) override {
    record(Loc);
  }
  void Ifndef(SourceLocation Loc, const Token &Name,
              const MacroDefinition &) override {
    if (!Check.MacrosOnly && userLocation(Loc, SM))
      Pending.push_back({Loc, Name.getIdentifierInfo(), ""});
  }
  void Elif(SourceLocation Loc, SourceRange, ConditionValueKind,
            SourceLocation) override {
    record(Loc);
  }
  void Elifdef(SourceLocation Loc, const Token &,
               const MacroDefinition &) override {
    record(Loc);
  }
  void Elifndef(SourceLocation Loc, const Token &,
                const MacroDefinition &) override {
    record(Loc);
  }
  void Elifdef(SourceLocation Loc, SourceRange, SourceLocation) override {
    record(Loc);
  }
  void Elifndef(SourceLocation Loc, SourceRange, SourceLocation) override {
    record(Loc);
  }
  void SourceRangeSkipped(SourceRange Range, SourceLocation) override {
    if (Check.MacrosOnly)
      return;
    auto [File, Begin] = SM.getDecomposedLoc(Range.getBegin());
    auto [EndFile, End] = SM.getDecomposedLoc(Range.getEnd());
    if (File != EndFile || !userLocation(Range.getBegin(), SM))
      return;
    StringRef Buffer = SM.getBufferData(File);
    Lexer Raw(SM.getLocForStartOfFile(File), PP.getLangOpts(), Buffer.begin(),
              Buffer.begin() + Begin, Buffer.end());
    Token Tok;
    while (!Raw.LexFromRawLexer(Tok)) {
      if (SM.getFileOffset(Tok.getLocation()) > End)
        break;
      if (!Tok.is(tok::hash) || !Tok.isAtStartOfLine())
        continue;
      if (Raw.LexFromRawLexer(Tok))
        break;
      const std::string Directive =
          Lexer::getSpelling(Tok, SM, PP.getLangOpts());
      if (Directive == "if" || Directive == "ifdef" || Directive == "ifndef" ||
          Directive == "elif" || Directive == "elifdef" ||
          Directive == "elifndef")
        record(Tok.getLocation());
    }
  }

  void EndOfMainFile() override {
    if (Check.MacrosOnly)
      return;
    llvm::DenseSet<unsigned> ExemptedGuards;
    llvm::DenseSet<unsigned> Processed;
    for (const Conditional &C : Pending) {
      if (!Processed.insert(C.Loc.getRawEncoding()).second)
        continue;
      // Recognize the compiler's actual controlling macro, not a guessed naming
      // convention. Other conditionals in a guarded header still get checked.
      if (auto File = SM.getFileEntryRefForID(SM.getFileID(C.Loc))) {
        const IdentifierInfo *Guard =
            PP.getHeaderSearchInfo().getFileInfo(*File).getControllingMacro(
                PP.getExternalSource());
        if (Guard &&
            !ExemptedGuards.contains(SM.getFileID(C.Loc).getHashValue()) &&
            (C.NegativeMacro == Guard ||
             C.Text == ("!defined(" + Guard->getName() + ")").str() ||
             C.Text == ("!defined" + Guard->getName()).str())) {
          ExemptedGuards.insert(SM.getFileID(C.Loc).getHashValue());
          continue;
        }
      }
      Check.diag(C.Loc,
                 "conditional compilation requires build, platform, or "
                 "compatibility code; keep application control flow in C++");
    }
  }

private:
  struct Conditional {
    SourceLocation Loc;
    const IdentifierInfo *NegativeMacro;
    std::string Text;
  };
  void record(SourceLocation Loc) {
    if (!Check.MacrosOnly && userLocation(Loc, SM))
      Pending.push_back({Loc, nullptr, ""});
  }
  PreprocessorCheck &Check;
  Preprocessor &PP;
  const SourceManager &SM;
  std::vector<Conditional> Pending;
};

void PreprocessorCheck::registerPPCallbacks(const SourceManager &,
                                            Preprocessor *PP,
                                            Preprocessor *ModuleExpanderPP) {
  Preprocessor *Active = ModuleExpanderPP ? ModuleExpanderPP : PP;
  Active->addPPCallbacks(std::make_unique<PolicyCallbacks>(*this, *Active));
}
} // namespace

void registerPreprocessorChecks(ClangTidyCheckFactories &Factories) {
  Factories.registerCheck<PreprocessorCheck>("narrow-restricted-preprocessor");
  Factories.registerCheck<PreprocessorCheck>("narrow-no-function-macros");
}
} // namespace clang::tidy::narrow
