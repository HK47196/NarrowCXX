#pragma once

#include "clang-tidy/ClangTidyCheck.h"
#include "clang-tidy/ClangTidyModule.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/DenseSet.h"

namespace clang::tidy::narrow {

// Ignore inline library namespaces, but retain the actual semantic declaration
// context. Namespace aliases and using declarations cannot change this
// identity.
inline std::string qualifiedName(const NamedDecl *D) {
  std::string Name = D->getNameAsString();
  for (const DeclContext *DC = D->getDeclContext();
       DC && !DC->isTranslationUnit(); DC = DC->getParent()) {
    if (const auto *NS = dyn_cast<NamespaceDecl>(DC); NS && NS->isInline())
      continue;
    if (const auto *ND = dyn_cast<NamedDecl>(DC);
        ND && !ND->getNameAsString().empty())
      Name = ND->getNameAsString() + "::" + Name;
  }
  return Name;
}

inline bool userLocation(SourceLocation Loc, const SourceManager &SM) {
  Loc = SM.getExpansionLoc(Loc);
  return Loc.isValid() && !SM.isInSystemHeader(Loc) &&
         !SM.isWrittenInBuiltinFile(Loc);
}

inline bool spellsToken(SourceLocation Loc, StringRef Expected,
                        const SourceManager &SM, const LangOptions &Lang) {
  Token Tok;
  return !Lexer::getRawToken(SM.getSpellingLoc(Loc), Tok, SM, Lang, true) &&
         Lexer::getSpelling(Tok, SM, Lang) == Expected;
}

inline bool stdTemplate(const CXXRecordDecl *RD, StringRef Name) {
  if (const auto *Spec = dyn_cast_or_null<ClassTemplateSpecializationDecl>(RD))
    return qualifiedName(Spec->getSpecializedTemplate()) ==
           ("std::" + Name).str();
  return false;
}

inline bool stdTemplateType(QualType Type, StringRef Name) {
  if (stdTemplate(Type->getAsCXXRecordDecl(), Name))
    return true;
  if (const auto *Specialization = Type->getAs<TemplateSpecializationType>())
    if (const TemplateDecl *Template =
            Specialization->getTemplateName().getAsTemplateDecl())
      if (qualifiedName(Template) == ("std::" + Name).str())
        return true;
  QualType Canonical = Type.getCanonicalType();
  if (Canonical != Type)
    return stdTemplateType(Canonical, Name);
  return false;
}

inline bool allowedStdSpecialization(const CXXRecordDecl *RD,
                                     const SourceManager &SM) {
  const auto *Spec = dyn_cast_or_null<ClassTemplateSpecializationDecl>(RD);
  if (!Spec || (!stdTemplate(RD, "formatter") && !stdTemplate(RD, "hash")) ||
      Spec->getTemplateArgs().size() == 0)
    return false;
  const TemplateArgument &Arg = Spec->getTemplateArgs()[0];
  if (Arg.getKind() != TemplateArgument::Type)
    return false;
  QualType Type = Arg.getAsType().getCanonicalType();
  const TagDecl *Tag = Type->getAsTagDecl();
  if (!Tag)
    if (const auto *Instance = Type->getAs<TemplateSpecializationType>())
      if (const auto *Template = dyn_cast_or_null<ClassTemplateDecl>(
              Instance->getTemplateName().getAsTemplateDecl()))
        Tag = Template->getTemplatedDecl();
  return Tag && userLocation(Tag->getLocation(), SM);
}

class PolicyCheck : public ClangTidyCheck {
public:
  using ClangTidyCheck::ClangTidyCheck;
  bool isLanguageVersionSupported(const LangOptions &Lang) const override {
    return Lang.CPlusPlus;
  }

protected:
  bool firstAt(SourceLocation Loc, const SourceManager &SM) {
    return userLocation(Loc, SM) &&
           Seen.insert(SM.getExpansionLoc(Loc).getRawEncoding()).second;
  }

private:
  llvm::DenseSet<unsigned> Seen;
};

void registerLanguageChecks(ClangTidyCheckFactories &Factories);
void registerLibraryChecks(ClangTidyCheckFactories &Factories);
void registerGenericChecks(ClangTidyCheckFactories &Factories);
void registerPreprocessorChecks(ClangTidyCheckFactories &Factories);
} // namespace clang::tidy::narrow
