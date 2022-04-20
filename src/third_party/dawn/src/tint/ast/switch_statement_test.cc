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

#include "src/tint/ast/switch_statement.h"

#include "gtest/gtest-spi.h"
#include "src/tint/ast/test_helper.h"

namespace tint::ast {
namespace {

using SwitchStatementTest = TestHelper;

TEST_F(SwitchStatementTest, Creation) {
  CaseSelectorList lit;
  lit.push_back(create<SintLiteralExpression>(1));

  auto* ident = Expr("ident");
  CaseStatementList body;
  auto* case_stmt = create<CaseStatement>(lit, Block());
  body.push_back(case_stmt);

  auto* stmt = create<SwitchStatement>(ident, body);
  EXPECT_EQ(stmt->condition, ident);
  ASSERT_EQ(stmt->body.size(), 1u);
  EXPECT_EQ(stmt->body[0], case_stmt);
}

TEST_F(SwitchStatementTest, Creation_WithSource) {
  auto* ident = Expr("ident");

  auto* stmt = create<SwitchStatement>(Source{Source::Location{20, 2}}, ident,
                                       CaseStatementList());
  auto src = stmt->source;
  EXPECT_EQ(src.range.begin.line, 20u);
  EXPECT_EQ(src.range.begin.column, 2u);
}

TEST_F(SwitchStatementTest, IsSwitch) {
  CaseSelectorList lit;
  lit.push_back(create<SintLiteralExpression>(2));

  auto* ident = Expr("ident");
  CaseStatementList body;
  body.push_back(create<CaseStatement>(lit, Block()));

  auto* stmt = create<SwitchStatement>(ident, body);
  EXPECT_TRUE(stmt->Is<SwitchStatement>());
}

TEST_F(SwitchStatementTest, Assert_Null_Condition) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder b;
        CaseStatementList cases;
        cases.push_back(
            b.create<CaseStatement>(CaseSelectorList{b.Expr(1)}, b.Block()));
        b.create<SwitchStatement>(nullptr, cases);
      },
      "internal compiler error");
}

TEST_F(SwitchStatementTest, Assert_Null_CaseStatement) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder b;
        b.create<SwitchStatement>(b.Expr(true), CaseStatementList{nullptr});
      },
      "internal compiler error");
}

TEST_F(SwitchStatementTest, Assert_DifferentProgramID_Condition) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder b1;
        ProgramBuilder b2;
        b1.create<SwitchStatement>(b2.Expr(true), CaseStatementList{
                                                      b1.create<CaseStatement>(
                                                          CaseSelectorList{
                                                              b1.Expr(1),
                                                          },
                                                          b1.Block()),
                                                  });
      },
      "internal compiler error");
}

TEST_F(SwitchStatementTest, Assert_DifferentProgramID_CaseStatement) {
  EXPECT_FATAL_FAILURE(
      {
        ProgramBuilder b1;
        ProgramBuilder b2;
        b1.create<SwitchStatement>(b1.Expr(true), CaseStatementList{
                                                      b2.create<CaseStatement>(
                                                          CaseSelectorList{
                                                              b2.Expr(1),
                                                          },
                                                          b2.Block()),
                                                  });
      },
      "internal compiler error");
}

}  // namespace
}  // namespace tint::ast
