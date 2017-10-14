// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Handles the rewriting of base::Value::GetType() to base::Value::type().

#ifndef TOOLS_CLANG_VALUE_CLEANUP_VALUE_REWRITER_H_
#define TOOLS_CLANG_VALUE_CLEANUP_VALUE_REWRITER_H_

#include <set>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Refactoring.h"

class ValueRewriter {
 public:
  explicit ValueRewriter(std::set<clang::tooling::Replacement>* replacements);

  void RegisterMatchers(clang::ast_matchers::MatchFinder* match_finder);

 private:
  class GetTypeCallback
      : public clang::ast_matchers::MatchFinder::MatchCallback {
   public:
    explicit GetTypeCallback(
        std::set<clang::tooling::Replacement>* replacements);

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& result) override;

   private:
    std::set<clang::tooling::Replacement>* const replacements_;
  };

  GetTypeCallback get_type_callback_;
};

#endif  // TOOLS_CLANG_VALUE_CLEANUP_VALUE_REWRITER_H_
