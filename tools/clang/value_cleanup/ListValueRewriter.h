// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Performs simple cleanups of base::ListValue API:
// - base::ListValue::Append(new base::FundamentalValue(bool))
//       => base::ListValue::AppendBoolean(...)
// - base::ListValue::Append(new base::FundamentalValue(int))
//       => base::ListValue::AppendInteger(...)
// - base::ListValue::Append(new base::FundamentalValue(double))
//       => base::ListValue::AppendDouble(...)
// - base::ListValue::Append(new base::StringValue(...))
//       => base::ListValue::AppendString(...)

#ifndef TOOLS_CLANG_VALUE_CLEANUP_LIST_VALUE_REWRITER_H_
#define TOOLS_CLANG_VALUE_CLEANUP_LIST_VALUE_REWRITER_H_

#include <memory>

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Refactoring.h"

class ListValueRewriter {
 public:
  explicit ListValueRewriter(clang::tooling::Replacements* replacements);

  void RegisterMatchers(clang::ast_matchers::MatchFinder* match_finder);

 private:
  class AppendCallback
      : public clang::ast_matchers::MatchFinder::MatchCallback {
   public:
    explicit AppendCallback(clang::tooling::Replacements* replacements);

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& result) override;

   protected:
    clang::tooling::Replacements* const replacements_;
  };

  class AppendBooleanCallback : public AppendCallback {
   public:
    explicit AppendBooleanCallback(clang::tooling::Replacements* replacements);

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& result) override;
  };

  class AppendIntegerCallback : public AppendCallback {
   public:
    explicit AppendIntegerCallback(clang::tooling::Replacements* replacements);

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& result) override;
  };

  class AppendDoubleCallback : public AppendCallback {
   public:
    explicit AppendDoubleCallback(clang::tooling::Replacements* replacements);

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& result) override;
  };

  class AppendStringCallback : public AppendCallback {
   public:
    explicit AppendStringCallback(clang::tooling::Replacements* replacements);

    void run(
        const clang::ast_matchers::MatchFinder::MatchResult& result) override;
  };

  AppendBooleanCallback append_boolean_callback_;
  AppendIntegerCallback append_integer_callback_;
  AppendDoubleCallback append_double_callback_;
  AppendStringCallback append_string_callback_;
};

#endif  // TOOLS_CLANG_VALUE_CLEANUP_LIST_VALUE_REWRITER_H_
