// Copyright (C) 2011 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Author: George Yakovlev
#include <gtest/gtest.h>

#include "base/memory/scoped_ptr.h"
#include "regexp_adapter.h"

namespace reg_exp {

TEST(RegExpAdapter, TestConsumeRegExp) {
  scoped_ptr<const reg_exp::RegularExpression> reg_exp1(
      reg_exp::CreateRegularExpression("[0-9a-z]+"));
  scoped_ptr<const reg_exp::RegularExpression> reg_exp2(
      reg_exp::CreateRegularExpression(" \\(([0-9a-z]+)\\)"));
  scoped_ptr<const reg_exp::RegularExpression> reg_exp3(
      reg_exp::CreateRegularExpression("([0-9a-z]+)-([0-9a-z]+)"));

  scoped_ptr<reg_exp::RegularExpressionInput> reg_input1(
      reg_exp::CreateRegularExpressionInput("+1-123-456-789"));
  scoped_ptr<reg_exp::RegularExpressionInput> reg_input2(
      reg_exp::CreateRegularExpressionInput("1 (123)456-789"));

  EXPECT_FALSE(reg_exp1->Consume(reg_input1.get(), true, NULL, NULL));
  EXPECT_EQ(reg_input1->ToString(), "+1-123-456-789");
  EXPECT_TRUE(reg_exp1->Consume(reg_input1.get(), false, NULL, NULL));
  EXPECT_EQ(reg_input1->ToString(), "-123-456-789");
  std::string res1, res2;
  EXPECT_FALSE(reg_exp2->Consume(reg_input1.get(), true, &res1, NULL));
  EXPECT_FALSE(reg_exp3->Consume(reg_input1.get(), true, &res1, &res2));
  EXPECT_TRUE(reg_exp3->Consume(reg_input1.get(), false, &res1, &res2));
  EXPECT_EQ(reg_input1->ToString(), "-789");
  EXPECT_EQ(res1, "123");
  EXPECT_EQ(res2, "456");

  EXPECT_EQ(reg_input2->ToString(), "1 (123)456-789");
  EXPECT_TRUE(reg_exp1->Consume(reg_input2.get(), true, NULL, NULL));
  EXPECT_EQ(reg_input2->ToString(), " (123)456-789");
  EXPECT_TRUE(reg_exp2->Consume(reg_input2.get(), true, &res1, NULL));
  EXPECT_EQ(reg_input2->ToString(), "456-789");
  EXPECT_EQ(res1, "123");
  EXPECT_TRUE(reg_exp3->Consume(reg_input2.get(), true, &res1, &res2));
  EXPECT_EQ(reg_input2->ToString(), "");
  EXPECT_EQ(res1, "456");
  EXPECT_EQ(res2, "789");
}

TEST(RegExpAdapter, TestConsumeInput) {
  scoped_ptr<reg_exp::RegularExpressionInput> reg_input(
      reg_exp::CreateRegularExpressionInput("1 (123)456-789"));
  std::string res1, res2;
  EXPECT_EQ(reg_input->ToString(), "1 (123)456-789");
  EXPECT_FALSE(reg_input->ConsumeRegExp(std::string("\\[1\\]"),
                                        true,
                                        &res1,
                                        &res2));
  EXPECT_EQ(reg_input->ToString(), "1 (123)456-789");
  EXPECT_FALSE(reg_input->ConsumeRegExp(std::string("([0-9]+) \\([0-9]+\\)"),
                                        true,
                                        &res1,
                                        &res2));
  EXPECT_EQ(reg_input->ToString(), "1 (123)456-789");
  EXPECT_TRUE(reg_input->ConsumeRegExp(std::string("([0-9]+) \\(([0-9]+)\\)"),
                                        true,
                                        &res1,
                                        &res2));
  EXPECT_EQ(reg_input->ToString(), "456-789");
  EXPECT_EQ(res1, "1");
  EXPECT_EQ(res2, "123");
}

TEST(RegExpAdapter, TestMatch) {
  scoped_ptr<const reg_exp::RegularExpression> reg_exp(
      reg_exp::CreateRegularExpression("([0-9a-z]+)"));
  std::string matched;
  EXPECT_TRUE(reg_exp->Match("12345af", true, &matched));
  EXPECT_EQ(matched, "12345af");
  EXPECT_TRUE(reg_exp->Match("12345af", false, &matched));
  EXPECT_EQ(matched, "12345af");
  EXPECT_TRUE(reg_exp->Match("12345af", false, NULL));
  EXPECT_TRUE(reg_exp->Match("12345af", true, NULL));

  EXPECT_FALSE(reg_exp->Match("[12]", true, &matched));
  EXPECT_TRUE(reg_exp->Match("[12]", false, &matched));
  EXPECT_EQ(matched, "12");

  EXPECT_FALSE(reg_exp->Match("[]", true, &matched));
  EXPECT_FALSE(reg_exp->Match("[]", false, &matched));
}

TEST(RegExpAdapter, TestReplace) {
  scoped_ptr<const reg_exp::RegularExpression> reg_exp(
      reg_exp::CreateRegularExpression("[0-9]"));

  std::string s("123-4567 ");
  EXPECT_TRUE(reg_exp->Replace(&s, false, "+"));
  EXPECT_EQ(s, "+23-4567 ");
  EXPECT_TRUE(reg_exp->Replace(&s, false, "+"));
  EXPECT_EQ(s, "++3-4567 ");
  EXPECT_TRUE(reg_exp->Replace(&s, true, "*"));
  EXPECT_EQ(s, "++*-**** ");
  EXPECT_TRUE(reg_exp->Replace(&s, true, "*"));
  EXPECT_EQ(s, "++*-**** ");

  scoped_ptr<const reg_exp::RegularExpression> full_number_expr(
      reg_exp::CreateRegularExpression("(\\d{3})(\\d{3})(\\d{4})"));
  s = "1234567890:0987654321";
  EXPECT_TRUE(full_number_expr->Replace(&s, true, "(\\1) \\2-\\3$1"));
  EXPECT_EQ(s, "(123) 456-7890$1:(098) 765-4321$1");
}

TEST(RegExpAdapter, TestUtf8) {
  // Expression: <tel symbol><opening square bracket>[<alpha>-<omega>]*
  // <closing square bracket>
  scoped_ptr<const reg_exp::RegularExpression> reg_exp(
      reg_exp::CreateRegularExpression(
      "\xe2\x84\xa1\xe2\x8a\x8f([\xce\xb1-\xcf\x89]*)\xe2\x8a\x90"));
  std::string matched;
  // The string is split to avoid problem with MSVC compiler when it thinks
  // 123 is a part of character code.
  EXPECT_FALSE(reg_exp->Match("\xe2\x84\xa1\xe2\x8a\x8f" "123\xe2\x8a\x90",
                              true, &matched));
  EXPECT_TRUE(reg_exp->Match(
      "\xe2\x84\xa1\xe2\x8a\x8f\xce\xb1\xce\xb2\xe2\x8a\x90", true, &matched));
  // <alpha><betha>
  EXPECT_EQ(matched, "\xce\xb1\xce\xb2");
}

}  // namespace reg_exp
