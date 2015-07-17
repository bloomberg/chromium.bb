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
  substitutions[""] = "blank";

  EXPECT_EQ(ReplaceTemplateExpressions("${}", substitutions), "blank");
  EXPECT_EQ(ReplaceTemplateExpressions("", substitutions), "");
  EXPECT_EQ(ReplaceTemplateExpressions("${test}", substitutions), "word");
  EXPECT_EQ(ReplaceTemplateExpressions("${5} ", substitutions), "number ");
  EXPECT_EQ(
      ReplaceTemplateExpressions("multiple: ${test}, ${5}.", substitutions),
      "multiple: word, number.");
}

TEST(TemplateExpressionsTest,
     ReplaceTemplateExpressionsConsecutiveDollarSignsPieces) {
  std::map<base::StringPiece, std::string> substitutions;
  substitutions["a"] = "x";
  EXPECT_EQ(ReplaceTemplateExpressions("$ $$ $$$", substitutions), "$ $$ $$$");
  EXPECT_EQ(ReplaceTemplateExpressions("$${a}", substitutions), "$x");
  EXPECT_EQ(ReplaceTemplateExpressions("$$${a}", substitutions), "$$x");
  EXPECT_EQ(ReplaceTemplateExpressions("$12", substitutions), "$12");
}

}  // namespace ui
