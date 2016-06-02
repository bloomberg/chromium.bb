// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ListValueRewriter.h"

#include <assert.h>
#include <algorithm>

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/Refactoring.h"
#include "llvm/ADT/STLExtras.h"

using namespace clang::ast_matchers;
using clang::tooling::Replacement;
using clang::tooling::Replacements;
using llvm::StringRef;

ListValueRewriter::AppendCallback::AppendCallback(Replacements* replacements)
    : replacements_(replacements) {}

void ListValueRewriter::AppendCallback::run(
    const MatchFinder::MatchResult& result) {
  // Delete `new base::*Value(' and `)'.
  auto* newExpr = result.Nodes.getNodeAs<clang::CXXNewExpr>("newExpr");
  auto* argExpr = result.Nodes.getNodeAs<clang::Expr>("argExpr");

  clang::CharSourceRange pre_arg_range = clang::CharSourceRange::getCharRange(
      newExpr->getLocStart(), argExpr->getLocStart());
  replacements_->emplace(*result.SourceManager, pre_arg_range, "");

  clang::CharSourceRange post_arg_range =
      clang::CharSourceRange::getTokenRange(newExpr->getLocEnd());
  replacements_->emplace(*result.SourceManager, post_arg_range, "");
}

ListValueRewriter::AppendBooleanCallback::AppendBooleanCallback(
    Replacements* replacements)
    : AppendCallback(replacements) {}

void ListValueRewriter::AppendBooleanCallback::run(
    const MatchFinder::MatchResult& result) {
  // Replace 'Append' with 'AppendBoolean'.
  auto* callExpr = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("callExpr");

  clang::CharSourceRange call_range =
      clang::CharSourceRange::getTokenRange(callExpr->getExprLoc());
  replacements_->emplace(*result.SourceManager, call_range, "AppendBoolean");

  AppendCallback::run(result);
}

ListValueRewriter::AppendIntegerCallback::AppendIntegerCallback(
    Replacements* replacements)
    : AppendCallback(replacements) {}

void ListValueRewriter::AppendIntegerCallback::run(
    const MatchFinder::MatchResult& result) {
  // Replace 'Append' with 'AppendInteger'.
  auto* callExpr = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("callExpr");

  clang::CharSourceRange call_range =
      clang::CharSourceRange::getTokenRange(callExpr->getExprLoc());
  replacements_->emplace(*result.SourceManager, call_range, "AppendInteger");

  AppendCallback::run(result);
}

ListValueRewriter::AppendDoubleCallback::AppendDoubleCallback(
    Replacements* replacements)
    : AppendCallback(replacements) {}

void ListValueRewriter::AppendDoubleCallback::run(
    const MatchFinder::MatchResult& result) {
  // Replace 'Append' with 'AppendDouble'.
  auto* callExpr = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("callExpr");

  clang::CharSourceRange call_range =
      clang::CharSourceRange::getTokenRange(callExpr->getExprLoc());
  replacements_->emplace(*result.SourceManager, call_range, "AppendDouble");

  AppendCallback::run(result);
}

ListValueRewriter::AppendStringCallback::AppendStringCallback(
    Replacements* replacements)
    : AppendCallback(replacements) {}

void ListValueRewriter::AppendStringCallback::run(
    const MatchFinder::MatchResult& result) {
  // Replace 'Append' with 'AppendString'.
  auto* callExpr = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("callExpr");

  clang::CharSourceRange call_range =
      clang::CharSourceRange::getTokenRange(callExpr->getExprLoc());
  replacements_->emplace(*result.SourceManager, call_range, "AppendString");

  AppendCallback::run(result);
}

ListValueRewriter::ListValueRewriter(Replacements* replacements)
    : append_boolean_callback_(replacements),
      append_integer_callback_(replacements),
      append_double_callback_(replacements),
      append_string_callback_(replacements) {}

void ListValueRewriter::RegisterMatchers(MatchFinder* match_finder) {
  auto is_list_append =
      allOf(callee(cxxMethodDecl(hasName("::base::ListValue::Append"))),
            argumentCountIs(1));

  // base::ListValue::Append(new base::FundamentalValue(bool))
  //     => base::ListValue::AppendBoolean()
  match_finder->addMatcher(
      id("callExpr",
         cxxMemberCallExpr(
             is_list_append,
             hasArgument(
                 0, ignoringParenImpCasts(id(
                        "newExpr",
                        cxxNewExpr(has(cxxConstructExpr(
                            hasDeclaration(cxxMethodDecl(hasName(
                                "::base::FundamentalValue::FundamentalValue"))),
                            argumentCountIs(1),
                            hasArgument(
                                0, id("argExpr",
                                      expr(hasType(booleanType())))))))))))),
      &append_boolean_callback_);

  // base::ListValue::Append(new base::FundamentalValue(int))
  //     => base::ListValue::AppendInteger()
  match_finder->addMatcher(
      id("callExpr",
         cxxMemberCallExpr(
             is_list_append,
             hasArgument(
                 0,
                 ignoringParenImpCasts(id(
                     "newExpr",
                     cxxNewExpr(has(cxxConstructExpr(
                         hasDeclaration(cxxMethodDecl(hasName(
                             "::base::FundamentalValue::FundamentalValue"))),
                         argumentCountIs(1),
                         hasArgument(0, id("argExpr",
                                           expr(hasType(isInteger()),
                                                unless(hasType(
                                                    booleanType()))))))))))))),
      &append_integer_callback_);

  // base::ListValue::Append(new base::FundamentalValue(double))
  //     => base::ListValue::AppendDouble()
  match_finder->addMatcher(
      id("callExpr",
         cxxMemberCallExpr(
             is_list_append,
             hasArgument(
                 0, ignoringParenImpCasts(id(
                        "newExpr",
                        cxxNewExpr(has(cxxConstructExpr(
                            hasDeclaration(cxxMethodDecl(hasName(
                                "::base::FundamentalValue::FundamentalValue"))),
                            argumentCountIs(1),
                            hasArgument(
                                0, id("argExpr",
                                      expr(hasType(
                                          realFloatingPointType())))))))))))),
      &append_double_callback_);

  // base::ListValue::Append(new base::StringValue(...))
  //     => base::ListValue::AppendString()
  match_finder->addMatcher(
      id("callExpr",
         cxxMemberCallExpr(
             is_list_append,
             hasArgument(
                 0, ignoringParenImpCasts(id(
                        "newExpr",
                        cxxNewExpr(has(cxxConstructExpr(
                            hasDeclaration(cxxMethodDecl(
                                hasName("::base::StringValue::StringValue"))),
                            argumentCountIs(1),
                            hasArgument(0, id("argExpr", expr())))))))))),
      &append_string_callback_);
}
