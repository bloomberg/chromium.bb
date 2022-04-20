// Copyright 2022 The Tint Authors.
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

TEST_F(ParserImplTest, IncrementDecrementStmt_Increment) {
  auto p = parser("a++");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* i = e->As<ast::IncrementDecrementStatement>();
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->lhs, nullptr);

  ASSERT_TRUE(i->lhs->Is<ast::IdentifierExpression>());
  auto* ident = i->lhs->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("a"));

  EXPECT_TRUE(i->increment);
}

TEST_F(ParserImplTest, IncrementDecrementStmt_Decrement) {
  auto p = parser("a--");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* i = e->As<ast::IncrementDecrementStatement>();
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->lhs, nullptr);

  ASSERT_TRUE(i->lhs->Is<ast::IdentifierExpression>());
  auto* ident = i->lhs->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("a"));

  EXPECT_FALSE(i->increment);
}

TEST_F(ParserImplTest, IncrementDecrementStmt_Parenthesized) {
  auto p = parser("(a)++");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* i = e->As<ast::IncrementDecrementStatement>();
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->lhs, nullptr);

  ASSERT_TRUE(i->lhs->Is<ast::IdentifierExpression>());
  auto* ident = i->lhs->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("a"));

  EXPECT_TRUE(i->increment);
}

TEST_F(ParserImplTest, IncrementDecrementStmt_ToMember) {
  auto p = parser("a.b.c[2].d++");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* i = e->As<ast::IncrementDecrementStatement>();
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->lhs, nullptr);
  EXPECT_TRUE(i->increment);

  ASSERT_TRUE(i->lhs->Is<ast::MemberAccessorExpression>());
  auto* mem = i->lhs->As<ast::MemberAccessorExpression>();

  ASSERT_TRUE(mem->member->Is<ast::IdentifierExpression>());
  auto* ident = mem->member->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("d"));

  ASSERT_TRUE(mem->structure->Is<ast::IndexAccessorExpression>());
  auto* idx = mem->structure->As<ast::IndexAccessorExpression>();

  ASSERT_NE(idx->index, nullptr);
  ASSERT_TRUE(idx->index->Is<ast::SintLiteralExpression>());
  EXPECT_EQ(idx->index->As<ast::SintLiteralExpression>()->value, 2);

  ASSERT_TRUE(idx->object->Is<ast::MemberAccessorExpression>());
  mem = idx->object->As<ast::MemberAccessorExpression>();
  ASSERT_TRUE(mem->member->Is<ast::IdentifierExpression>());
  ident = mem->member->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("c"));

  ASSERT_TRUE(mem->structure->Is<ast::MemberAccessorExpression>());
  mem = mem->structure->As<ast::MemberAccessorExpression>();

  ASSERT_TRUE(mem->structure->Is<ast::IdentifierExpression>());
  ident = mem->structure->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("a"));

  ASSERT_TRUE(mem->member->Is<ast::IdentifierExpression>());
  ident = mem->member->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("b"));
}

TEST_F(ParserImplTest, IncrementDecrementStmt_InvalidLHS) {
  auto p = parser("{}++");
  auto e = p->assignment_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_EQ(e.value, nullptr);
}

}  // namespace
}  // namespace tint::reader::wgsl
