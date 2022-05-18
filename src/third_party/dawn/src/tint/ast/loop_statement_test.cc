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

#include "src/tint/ast/loop_statement.h"

#include "gtest/gtest-spi.h"
#include "src/tint/ast/discard_statement.h"
#include "src/tint/ast/if_statement.h"
#include "src/tint/ast/test_helper.h"

namespace tint::ast {
namespace {

using LoopStatementTest = TestHelper;

TEST_F(LoopStatementTest, Creation) {
    auto* body = Block(create<DiscardStatement>());
    auto* b = body->Last();

    auto* continuing = Block(create<DiscardStatement>());

    auto* l = create<LoopStatement>(body, continuing);
    ASSERT_EQ(l->body->statements.size(), 1u);
    EXPECT_EQ(l->body->statements[0], b);
    ASSERT_EQ(l->continuing->statements.size(), 1u);
    EXPECT_EQ(l->continuing->statements[0], continuing->Last());
}

TEST_F(LoopStatementTest, Creation_WithSource) {
    auto* body = Block(create<DiscardStatement>());

    auto* continuing = Block(create<DiscardStatement>());

    auto* l = create<LoopStatement>(Source{Source::Location{20, 2}}, body, continuing);
    auto src = l->source;
    EXPECT_EQ(src.range.begin.line, 20u);
    EXPECT_EQ(src.range.begin.column, 2u);
}

TEST_F(LoopStatementTest, IsLoop) {
    auto* l = create<LoopStatement>(Block(), Block());
    EXPECT_TRUE(l->Is<LoopStatement>());
}

TEST_F(LoopStatementTest, HasContinuing_WithoutContinuing) {
    auto* body = Block(create<DiscardStatement>());

    auto* l = create<LoopStatement>(body, nullptr);
    EXPECT_FALSE(l->continuing);
}

TEST_F(LoopStatementTest, HasContinuing_WithContinuing) {
    auto* body = Block(create<DiscardStatement>());

    auto* continuing = Block(create<DiscardStatement>());

    auto* l = create<LoopStatement>(body, continuing);
    EXPECT_TRUE(l->continuing);
}

TEST_F(LoopStatementTest, Assert_Null_Body) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b;
            b.create<LoopStatement>(nullptr, nullptr);
        },
        "internal compiler error");
}

TEST_F(LoopStatementTest, Assert_DifferentProgramID_Body) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.create<LoopStatement>(b2.Block(), b1.Block());
        },
        "internal compiler error");
}

TEST_F(LoopStatementTest, Assert_DifferentProgramID_Continuing) {
    EXPECT_FATAL_FAILURE(
        {
            ProgramBuilder b1;
            ProgramBuilder b2;
            b1.create<LoopStatement>(b1.Block(), b2.Block());
        },
        "internal compiler error");
}

}  // namespace
}  // namespace tint::ast
