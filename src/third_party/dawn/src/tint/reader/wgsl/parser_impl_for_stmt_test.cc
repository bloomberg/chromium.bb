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

#include "src/tint/ast/discard_statement.h"

namespace tint::reader::wgsl {
namespace {

using ForStmtTest = ParserImplTest;

// Test an empty for loop.
TEST_F(ForStmtTest, Empty) {
  auto p = parser("for (;;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_EQ(fl->initializer, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop with non-empty body.
TEST_F(ForStmtTest, Body) {
  auto p = parser("for (;;) { discard; }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_EQ(fl->initializer, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  ASSERT_EQ(fl->body->statements.size(), 1u);
  EXPECT_TRUE(fl->body->statements[0]->Is<ast::DiscardStatement>());
}

// Test a for loop declaring a variable in the initializer statement.
TEST_F(ForStmtTest, InitializerStatementDecl) {
  auto p = parser("for (var i: i32 ;;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  ASSERT_TRUE(Is<ast::VariableDeclStatement>(fl->initializer));
  auto* var = fl->initializer->As<ast::VariableDeclStatement>()->variable;
  EXPECT_FALSE(var->is_const);
  EXPECT_EQ(var->constructor, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop declaring and initializing a variable in the initializer
// statement.
TEST_F(ForStmtTest, InitializerStatementDeclEqual) {
  auto p = parser("for (var i: i32 = 0 ;;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  ASSERT_TRUE(Is<ast::VariableDeclStatement>(fl->initializer));
  auto* var = fl->initializer->As<ast::VariableDeclStatement>()->variable;
  EXPECT_FALSE(var->is_const);
  EXPECT_NE(var->constructor, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop declaring a const variable in the initializer statement.
TEST_F(ForStmtTest, InitializerStatementConstDecl) {
  auto p = parser("for (let i: i32 = 0 ;;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  ASSERT_TRUE(Is<ast::VariableDeclStatement>(fl->initializer));
  auto* var = fl->initializer->As<ast::VariableDeclStatement>()->variable;
  EXPECT_TRUE(var->is_const);
  EXPECT_NE(var->constructor, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop assigning a variable in the initializer statement.
TEST_F(ForStmtTest, InitializerStatementAssignment) {
  auto p = parser("for (i = 0 ;;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_TRUE(Is<ast::AssignmentStatement>(fl->initializer));
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop incrementing a variable in the initializer statement.
TEST_F(ForStmtTest, InitializerStatementIncrement) {
  auto p = parser("for (i++;;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_TRUE(Is<ast::IncrementDecrementStatement>(fl->initializer));
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop calling a function in the initializer statement.
TEST_F(ForStmtTest, InitializerStatementFuncCall) {
  auto p = parser("for (a(b,c) ;;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_TRUE(Is<ast::CallStatement>(fl->initializer));
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop with a break condition
TEST_F(ForStmtTest, BreakCondition) {
  auto p = parser("for (; 0 == 1;) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_EQ(fl->initializer, nullptr);
  EXPECT_TRUE(Is<ast::BinaryExpression>(fl->condition));
  EXPECT_EQ(fl->continuing, nullptr);
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop assigning a variable in the continuing statement.
TEST_F(ForStmtTest, ContinuingAssignment) {
  auto p = parser("for (;; x = 2) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_EQ(fl->initializer, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_TRUE(Is<ast::AssignmentStatement>(fl->continuing));
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop with an increment statement as the continuing statement.
TEST_F(ForStmtTest, ContinuingIncrement) {
  auto p = parser("for (;; x++) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_EQ(fl->initializer, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_TRUE(Is<ast::IncrementDecrementStatement>(fl->continuing));
  EXPECT_TRUE(fl->body->Empty());
}

// Test a for loop calling a function in the continuing statement.
TEST_F(ForStmtTest, ContinuingFuncCall) {
  auto p = parser("for (;; a(b,c)) { }");
  auto fl = p->for_stmt();
  EXPECT_FALSE(p->has_error()) << p->error();
  EXPECT_FALSE(fl.errored);
  ASSERT_TRUE(fl.matched);
  EXPECT_EQ(fl->initializer, nullptr);
  EXPECT_EQ(fl->condition, nullptr);
  EXPECT_TRUE(Is<ast::CallStatement>(fl->continuing));
  EXPECT_TRUE(fl->body->Empty());
}

class ForStmtErrorTest : public ParserImplTest {
 public:
  void TestForWithError(std::string for_str, std::string error_str) {
    auto p_for = parser(for_str);
    auto e_for = p_for->for_stmt();

    EXPECT_FALSE(e_for.matched);
    EXPECT_TRUE(e_for.errored);
    EXPECT_TRUE(p_for->has_error());
    ASSERT_EQ(e_for.value, nullptr);
    EXPECT_EQ(p_for->error(), error_str);
  }
};

// Test a for loop with missing left parenthesis is invalid.
TEST_F(ForStmtErrorTest, MissingLeftParen) {
  std::string for_str = "for { }";
  std::string error_str = "1:5: expected '(' for for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with missing first semicolon is invalid.
TEST_F(ForStmtErrorTest, MissingFirstSemicolon) {
  std::string for_str = "for () {}";
  std::string error_str = "1:6: expected ';' for initializer in for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with missing second semicolon is invalid.
TEST_F(ForStmtErrorTest, MissingSecondSemicolon) {
  std::string for_str = "for (;) {}";
  std::string error_str = "1:7: expected ';' for condition in for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with missing right parenthesis is invalid.
TEST_F(ForStmtErrorTest, MissingRightParen) {
  std::string for_str = "for (;; {}";
  std::string error_str = "1:9: expected ')' for for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with missing left brace is invalid.
TEST_F(ForStmtErrorTest, MissingLeftBrace) {
  std::string for_str = "for (;;)";
  std::string error_str = "1:9: expected '{' for for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with missing right brace is invalid.
TEST_F(ForStmtErrorTest, MissingRightBrace) {
  std::string for_str = "for (;;) {";
  std::string error_str = "1:11: expected '}' for for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with an invalid initializer statement.
TEST_F(ForStmtErrorTest, InvalidInitializerAsConstDecl) {
  std::string for_str = "for (let x: i32;;) { }";
  std::string error_str = "1:16: expected '=' for let declaration";

  TestForWithError(for_str, error_str);
}

// Test a for loop with a initializer statement not matching
// variable_stmt | assignment_stmt | func_call_stmt.
TEST_F(ForStmtErrorTest, InvalidInitializerMatch) {
  std::string for_str = "for (if (true) {} ;;) { }";
  std::string error_str = "1:6: expected ';' for initializer in for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with an invalid break condition.
TEST_F(ForStmtErrorTest, InvalidBreakConditionAsExpression) {
  std::string for_str = "for (; (0 == 1; ) { }";
  std::string error_str = "1:15: expected ')'";

  TestForWithError(for_str, error_str);
}

// Test a for loop with a break condition not matching
// logical_or_expression.
TEST_F(ForStmtErrorTest, InvalidBreakConditionMatch) {
  std::string for_str = "for (; var i: i32 = 0;) { }";
  std::string error_str = "1:8: expected ';' for condition in for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with an invalid continuing statement.
TEST_F(ForStmtErrorTest, InvalidContinuingAsFuncCall) {
  std::string for_str = "for (;; a(,) ) { }";
  std::string error_str = "1:11: expected ')' for function call";

  TestForWithError(for_str, error_str);
}

// Test a for loop with a continuing statement not matching
// assignment_stmt | func_call_stmt.
TEST_F(ForStmtErrorTest, InvalidContinuingMatch) {
  std::string for_str = "for (;; var i: i32 = 0) { }";
  std::string error_str = "1:9: expected ')' for for loop";

  TestForWithError(for_str, error_str);
}

// Test a for loop with an invalid body.
TEST_F(ForStmtErrorTest, InvalidBody) {
  std::string for_str = "for (;;) { let x: i32; }";
  std::string error_str = "1:22: expected '=' for let declaration";

  TestForWithError(for_str, error_str);
}

// Test a for loop with a body not matching statements
TEST_F(ForStmtErrorTest, InvalidBodyMatch) {
  std::string for_str = "for (;;) { fn main() {} }";
  std::string error_str = "1:12: expected '}' for for loop";

  TestForWithError(for_str, error_str);
}

}  // namespace
}  // namespace tint::reader::wgsl
