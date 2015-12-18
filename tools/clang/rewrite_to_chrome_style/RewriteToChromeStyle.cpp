// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Changes Blink-style names to Chrome-style names. Currently transforms:
//   fields:
//     int m_operationCount => int operation_count_
//   variables:
//     int mySuperVariable => int my_super_variable
//   constants:
//     const int maxThings => const int kMaxThings
//   free functions:
//     void doThisThenThat() => void DoThisAndThat()

#include <algorithm>
#include <memory>
#include <string>

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::ast_matchers;
using clang::tooling::CommonOptionsParser;
using clang::tooling::Replacement;
using clang::tooling::Replacements;
using llvm::StringRef;

namespace {

constexpr char kBlinkFieldPrefix[] = "m_";
constexpr char kBlinkStaticMemberPrefix[] = "s_";

bool GetNameForDecl(const clang::FunctionDecl& decl,
                    clang::ASTContext* context,
                    std::string& name) {
  name = decl.getNameAsString();
  name[0] = clang::toUppercase(name[0]);
  return true;
}

// Helper to convert from a camelCaseName to camel_case_name. It uses some
// heuristics to try to handle acronyms in camel case names correctly.
std::string CamelCaseToUnderscoreCase(StringRef input) {
  std::string output;
  bool needs_underscore = false;
  bool was_lowercase = false;
  bool was_uppercase = false;
  // Iterate in reverse to minimize the amount of backtracking.
  for (const unsigned char* i = input.bytes_end() - 1; i >= input.bytes_begin();
       --i) {
    char c = *i;
    bool is_lowercase = clang::isLowercase(c);
    bool is_uppercase = clang::isUppercase(c);
    c = clang::toLowercase(c);
    // Transitioning from upper to lower case requires an underscore. This is
    // needed to handle names with acronyms, e.g. handledHTTPRequest needs a '_'
    // in 'dH'. This is a complement to the non-acronym case further down.
    if (needs_underscore || (was_uppercase && is_lowercase)) {
      output += '_';
      needs_underscore = false;
    }
    output += c;
    // Handles the non-acronym case: transitioning from lower to upper case
    // requires an underscore when emitting the next character, e.g. didLoad
    // needs a '_' in 'dL'.
    if (i != input.bytes_end() - 1 && was_lowercase && is_uppercase)
      needs_underscore = true;
    was_lowercase = is_lowercase;
    was_uppercase = is_uppercase;
  }
  std::reverse(output.begin(), output.end());
  return output;
}

bool GetNameForDecl(const clang::FieldDecl& decl,
                    clang::ASTContext* context,
                    std::string& name) {
  StringRef original_name = decl.getName();
  // Blink style field names are prefixed with `m_`. If this prefix isn't
  // present, assume it's already been converted to Google style.
  if (original_name.size() < strlen(kBlinkFieldPrefix) ||
      !original_name.startswith(kBlinkFieldPrefix))
    return false;
  name = CamelCaseToUnderscoreCase(
      original_name.substr(strlen(kBlinkFieldPrefix)));
  // The few examples I could find used struct-style naming with no `_` suffix
  // for unions.
  if (decl.getParent()->isClass())
    name += '_';
  return true;
}

bool IsProbablyConst(const clang::VarDecl& decl) {
  clang::QualType type = decl.getType();
  if (!type.isConstQualified())
    return false;

  if (type.isVolatileQualified())
    return false;

  // http://google.github.io/styleguide/cppguide.html#Constant_Names
  // Static variables that are const-qualified should use kConstantStyle naming.
  if (decl.getStorageDuration() == clang::SD_Static)
    return true;

  // Otherwise, use a simple heuristic: if it's initialized with a literal of
  // some sort, also use kConstantStyle naming.
  const clang::Expr* initializer = decl.getInit();
  if (!initializer)
    return false;

  // Ignore implicit casts, so the literal check below still matches on
  // array-to-pointer decay, e.g.
  //   const char* const kConst = "...";
  if (const clang::ImplicitCastExpr* cast_expr =
          clang::dyn_cast<clang::ImplicitCastExpr>(initializer))
    initializer = cast_expr->getSubExprAsWritten();

  return clang::isa<clang::CharacterLiteral>(initializer) ||
         clang::isa<clang::CompoundLiteralExpr>(initializer) ||
         clang::isa<clang::CXXBoolLiteralExpr>(initializer) ||
         clang::isa<clang::CXXNullPtrLiteralExpr>(initializer) ||
         clang::isa<clang::FloatingLiteral>(initializer) ||
         clang::isa<clang::IntegerLiteral>(initializer) ||
         clang::isa<clang::StringLiteral>(initializer) ||
         clang::isa<clang::UserDefinedLiteral>(initializer);
}

bool GetNameForDecl(const clang::VarDecl& decl,
                    clang::ASTContext* context,
                    std::string& name) {
  StringRef original_name = decl.getName();

  // Nothing to do for unnamed parameters.
  if (clang::isa<clang::ParmVarDecl>(decl) && original_name.empty())
    return false;

  // static class members match against VarDecls. Blink style dictates that
  // these should be prefixed with `s_`, so strip that off. Also check for `m_`
  // and strip that off too, for code that accidentally uses the wrong prefix.
  if (original_name.startswith(kBlinkStaticMemberPrefix))
    original_name = original_name.substr(strlen(kBlinkStaticMemberPrefix));
  else if (original_name.startswith(kBlinkFieldPrefix))
    original_name = original_name.substr(strlen(kBlinkFieldPrefix));

  if (IsProbablyConst(decl)) {
    // Don't try to rename constants that already conform to Chrome style.
    if (original_name.size() >= 2 && original_name[0] == 'k' &&
        clang::isUppercase(original_name[1]))
      return false;
    name = 'k';
    name.append(original_name.data(), original_name.size());
    name[1] = clang::toUppercase(name[1]);
  } else {
    name = CamelCaseToUnderscoreCase(original_name);
  }

  if (decl.isStaticDataMember()) {
    name += '_';
  }

  return true;
}

template <typename DeclNodeType, typename TargetTraits>
class RewriterCallbackBase : public MatchFinder::MatchCallback {
 public:
  explicit RewriterCallbackBase(Replacements* replacements)
      : replacements_(replacements) {}
  virtual void run(const MatchFinder::MatchResult& result) override {
    std::string name;
    if (!GetNameForDecl(*result.Nodes.getNodeAs<DeclNodeType>("decl"),
                        result.Context, name))
      return;
    replacements_->emplace(
        *result.SourceManager,
        TargetTraits::GetRange(
            *result.Nodes.getNodeAs<typename TargetTraits::NodeType>(
                TargetTraits::kName)),
        name);
  }

