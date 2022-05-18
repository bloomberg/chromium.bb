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

TEST_F(ParserImplTest, IfStmt) {
    auto p = parser("if a == 4 { a = b; c = d; }");
    auto e = p->if_stmt();
    EXPECT_TRUE(e.matched);
    EXPECT_FALSE(e.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(e.value, nullptr);

    ASSERT_TRUE(e->Is<ast::IfStatement>());
    ASSERT_NE(e->condition, nullptr);
    ASSERT_TRUE(e->condition->Is<ast::BinaryExpression>());
    EXPECT_EQ(e->body->statements.size(), 2u);
    EXPECT_EQ(e->else_statement, nullptr);
}

TEST_F(ParserImplTest, IfStmt_WithElse) {
    auto p = parser("if a == 4 { a = b; c = d; } else if(c) { d = 2; } else {}");
    auto e = p->if_stmt();
    EXPECT_TRUE(e.matched);
    EXPECT_FALSE(e.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(e.value, nullptr);

    ASSERT_TRUE(e->Is<ast::IfStatement>());
    ASSERT_NE(e->condition, nullptr);
    ASSERT_TRUE(e->condition->Is<ast::BinaryExpression>());
    EXPECT_EQ(e->body->statements.size(), 2u);

    auto* elseif = As<ast::IfStatement>(e->else_statement);
    ASSERT_NE(elseif, nullptr);
    ASSERT_TRUE(elseif->condition->Is<ast::IdentifierExpression>());
    EXPECT_EQ(elseif->body->statements.size(), 1u);

    auto* el = As<ast::BlockStatement>(elseif->else_statement);
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->statements.size(), 0u);
}

TEST_F(ParserImplTest, IfStmt_WithElse_WithParens) {
    auto p = parser("if(a==4) { a = b; c = d; } else if(c) { d = 2; } else {}");
    auto e = p->if_stmt();
    EXPECT_TRUE(e.matched);
    EXPECT_FALSE(e.errored);
    EXPECT_FALSE(p->has_error()) << p->error();
    ASSERT_NE(e.value, nullptr);

    ASSERT_TRUE(e->Is<ast::IfStatement>());
    ASSERT_NE(e->condition, nullptr);
    ASSERT_TRUE(e->condition->Is<ast::BinaryExpression>());
    EXPECT_EQ(e->body->statements.size(), 2u);

    auto* elseif = As<ast::IfStatement>(e->else_statement);
    ASSERT_NE(elseif, nullptr);
    ASSERT_TRUE(elseif->condition->Is<ast::IdentifierExpression>());
    EXPECT_EQ(elseif->body->statements.size(), 1u);

    auto* el = As<ast::BlockStatement>(elseif->else_statement);
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(el->statements.size(), 0u);
}

TEST_F(ParserImplTest, IfStmt_InvalidCondition) {
    auto p = parser("if a = 3 {}");
    auto e = p->if_stmt();
    EXPECT_FALSE(e.matched);
    EXPECT_TRUE(e.errored);
    EXPECT_EQ(e.value, nullptr);
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:6: expected '{'");
}

TEST_F(ParserImplTest, IfStmt_MissingCondition) {
    auto p = parser("if {}");
    auto e = p->if_stmt();
    EXPECT_FALSE(e.matched);
    EXPECT_TRUE(e.errored);
    EXPECT_EQ(e.value, nullptr);
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:4: unable to parse condition expression");
}

TEST_F(ParserImplTest, IfStmt_InvalidBody) {
    auto p = parser("if a { fn main() {}}");
    auto e = p->if_stmt();
    EXPECT_FALSE(e.matched);
    EXPECT_TRUE(e.errored);
    EXPECT_EQ(e.value, nullptr);
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:8: expected '}'");
}

TEST_F(ParserImplTest, IfStmt_MissingBody) {
    auto p = parser("if a");
    auto e = p->if_stmt();
    EXPECT_FALSE(e.matched);
    EXPECT_TRUE(e.errored);
    EXPECT_EQ(e.value, nullptr);
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:5: expected '{'");
}

TEST_F(ParserImplTest, IfStmt_InvalidElseif) {
    auto p = parser("if a {} else if a { fn main() -> a{}}");
    auto e = p->if_stmt();
    EXPECT_FALSE(e.matched);
    EXPECT_TRUE(e.errored);
    EXPECT_EQ(e.value, nullptr);
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:21: expected '}'");
}

TEST_F(ParserImplTest, IfStmt_InvalidElse) {
    auto p = parser("if a {} else { fn main() -> a{}}");
    auto e = p->if_stmt();
    EXPECT_FALSE(e.matched);
    EXPECT_TRUE(e.errored);
    EXPECT_EQ(e.value, nullptr);
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:16: expected '}'");
}

}  // namespace
}  // namespace tint::reader::wgsl
