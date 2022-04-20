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

#include "src/tint/ast/break_statement.h"
#include "src/tint/ast/continue_statement.h"
#include "src/tint/ast/discard_statement.h"
#include "src/tint/reader/wgsl/parser_impl_test_helper.h"

namespace tint::reader::wgsl {
namespace {

TEST_F(ParserImplTest, Statement) {
  auto p = parser("return;");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::ReturnStatement>());
}

TEST_F(ParserImplTest, Statement_Semicolon) {
  auto p = parser(";");
  p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
}

TEST_F(ParserImplTest, Statement_Return_NoValue) {
  auto p = parser("return;");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::ReturnStatement>());
  auto* ret = e->As<ast::ReturnStatement>();
  ASSERT_EQ(ret->value, nullptr);
}

TEST_F(ParserImplTest, Statement_Return_Value) {
  auto p = parser("return a + b * (.1 - .2);");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();

  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::ReturnStatement>());
  auto* ret = e->As<ast::ReturnStatement>();
  ASSERT_NE(ret->value, nullptr);
  EXPECT_TRUE(ret->value->Is<ast::BinaryExpression>());
}

TEST_F(ParserImplTest, Statement_Return_MissingSemi) {
  auto p = parser("return");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:7: expected ';' for return statement");
}

TEST_F(ParserImplTest, Statement_Return_Invalid) {
  auto p = parser("return if(a) {};");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:8: expected ';' for return statement");
}

TEST_F(ParserImplTest, Statement_If) {
  auto p = parser("if (a) {}");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::IfStatement>());
}

TEST_F(ParserImplTest, Statement_If_Invalid) {
  auto p = parser("if (a) { fn main() -> {}}");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:10: expected '}'");
}

TEST_F(ParserImplTest, Statement_Variable) {
  auto p = parser("var a : i32 = 1;");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::VariableDeclStatement>());
}

TEST_F(ParserImplTest, Statement_Variable_Invalid) {
  auto p = parser("var a : i32 =;");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:14: missing constructor for variable declaration");
}

TEST_F(ParserImplTest, Statement_Variable_MissingSemicolon) {
  auto p = parser("var a : i32");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:12: expected ';' for variable declaration");
}

TEST_F(ParserImplTest, Statement_Switch) {
  auto p = parser("switch (a) {}");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::SwitchStatement>());
}

TEST_F(ParserImplTest, Statement_Switch_Invalid) {
  auto p = parser("switch (a) { case: {}}");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:18: unable to parse case selectors");
}

TEST_F(ParserImplTest, Statement_Loop) {
  auto p = parser("loop {}");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::LoopStatement>());
}

TEST_F(ParserImplTest, Statement_Loop_Invalid) {
  auto p = parser("loop discard; }");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:6: expected '{' for loop");
}

TEST_F(ParserImplTest, Statement_Assignment) {
  auto p = parser("a = b;");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::AssignmentStatement>());
}

TEST_F(ParserImplTest, Statement_Assignment_Invalid) {
  auto p = parser("a = if(b) {};");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:5: unable to parse right side of assignment");
}

TEST_F(ParserImplTest, Statement_Assignment_MissingSemicolon) {
  auto p = parser("a = b");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:6: expected ';' for assignment statement");
}

TEST_F(ParserImplTest, Statement_Break) {
  auto p = parser("break;");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::BreakStatement>());
}

TEST_F(ParserImplTest, Statement_Break_MissingSemicolon) {
  auto p = parser("break");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:6: expected ';' for break statement");
}

TEST_F(ParserImplTest, Statement_Continue) {
  auto p = parser("continue;");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::ContinueStatement>());
}

TEST_F(ParserImplTest, Statement_Continue_MissingSemicolon) {
  auto p = parser("continue");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:9: expected ';' for continue statement");
}

TEST_F(ParserImplTest, Statement_Discard) {
  auto p = parser("discard;");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  ASSERT_NE(e.value, nullptr);
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::DiscardStatement>());
}

TEST_F(ParserImplTest, Statement_Discard_MissingSemicolon) {
  auto p = parser("discard");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_EQ(e.value, nullptr);
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(p->error(), "1:8: expected ';' for discard statement");
}

TEST_F(ParserImplTest, Statement_Body) {
  auto p = parser("{ var i: i32; }");
  auto e = p->statement();
  ASSERT_FALSE(p->has_error()) << p->error();
  EXPECT_TRUE(e.matched);
  EXPECT_FALSE(e.errored);
  ASSERT_TRUE(e->Is<ast::BlockStatement>());
  EXPECT_TRUE(e->As<ast::BlockStatement>()
                  ->statements[0]
                  ->Is<ast::VariableDeclStatement>());
}

TEST_F(ParserImplTest, Statement_Body_Invalid) {
  auto p = parser("{ fn main() -> {}}");
  auto e = p->statement();
  EXPECT_TRUE(p->has_error());
  EXPECT_TRUE(e.errored);
  EXPECT_FALSE(e.matched);
  EXPECT_EQ(e.value, nullptr);
  EXPECT_EQ(p->error(), "1:3: expected '}'");
}

}  // namespace
}  // namespace tint::reader::wgsl
