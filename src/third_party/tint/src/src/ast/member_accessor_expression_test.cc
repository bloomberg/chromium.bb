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

#include "src/ast/member_accessor_expression.h"

#include <sstream>

#include "src/ast/identifier_expression.h"
#include "src/ast/test_helper.h"

namespace tint {
namespace ast {
namespace {

using MemberAccessorExpressionTest = TestHelper;

TEST_F(MemberAccessorExpressionTest, Creation) {
  auto* str = Expr("structure");
  auto* mem = Expr("member");

  auto* stmt = create<MemberAccessorExpression>(str, mem);
  EXPECT_EQ(stmt->structure(), str);
  EXPECT_EQ(stmt->member(), mem);
}

TEST_F(MemberAccessorExpressionTest, Creation_WithSource) {
  auto* stmt = create<MemberAccessorExpression>(
      Source{Source::Location{20, 2}}, Expr("structure"), Expr("member"));
  auto src = stmt->source();
  EXPECT_EQ(src.range.begin.line, 20u);
  EXPECT_EQ(src.range.begin.column, 2u);
}

TEST_F(MemberAccessorExpressionTest, IsMemberAccessor) {
  auto* stmt =
      create<MemberAccessorExpression>(Expr("structure"), Expr("member"));
  EXPECT_TRUE(stmt->Is<MemberAccessorExpression>());
}

TEST_F(MemberAccessorExpressionTest, IsValid) {
  auto* stmt =
      create<MemberAccessorExpression>(Expr("structure"), Expr("member"));
  EXPECT_TRUE(stmt->IsValid());
}

TEST_F(MemberAccessorExpressionTest, IsValid_NullStruct) {
  auto* stmt = create<MemberAccessorExpression>(nullptr, Expr("member"));
  EXPECT_FALSE(stmt->IsValid());
}

TEST_F(MemberAccessorExpressionTest, IsValid_InvalidStruct) {
  auto* stmt = create<MemberAccessorExpression>(Expr(""), Expr("member"));
  EXPECT_FALSE(stmt->IsValid());
}

TEST_F(MemberAccessorExpressionTest, IsValid_NullMember) {
  auto* stmt = create<MemberAccessorExpression>(Expr("structure"), nullptr);
  EXPECT_FALSE(stmt->IsValid());
}

TEST_F(MemberAccessorExpressionTest, IsValid_InvalidMember) {
  auto* stmt = create<MemberAccessorExpression>(Expr("structure"), Expr(""));
  EXPECT_FALSE(stmt->IsValid());
}

TEST_F(MemberAccessorExpressionTest, ToStr) {
  auto* stmt =
      create<MemberAccessorExpression>(Expr("structure"), Expr("member"));
  EXPECT_EQ(str(stmt), R"(MemberAccessor[not set]{
  Identifier[not set]{structure}
  Identifier[not set]{member}
}
)");
}

}  // namespace
}  // namespace ast
}  // namespace tint
