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

#include "gtest/gtest.h"
#include "src/ast/array_accessor_expression.h"
#include "src/ast/bitcast_expression.h"
#include "src/ast/bool_literal.h"
#include "src/ast/identifier_expression.h"
#include "src/ast/scalar_constructor_expression.h"
#include "src/ast/sint_literal.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type_constructor_expression.h"
#include "src/ast/unary_op_expression.h"
#include "src/reader/wgsl/parser_impl.h"
#include "src/reader/wgsl/parser_impl_test_helper.h"
#include "src/type_manager.h"

namespace tint {
namespace reader {
namespace wgsl {
namespace {

TEST_F(ParserImplTest, PrimaryExpression_Ident) {
  auto* p = parser("a");
  auto e = p->primary_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->IsIdentifier());
  auto* ident = e->AsIdentifier();
  EXPECT_EQ(ident->name(), "a");
}

TEST_F(ParserImplTest, PrimaryExpression_TypeDecl) {
  auto* p = parser("vec4<i32>(1, 2, 3, 4))");
  auto e = p->primary_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->IsConstructor());
  ASSERT_TRUE(e->AsConstructor()->IsTypeConstructor());
  auto* ty = e->AsConstructor()->AsTypeConstructor();

  ASSERT_EQ(ty->values().size(), 4u);
  const auto& val = ty->values();
  ASSERT_TRUE(val[0]->IsConstructor());
  ASSERT_TRUE(val[0]->AsConstructor()->IsScalarConstructor());
  auto* ident = val[0]->AsConstructor()->AsScalarConstructor();
  ASSERT_TRUE(ident->literal()->IsSint());
  EXPECT_EQ(ident->literal()->AsSint()->value(), 1);

  ASSERT_TRUE(val[1]->IsConstructor());
  ASSERT_TRUE(val[1]->AsConstructor()->IsScalarConstructor());
  ident = val[1]->AsConstructor()->AsScalarConstructor();
  ASSERT_TRUE(ident->literal()->IsSint());
  EXPECT_EQ(ident->literal()->AsSint()->value(), 2);

  ASSERT_TRUE(val[2]->IsConstructor());
  ASSERT_TRUE(val[2]->AsConstructor()->IsScalarConstructor());
  ident = val[2]->AsConstructor()->AsScalarConstructor();
  ASSERT_TRUE(ident->literal()->IsSint());
  EXPECT_EQ(ident->literal()->AsSint()->value(), 3);

  ASSERT_TRUE(val[3]->IsConstructor());
  ASSERT_TRUE(val[3]->AsConstructor()->IsScalarConstructor());
  ident = val[3]->AsConstructor()->AsScalarConstructor();
  ASSERT_TRUE(ident->literal()->IsSint());
  EXPECT_EQ(ident->literal()->AsSint()->value(), 4);
}

TEST_F(ParserImplTest, PrimaryExpression_TypeDecl_ZeroConstructor) {
  auto* p = parser("vec4<i32>()");
  auto e = p->primary_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->IsConstructor());
  ASSERT_TRUE(e->AsConstructor()->IsTypeConstructor());
  auto* ty = e->AsConstructor()->AsTypeConstructor();

  ASSERT_EQ(ty->values().size(), 0u);
}

TEST_F(ParserImplTest, PrimaryExpression_TypeDecl_InvalidTypeDecl) {
  auto* p = parser("vec4<if>(2., 3., 4., 5.)");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:6: invalid type for vector");
}

TEST_F(ParserImplTest, PrimaryExpression_TypeDecl_MissingLeftParen) {
  auto* p = parser("vec4<f32> 2., 3., 4., 5.)");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:11: expected '(' for type constructor");
}

TEST_F(ParserImplTest, PrimaryExpression_TypeDecl_MissingRightParen) {
  auto* p = parser("vec4<f32>(2., 3., 4., 5.");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:25: expected ')' for type constructor");
}

TEST_F(ParserImplTest, PrimaryExpression_TypeDecl_InvalidValue) {
  auto* p = parser("i32(if(a) {})");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:5: unable to parse argument expression");
}

TEST_F(ParserImplTest, PrimaryExpression_ConstLiteral_True) {
  auto* p = parser("true");
  auto e = p->primary_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->IsConstructor());
  ASSERT_TRUE(e->AsConstructor()->IsScalarConstructor());
  auto* init = e->AsConstructor()->AsScalarConstructor();
  ASSERT_TRUE(init->literal()->IsBool());
  EXPECT_TRUE(init->literal()->AsBool()->IsTrue());
}

