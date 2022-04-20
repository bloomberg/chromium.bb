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

#include "src/tint/ast/discard_statement.h"
#include "src/tint/reader/wgsl/parser_impl_test_helper.h"

namespace tint::reader::wgsl {
namespace {

TEST_F(ParserImplTest, LoopStmt_BodyNoContinuing) {
  auto p = parser("loop { discard; }");
  auto e = p->loop_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  ASSERT_EQ(e->body->statements.size(), 1u);
  EXPECT_TRUE(e->body->statements[0]->Is<ast::DiscardStatement>());

  EXPECT_EQ(e->continuing->statements.size(), 0u);
}

TEST_F(ParserImplTest, LoopStmt_BodyWithContinuing) {
  auto p = parser("loop { discard; continuing { discard; }}");
  auto e = p->loop_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);

  ASSERT_EQ(e->body->statements.size(), 1u);
  EXPECT_TRUE(e->body->statements[0]->Is<ast::DiscardStatement>());

  EXPECT_EQ(e->continuing->statements.size(), 1u);
  EXPECT_TRUE(e->continuing->statements[0]->Is<ast::DiscardStatement>());
}

TEST_F(ParserImplTest, LoopStmt_NoBodyNoContinuing) {
  auto p = parser("loop { }");
  auto e = p->loop_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_EQ(e->body->statements.size(), 0u);
  ASSERT_EQ(e->continuing->statements.size(), 0u);
}

TEST_F(ParserImplTest, LoopStmt_NoBodyWithContinuing) {
  auto p = parser("loop { continuing { discard; }}");
  auto e = p->loop_stmt();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  EXPECT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  ASSERT_EQ(e->body->statements.size(), 0u);
  ASSERT_EQ(e->continuing->statements.size(), 1u);
  EXPECT_TRUE(e->continuing->statements[0]->Is<ast::DiscardStatement>());
}

TEST_F(ParserImplTest, LoopStmt_MissingBracketLeft) {
  auto p = parser("loop discard; }");
  auto e = p->loop_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:6: expected '{' for loop");
}

TEST_F(ParserImplTest, LoopStmt_MissingBracketRight) {
  auto p = parser("loop { discard; ");
  auto e = p->loop_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:17: expected '}' for loop");
}

TEST_F(ParserImplTest, LoopStmt_InvalidStatements) {
  auto p = parser("loop { discard }");
  auto e = p->loop_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:16: expected ';' for discard statement");
}

TEST_F(ParserImplTest, LoopStmt_InvalidContinuing) {
  auto p = parser("loop { continuing { discard }}");
  auto e = p->loop_stmt();
  EXPECT_FALSE(e.matched);
  EXPECT_TRUE(e.errored);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(p->error(), "1:29: expected ';' for discard statement");
}

}  // namespace
}  // namespace tint::reader::wgsl