 private:
  Replacements* const replacements_;
};

struct DeclTargetTraits {
  using NodeType = clang::NamedDecl;
  static constexpr char kName[] = "decl";
  static clang::CharSourceRange GetRange(const NodeType& decl) {
    return clang::CharSourceRange::getTokenRange(decl.getLocation());
  }
};
constexpr char DeclTargetTraits::kName[];

struct ExprTargetTraits {
  using NodeType = clang::Expr;
  static constexpr char kName[] = "expr";
  static clang::CharSourceRange GetRange(const NodeType& expr) {
    return clang::CharSourceRange::getTokenRange(expr.getExprLoc());
  }
};
constexpr char ExprTargetTraits::kName[];

struct InitializerTargetTraits {
  using NodeType = clang::CXXCtorInitializer;
  static constexpr char kName[] = "initializer";
  static clang::CharSourceRange GetRange(const NodeType& init) {
    return clang::CharSourceRange::getTokenRange(init.getSourceLocation());
  }
};
constexpr char InitializerTargetTraits::kName[];

using FunctionDeclRewriterCallback =
    RewriterCallbackBase<clang::FunctionDecl, DeclTargetTraits>;
using FieldDeclRewriterCallback =
    RewriterCallbackBase<clang::FieldDecl, DeclTargetTraits>;
using VarDeclRewriterCallback =
    RewriterCallbackBase<clang::VarDecl, DeclTargetTraits>;

using CallRewriterCallback =
    RewriterCallbackBase<clang::FunctionDecl, ExprTargetTraits>;
using MemberRewriterCallback =
    RewriterCallbackBase<clang::FieldDecl, ExprTargetTraits>;
using DeclRefRewriterCallback =
    RewriterCallbackBase<clang::VarDecl, ExprTargetTraits>;

using ConstructorInitializerRewriterCallback =
    RewriterCallbackBase<clang::FieldDecl, InitializerTargetTraits>;

}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  // TODO(dcheng): Clang tooling should do this itself.
  // http://llvm.org/bugs/show_bug.cgi?id=21627
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "rewrite_to_chrome_style: convert Blink style to Chrome style.");
  CommonOptionsParser options(argc, argv, category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  Replacements replacements;

  auto in_blink_namespace =
      decl(hasAncestor(namespaceDecl(anyOf(hasName("blink"), hasName("WTF")))));

  // Declaration handling (e.g. function definitions and variable definitions):

  // Note: for now, only rewrite standalone functions until the question of JS
  // binding integration for class methods is resolved.
  // TODO(dcheng): Since classes in public/ aren't directly web-exposed, just go
  // ahead and rewrite those.
  auto function_decl_matcher =
      id("decl", functionDecl(unless(cxxMethodDecl()), in_blink_namespace));

  auto field_decl_matcher = id("decl", fieldDecl(in_blink_namespace));
  auto var_decl_matcher = id("decl", varDecl(in_blink_namespace));

  FunctionDeclRewriterCallback function_decl_rewriter(&replacements);
  match_finder.addMatcher(function_decl_matcher, &function_decl_rewriter);
  FieldDeclRewriterCallback field_decl_rewriter(&replacements);
  match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);
  VarDeclRewriterCallback var_decl_rewriter(&replacements);
  match_finder.addMatcher(var_decl_matcher, &var_decl_rewriter);

  // Expression handling (e.g. calling a Blink function or referencing a
  // variable defined in Blink):
  auto call_matcher = id("expr", callExpr(callee(function_decl_matcher)));
  auto member_matcher = id("expr", memberExpr(member(field_decl_matcher)));
  auto decl_ref_matcher = id("expr", declRefExpr(to(var_decl_matcher)));

  CallRewriterCallback call_rewriter(&replacements);
  match_finder.addMatcher(call_matcher, &call_rewriter);
  MemberRewriterCallback member_rewriter(&replacements);
  match_finder.addMatcher(member_matcher, &member_rewriter);
  DeclRefRewriterCallback decl_ref_rewriter(&replacements);
  match_finder.addMatcher(decl_ref_matcher, &decl_ref_rewriter);

  // Function reference handling (e.g. getting a pointer to a function without
  // calling it):
  auto function_ref_matcher =
      id("expr", declRefExpr(to(function_decl_matcher)));
  match_finder.addMatcher(function_ref_matcher, &call_rewriter);

  // Initializer handling:
  auto constructor_initializer_matcher =
      cxxConstructorDecl(forEachConstructorInitializer(
          id("initializer", cxxCtorInitializer(forField(field_decl_matcher)))));

  ConstructorInitializerRewriterCallback constructor_initializer_rewriter(
      &replacements);
  match_finder.addMatcher(constructor_initializer_matcher,
                          &constructor_initializer_rewriter);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

  // Serialization format is documented in tools/clang/scripts/run_tool.py
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (const auto& r : replacements) {
    std::string replacement_text = r.getReplacementText().str();
    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    llvm::outs() << "r:::" << r.getFilePath() << ":::" << r.getOffset()
                 << ":::" << r.getLength() << ":::" << replacement_text << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