TEST_F(ParserImplTest, PrimaryExpression_ParenExpr) {
  auto* p = parser("(a == b)");
  auto e = p->primary_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->IsBinary());
}

TEST_F(ParserImplTest, PrimaryExpression_ParenExpr_MissingRightParen) {
  auto* p = parser("(a == b");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:8: expected ')'");
}

TEST_F(ParserImplTest, PrimaryExpression_ParenExpr_MissingExpr) {
  auto* p = parser("()");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:2: unable to parse expression");
}

TEST_F(ParserImplTest, PrimaryExpression_ParenExpr_InvalidExpr) {
  auto* p = parser("(if (a) {})");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:2: unable to parse expression");
}

TEST_F(ParserImplTest, PrimaryExpression_Cast) {
  auto* f32_type = tm()->Get(std::make_unique<ast::type::F32Type>());

  auto* p = parser("f32(1)");
  auto e = p->primary_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->IsConstructor());
  ASSERT_TRUE(e->AsConstructor()->IsTypeConstructor());

  auto* c = e->AsConstructor()->AsTypeConstructor();
  ASSERT_EQ(c->type(), f32_type);
  ASSERT_EQ(c->values().size(), 1u);

  ASSERT_TRUE(c->values()[0]->IsConstructor());
  ASSERT_TRUE(c->values()[0]->AsConstructor()->IsScalarConstructor());
}

TEST_F(ParserImplTest, PrimaryExpression_Bitcast) {
  auto* f32_type = tm()->Get(std::make_unique<ast::type::F32Type>());

  auto* p = parser("bitcast<f32>(1)");
  auto e = p->primary_expression();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_TRUE(e->IsBitcast());

  auto* c = e->AsBitcast();
  ASSERT_EQ(c->type(), f32_type);

  ASSERT_TRUE(c->expr()->IsConstructor());
  ASSERT_TRUE(c->expr()->AsConstructor()->IsScalarConstructor());
}

TEST_F(ParserImplTest, PrimaryExpression_Bitcast_MissingGreaterThan) {
  auto* p = parser("bitcast<f32(1)");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:12: expected '>' for bitcast expression");
}

TEST_F(ParserImplTest, PrimaryExpression_Bitcast_MissingType) {
  auto* p = parser("bitcast<>(1)");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:9: invalid type for bitcast expression");
}

TEST_F(ParserImplTest, PrimaryExpression_Bitcast_InvalidType) {
  auto* p = parser("bitcast<invalid>(1)");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:9: unknown constructed type 'invalid'");
}

TEST_F(ParserImplTest, PrimaryExpression_Bitcast_MissingLeftParen) {
  auto* p = parser("bitcast<f32>1)");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:13: expected '('");
}

TEST_F(ParserImplTest, PrimaryExpression_Bitcast_MissingRightParen) {
  auto* p = parser("bitcast<f32>(1");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:15: expected ')'");
}

TEST_F(ParserImplTest, PrimaryExpression_Bitcast_MissingExpression) {
  auto* p = parser("bitcast<f32>()");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:14: unable to parse expression");
}

TEST_F(ParserImplTest, PrimaryExpression_bitcast_InvalidExpression) {
  auto* p = parser("bitcast<f32>(if (a) {})");
  auto e = p->primary_expression();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  ASSERT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:14: unable to parse expression");
}

}  // namespace
}  // namespace wgsl
}  // namespace reader
}  // namespace tint
