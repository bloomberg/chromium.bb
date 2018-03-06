// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <algorithm>
#include <memory>
#include <string>

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
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
using Replacements = std::vector<clang::tooling::Replacement>;

namespace {

class Rewriter {
 public:
  virtual ~Rewriter() {}
};

// Removes unneeded base::Passed() on a parameter of base::BindOnce().
// Example:
//   // Before
//   base::BindOnce(&Foo, base::Passed(&bar));
//   base::BindOnce(&Foo, base::Passed(std::move(baz)));
//   base::BindOnce(&Foo, base::Passed(qux));
//
//   // After
//   base::BindOnce(&Foo, std::move(bar));
//   base::BindOnce(&Foo, std::move(baz));
//   base::BindOnce(&Foo, std::move(*qux));
class PassedToMoveRewriter : public MatchFinder::MatchCallback,
                             public Rewriter {
 public:
  explicit PassedToMoveRewriter(Replacements* replacements)
      : replacements_(replacements) {}
  ~PassedToMoveRewriter() override {}

  StatementMatcher GetMatcher() {
    auto is_passed = namedDecl(hasName("::base::Passed"));
    auto is_bind_once_call = callee(namedDecl(hasName("::base::BindOnce")));

    // Matches base::Passed() call on a base::BindOnce() argument.
    return callExpr(is_bind_once_call,
                    hasAnyArgument(ignoringImplicit(
                        callExpr(callee(is_passed)).bind("target"))));
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* target = result.Nodes.getNodeAs<clang::CallExpr>("target");
    auto* callee = target->getCallee()->IgnoreImpCasts();

    auto* callee_decl = clang::dyn_cast<clang::DeclRefExpr>(callee)->getDecl();
    auto* passed_decl = clang::dyn_cast<clang::FunctionDecl>(callee_decl);
    auto* param_type = passed_decl->getParamDecl(0)->getType().getTypePtr();

    if (param_type->isRValueReferenceType()) {
      // base::Passed(xxx) -> xxx.
      // The parameter type is already an rvalue reference.
      // Example:
      //   std::unique_ptr<int> foo();
      //   std::unique_ptr<int> bar;
      //   base::Passed(foo());
      //   base::Passed(std::move(bar));
      // In these cases, we can just remove base::Passed.
      auto left = clang::CharSourceRange::getTokenRange(
          result.SourceManager->getSpellingLoc(target->getLocStart()),
          result.SourceManager->getSpellingLoc(target->getArg(0)->getExprLoc())
              .getLocWithOffset(-1));
      auto r_paren = clang::CharSourceRange::getTokenRange(
          result.SourceManager->getSpellingLoc(target->getRParenLoc()),
          result.SourceManager->getSpellingLoc(target->getRParenLoc()));
      replacements_->emplace_back(*result.SourceManager, left, " ");
      replacements_->emplace_back(*result.SourceManager, r_paren, " ");
      return;
    }

    if (!param_type->isPointerType())
      return;

    auto* passed_arg = target->getArg(0)->IgnoreImpCasts();
    if (auto* unary = clang::dyn_cast<clang::UnaryOperator>(passed_arg)) {
      if (unary->getOpcode() == clang::UO_AddrOf) {
        // base::Passed(&xxx) -> std::move(xxx).
        auto left = clang::CharSourceRange::getTokenRange(
            result.SourceManager->getSpellingLoc(target->getLocStart()),
            result.SourceManager->getSpellingLoc(
                target->getArg(0)->getExprLoc()));
        replacements_->emplace_back(*result.SourceManager, left, "std::move(");
        return;
      }
    }

    // base::Passed(xxx) -> std::move(*xxx)
    auto left = clang::CharSourceRange::getTokenRange(
        result.SourceManager->getSpellingLoc(target->getLocStart()),
        result.SourceManager->getSpellingLoc(target->getArg(0)->getExprLoc())
            .getLocWithOffset(-1));
    replacements_->emplace_back(*result.SourceManager, left, "std::move(*");
  }

 private:
  Replacements* replacements_;
};

// Replace base::Bind() to base::BindOnce() where resulting base::Callback is
// implicitly converted into base::OnceCallback.
// Example:
//   // Before
//   base::PostTask(FROM_HERE, base::Bind(&Foo));
//   base::OnceCallback<void()> cb = base::Bind(&Foo);
//
//   // After
//   base::PostTask(FROM_HERE, base::BindOnce(&Foo));
//   base::OnceCallback<void()> cb = base::BindOnce(&Foo);
class BindOnceRewriter : public MatchFinder::MatchCallback, public Rewriter {
 public:
  explicit BindOnceRewriter(Replacements* replacements)
      : replacements_(replacements) {}
  ~BindOnceRewriter() override {}

  StatementMatcher GetMatcher() {
    auto is_once_callback = hasType(hasCanonicalType(hasDeclaration(
        classTemplateSpecializationDecl(hasName("::base::OnceCallback")))));
    auto is_repeating_callback =
        hasType(hasCanonicalType(hasDeclaration(classTemplateSpecializationDecl(
            hasName("::base::RepeatingCallback")))));

    auto bind_call =
        callExpr(callee(namedDecl(hasName("::base::Bind")))).bind("target");
    auto parameter_construction =
        cxxConstructExpr(is_repeating_callback, argumentCountIs(1),
                         hasArgument(0, ignoringImplicit(bind_call)));
    auto constructor_conversion = cxxConstructExpr(
        is_once_callback, argumentCountIs(1),
        hasArgument(0, ignoringImplicit(parameter_construction)));
    auto implicit_conversion = implicitCastExpr(
        is_once_callback, hasSourceExpression(constructor_conversion));
    return implicit_conversion;
  }

  void run(const MatchFinder::MatchResult& result) override {
    auto* target = result.Nodes.getNodeAs<clang::CallExpr>("target");
    auto* callee = target->getCallee();
    auto range = clang::CharSourceRange::getTokenRange(
        result.SourceManager->getSpellingLoc(callee->getLocEnd()),
        result.SourceManager->getSpellingLoc(callee->getLocEnd()));
    replacements_->emplace_back(*result.SourceManager, range, "BindOnce");
  }

 private:
  Replacements* replacements_;
};

llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);
llvm::cl::OptionCategory rewriter_category("Rewriter Options");

llvm::cl::opt<std::string> rewriter_option(
    "rewriter",
    llvm::cl::desc(R"(One of the name of rewriter to apply.
Available rewriters are:
    remove_unneeded_passed
    bind_to_bind_once
The default is remove_unneeded_passed.
)"),
    llvm::cl::init("remove_unneeded_passed"),
    llvm::cl::cat(rewriter_category));

}  // namespace.

int main(int argc, const char* argv[]) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  CommonOptionsParser options(argc, argv, rewriter_category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  std::vector<clang::tooling::Replacement> replacements;

  std::unique_ptr<Rewriter> rewriter;
  if (rewriter_option == "remove_unneeded_passed") {
    auto passed_to_move =
        llvm::make_unique<PassedToMoveRewriter>(&replacements);
    match_finder.addMatcher(passed_to_move->GetMatcher(), passed_to_move.get());
    rewriter = std::move(passed_to_move);
  } else if (rewriter_option == "bind_to_bind_once") {
    auto bind_once = llvm::make_unique<BindOnceRewriter>(&replacements);
    match_finder.addMatcher(bind_once->GetMatcher(), bind_once.get());
    rewriter = std::move(bind_once);
  }

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
