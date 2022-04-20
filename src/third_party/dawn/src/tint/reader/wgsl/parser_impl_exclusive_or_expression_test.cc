// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/tint/reader/wgsl/parser_impl_test_helper.h"

namespace tint::reader::wgsl {
namespace {

TEST_F(ParserImplTest, ExclusiveOrExpression_Parses) {
  auto p = parser("a ^ true");
  auto e = p->exclusive_or_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  ASSERT_TRUE(e->Is<ast::BinaryExpression>());
  auto* rel = e->As<ast::BinaryExpression>();
  EXPECT_EQ(ast::BinaryOp::kXor, rel->op);

  ASSERT_TRUE(rel->lhs->Is<ast::IdentifierExpression>());
  auto* ident = rel->lhs->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("a"));

  ASSERT_TRUE(rel->rhs->Is<ast::BoolLiteralExpression>());
  ASSERT_TRUE(rel->rhs->As<ast::BoolLiteralExpression>()->value);
}

TEST_F(ParserImplTest, ExclusiveOrExpression_InvalidLHS) {
  auto p = parser("if (a) {} ^ true");
  auto e = p->exclusive_or_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_EQ(e.value, nullptr);
}

TEST_F(ParserImplTest, ExclusiveOrExpression_InvalidRHS) {
  auto p = parser("true ^ if (a) {}");
  auto e = p->exclusive_or_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:8: unable to parse right side of ^ expression");
}

TEST_F(ParserImplTest, ExclusiveOrExpression_NoOr_ReturnsLHS) {
  auto p = parser("a true");
  auto e = p->exclusive_or_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->Is<ast::IdentifierExpression>());
}

}  // namespace
}  // namespace tint::reader::wgsl
