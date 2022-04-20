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

#include "gtest/gtest-spi.h"
#include "src/tint/ast/discard_statement.h"
#include "src/tint/ast/if_statement.h"
#include "src/tint/ast/test_helper.h"

namespace tint::ast {
namespace {

using ElseStatementTest = TestHelper;

TEST_F(ElseStatementTest, Creation) {
  auto* cond = Expr(true);
  auto* body = create<BlockStatement>(StatementList{
      create<DiscardStatement>(),
  });
  auto* discard = body->statements[0];

  auto* e = create<ElseStatement>(cond, body);
  EXPECT_EQ(e->condition, cond);
  ASSERT_EQ(e->body->statements.size(), 1u);
  EXPECT_EQ(e->body->statements[0], discard);
}

TEST_F(ElseStatementTest, Creation_WithSource) {
  auto* e = create<ElseStatement>(Source{Source::Location{20, 2}}, Expr(true),
                                  Block());
  auto src = e->source;
  EXPECT_EQ(src.range.begin.line, 20u);
  EXPECT_EQ(src.range.begin.column, 2u);
}

TEST_F(ElseStatementTest, IsElse) {
  auto* e = create<ElseStatement>(nullptr, Block());
  EXPECT_TRUE(e->Is<ElseStatement>());
}

TEST_F(ElseStatementTest, HasCondition) {
  auto* cond = Expr(true);
  auto* e = create<ElseStatement>(cond, Block());
  EXPECT_TRUE(e->condition);
}

TEST_F(ElseStatementTest, HasContition_NullCondition) {
  auto* e = create<ElseStatement>(nullptr, Block());
  EXPECT_FALSE(e->condition);
}

TEST_F(ElseStatementTest, Assert_Null_Body) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder b;
        b.create<ElseStatement>(b.Expr(true), nullptr);
      },
      "internal compiler error");
}

TEST_F(ElseStatementTest, Assert_DifferentProgramID_Condition) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder b1;
        ProgramBuilder b2;
        b1.create<ElseStatement>(b2.Expr(true), b1.Block());
      },
      "internal compiler error");
}

TEST_F(ElseStatementTest, Assert_DifferentProgramID_Body) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder b1;
        ProgramBuilder b2;
        b1.create<ElseStatement>(b1.Expr(true), b2.Block());
      },
      "internal compiler error");
}

}  // namespace
}  // namespace tint::ast
