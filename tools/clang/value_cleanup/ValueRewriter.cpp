// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ValueRewriter.h"

using namespace clang::ast_matchers;

ValueRewriter::GetTypeCallback::GetTypeCallback(
    std::set<clang::tooling::Replacement>* replacements)
    : replacements_(replacements) {}

// Replaces calls to |base::Value::GetType| with calls to |base::Value::type|.
void ValueRewriter::GetTypeCallback::run(
    const MatchFinder::MatchResult& result) {
  // Replace 'GetType' with 'type'.
  auto* callExpr = result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("callExpr");

  clang::CharSourceRange call_range =
      clang::CharSourceRange::getTokenRange(callExpr->getExprLoc());
  replacements_->emplace(*result.SourceManager, call_range, "type");
}

ValueRewriter::ValueRewriter(
    std::set<clang::tooling::Replacement>* replacements)
    : get_type_callback_(replacements) {}

void ValueRewriter::RegisterMatchers(MatchFinder* match_finder) {
  match_finder->addMatcher(
      id("callExpr", cxxMemberCallExpr(callee(cxxMethodDecl(
                                           hasName("::base::Value::GetType"))),
                                       argumentCountIs(0))),
      &get_type_callback_);
}
