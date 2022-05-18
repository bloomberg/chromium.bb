// Copyright 2021 The Tint Authors.
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
#include "src/tint/ast/binary_expression.h"
#include "src/tint/ast/test_helper.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::ast {
namespace {

using ForLoopStatementTest = TestHelper;

TEST_F(ForLoopStatementTest, Creation) {
    auto* init = Decl(Var("i", ty.u32()));
    auto* cond = create<BinaryExpression>(BinaryOp::kLessThan, Expr("i"), Expr(5_u));
    auto* cont = Assign("i", Add("i", 1_u));
    auto* body = Block(Return());
    auto* l = For(init, cond, cont, body);

    EXPECT_EQ(l->initializer, init);
    EXPECT_EQ(l->condition, cond);
    EXPECT_EQ(l->continuing, cont);
    EXPECT_EQ(l->body, body);
}

TEST_F(ForLoopStatementTest, Creation_WithSource) {
    auto* body = Block(Return());
    auto* l = For(Source{{20u, 2u}}, nullptr, nullptr, nullptr, body);
    auto src = l->source;
    EXPECT_EQ(src.range.begin.line, 20u);
    EXPECT_EQ(src.range.begin.column, 2u);
}

TEST_F(ForLoopStatementTest, Creation_Null_InitCondCont) {
    auto* body = Block(Return());
    auto* l = For(nullptr, nullptr, nullptr, body);
    EXPECT_EQ(l->body, body);
}

TEST_F(ForLoopStatementTest, Assert_Null_Body) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b;
            b.For(nullptr, nullptr, nullptr, nullptr);
        },
        "internal compiler error");
}

TEST_F(ForLoopStatementTest, Assert_DifferentProgramID_Initializer) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.For(b2.Block(), nullptr, nullptr, b1.Block());
        },
        "internal compiler error");
}

TEST_F(ForLoopStatementTest, Assert_DifferentProgramID_Condition) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.For(nullptr, b2.Expr(true), nullptr, b1.Block());
        },
        "internal compiler error");
}

TEST_F(ForLoopStatementTest, Assert_DifferentProgramID_Continuing) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.For(nullptr, nullptr, b2.Block(), b1.Block());
        },
        "internal compiler error");
}

TEST_F(ForLoopStatementTest, Assert_DifferentProgramID_Body) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.For(nullptr, nullptr, nullptr, b2.Block());
        },
        "internal compiler error");
}

}  // namespace
}  // namespace tint::ast
