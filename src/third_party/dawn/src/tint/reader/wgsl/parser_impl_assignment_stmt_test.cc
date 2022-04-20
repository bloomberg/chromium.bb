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

TEST_F(ParserImplTest, AssignmentStmt_Parses_ToVariable) {
  auto p = parser("a = 123");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* a = e->As<ast::AssignmentStatement>();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->lhs, nullptr);
  ASSERT_NE(a->rhs, nullptr);

  ASSERT_TRUE(a->lhs->Is<ast::IdentifierExpression>());
  auto* ident = a->lhs->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("a"));

  ASSERT_TRUE(a->rhs->Is<ast::SintLiteralExpression>());
  EXPECT_EQ(a->rhs->As<ast::SintLiteralExpression>()->value, 123);
}

TEST_F(ParserImplTest, AssignmentStmt_Parses_ToMember) {
  auto p = parser("a.b.c[2].d = 123");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* a = e->As<ast::AssignmentStatement>();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->lhs, nullptr);
  ASSERT_NE(a->rhs, nullptr);

  ASSERT_TRUE(a->rhs->Is<ast::SintLiteralExpression>());
  EXPECT_EQ(a->rhs->As<ast::SintLiteralExpression>()->value, 123);

  ASSERT_TRUE(a->lhs->Is<ast::MemberAccessorExpression>());
  auto* mem = a->lhs->As<ast::MemberAccessorExpression>();

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

TEST_F(ParserImplTest, AssignmentStmt_Parses_ToPhony) {
  auto p = parser("_ = 123");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* a = e->As<ast::AssignmentStatement>();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->lhs, nullptr);
  ASSERT_NE(a->rhs, nullptr);

  ASSERT_TRUE(a->rhs->Is<ast::SintLiteralExpression>());
  EXPECT_EQ(a->rhs->As<ast::SintLiteralExpression>()->value, 123);

  ASSERT_TRUE(a->lhs->Is<ast::PhonyExpression>());
}

TEST_F(ParserImplTest, AssignmentStmt_Parses_CompoundOp) {
  auto p = parser("a += 123");
  auto e = p->assignment_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  auto* a = e->As<ast::CompoundAssignmentStatement>();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->lhs, nullptr);
  ASSERT_NE(a->rhs, nullptr);
  EXPECT_EQ(a->op, ast::BinaryOp::kAdd);

  ASSERT_TRUE(a->lhs->Is<ast::IdentifierExpression>());
  auto* ident = a->lhs->As<ast::IdentifierExpression>();
  EXPECT_EQ(ident->symbol, p->builder().Symbols().Get("a"));

  ASSERT_TRUE(a->rhs->Is<ast::SintLiteralExpression>());
  EXPECT_EQ(a->rhs->As<ast::SintLiteralExpression>()->value, 123);
}

TEST_F(ParserImplTest, AssignmentStmt_MissingEqual) {
  auto p = parser("a.b.c[2].d 123");
  auto e = p->assignment_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:12: expected '=' for assignment");
}

TEST_F(ParserImplTest, AssignmentStmt_Compound_MissingEqual) {
  auto p = parser("a + 123");
  auto e = p->assignment_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:3: expected '=' for assignment");
}

TEST_F(ParserImplTest, AssignmentStmt_InvalidLHS) {
  auto p = parser("if (true) {} = 123");
  auto e = p->assignment_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_EQ(e.value, nullptr);
}

TEST_F(ParserImplTest, AssignmentStmt_InvalidRHS) {
  auto p = parser("a.b.c[2].d = if (true) {}");
  auto e = p->assignment_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:14: unable to parse right side of assignment");
}

TEST_F(ParserImplTest, AssignmentStmt_InvalidCompoundOp) {
  auto p = parser("a &&= true");
  auto e = p->assignment_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:3: expected '=' for assignment");
}

}  // namespace
}  // namespace tint::reader::wgsl
