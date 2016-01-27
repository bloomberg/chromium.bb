// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/template_expressions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(TemplateExpressionsTest, ReplaceTemplateExpressionsPieces) {
  std::map<base::StringPiece, std::string> substitutions;
  substitutions["test"] = "word";
  substitutions["5"] = "number";

  EXPECT_EQ("${}", ReplaceTemplateExpressions("${}", substitutions));
  EXPECT_EQ("", ReplaceTemplateExpressions("", substitutions));
  EXPECT_EQ("word", ReplaceTemplateExpressions("${test}", substitutions));
  EXPECT_EQ("number ", ReplaceTemplateExpressions("${5} ", substitutions));
  EXPECT_EQ(
      "multiple: word, number.",
      ReplaceTemplateExpressions("multiple: ${test}, ${5}.", substitutions));
}

TEST(TemplateExpressionsTest,
     ReplaceTemplateExpressionsConsecutiveDollarSignsPieces) {
  std::map<base::StringPiece, std::string> substitutions;
  substitutions["a"] = "x";
  EXPECT_EQ("$ $$ $$$", ReplaceTemplateExpressions("$ $$ $$$", substitutions));
  EXPECT_EQ("$x", ReplaceTemplateExpressions("$${a}", substitutions));
  EXPECT_EQ("$$x", ReplaceTemplateExpressions("$$${a}", substitutions));
  EXPECT_EQ("$12", ReplaceTemplateExpressions("$12", substitutions));
}

}  // namespace ui
