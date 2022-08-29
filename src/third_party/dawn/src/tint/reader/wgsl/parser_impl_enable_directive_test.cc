// Copyright 2022 The Tint Authors.
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

#include "src/tint/ast/enable.h"

namespace tint::reader::wgsl {
namespace {

using EnableDirectiveTest = ParserImplTest;

// Test a valid enable directive.
TEST_F(EnableDirectiveTest, Valid) {
    auto p = parser("enable f16;");
    p->enable_directive();
    EXPECT_FALSE(p->has_error()) << p->error();
    auto program = p->program();
    auto& ast = program.AST();
    ASSERT_EQ(ast.Enables().size(), 1u);
    auto* enable = ast.Enables()[0];
    EXPECT_EQ(enable->extension, ast::Extension::kF16);
    ASSERT_EQ(ast.GlobalDeclarations().size(), 1u);
    EXPECT_EQ(ast.GlobalDeclarations()[0], enable);
}

// Test multiple enable directives for a same extension.
TEST_F(EnableDirectiveTest, EnableMultipleTime) {
    auto p = parser(R"(
enable f16;
enable f16;
)");
    p->translation_unit();
    EXPECT_FALSE(p->has_error()) << p->error();
    auto program = p->program();
    auto& ast = program.AST();
    ASSERT_EQ(ast.Enables().size(), 2u);
    auto* enable_a = ast.Enables()[0];
    auto* enable_b = ast.Enables()[1];
    EXPECT_EQ(enable_a->extension, ast::Extension::kF16);
    EXPECT_EQ(enable_b->extension, ast::Extension::kF16);
    ASSERT_EQ(ast.GlobalDeclarations().size(), 2u);
    EXPECT_EQ(ast.GlobalDeclarations()[0], enable_a);
    EXPECT_EQ(ast.GlobalDeclarations()[1], enable_b);
}

// Test an unknown extension identifier.
TEST_F(EnableDirectiveTest, InvalidIdentifier) {
    auto p = parser("enable NotAValidExtensionName;");
    p->enable_directive();
    // Error when unknown extension found
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:8: unsupported extension: 'NotAValidExtensionName'");
    auto program = p->program();
    auto& ast = program.AST();
    EXPECT_EQ(ast.Enables().size(), 0u);
    EXPECT_EQ(ast.GlobalDeclarations().size(), 0u);
}

// Test an enable directive missing ending semicolon.
TEST_F(EnableDirectiveTest, MissingEndingSemicolon) {
    auto p = parser("enable f16");
    p->translation_unit();
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "1:11: expected ';' for enable directive");
    auto program = p->program();
    auto& ast = program.AST();
    EXPECT_EQ(ast.Enables().size(), 0u);
    EXPECT_EQ(ast.GlobalDeclarations().size(), 0u);
}

// Test using invalid tokens in an enable directive.
TEST_F(EnableDirectiveTest, InvalidTokens) {
    {
        auto p = parser("enable f16<;");
        p->translation_unit();
        EXPECT_TRUE(p->has_error());
        EXPECT_EQ(p->error(), "1:11: expected ';' for enable directive");
        auto program = p->program();
        auto& ast = program.AST();
        EXPECT_EQ(ast.Enables().size(), 0u);
        EXPECT_EQ(ast.GlobalDeclarations().size(), 0u);
    }
    {
        auto p = parser("enable <f16;");
        p->translation_unit();
        EXPECT_TRUE(p->has_error());
        EXPECT_EQ(p->error(), "1:8: invalid extension name");
        auto program = p->program();
        auto& ast = program.AST();
        EXPECT_EQ(ast.Enables().size(), 0u);
        EXPECT_EQ(ast.GlobalDeclarations().size(), 0u);
    }
    {
        auto p = parser("enable =;");
        p->translation_unit();
        EXPECT_TRUE(p->has_error());
        EXPECT_EQ(p->error(), "1:8: invalid extension name");
        auto program = p->program();
        auto& ast = program.AST();
        EXPECT_EQ(ast.Enables().size(), 0u);
        EXPECT_EQ(ast.GlobalDeclarations().size(), 0u);
    }
    {
        auto p = parser("enable vec4;");
        p->translation_unit();
        EXPECT_TRUE(p->has_error());
        EXPECT_EQ(p->error(), "1:8: invalid extension name");
        auto program = p->program();
        auto& ast = program.AST();
        EXPECT_EQ(ast.Enables().size(), 0u);
        EXPECT_EQ(ast.GlobalDeclarations().size(), 0u);
    }
}

// Test an enable directive go after other global declarations.
TEST_F(EnableDirectiveTest, FollowingOtherGlobalDecl) {
    auto p = parser(R"(
var<private> t: f32 = 0f;
enable f16;
)");
    p->translation_unit();
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "3:1: enable directives must come before all global declarations");
    auto program = p->program();
    auto& ast = program.AST();
    // Accept the enable directive although it caused an error
    ASSERT_EQ(ast.Enables().size(), 1u);
    auto* enable = ast.Enables()[0];
    EXPECT_EQ(enable->extension, ast::Extension::kF16);
    ASSERT_EQ(ast.GlobalDeclarations().size(), 2u);
    EXPECT_EQ(ast.GlobalDeclarations()[1], enable);
}

// Test an enable directive go after an empty semicolon.
TEST_F(EnableDirectiveTest, FollowingEmptySemicolon) {
    auto p = parser(R"(
;
enable f16;
)");
    p->translation_unit();
    // An empty semicolon is treated as a global declaration
    EXPECT_TRUE(p->has_error());
    EXPECT_EQ(p->error(), "3:1: enable directives must come before all global declarations");
    auto program = p->program();
    auto& ast = program.AST();
    // Accept the enable directive although it cause an error
    ASSERT_EQ(ast.Enables().size(), 1u);
    auto* enable = ast.Enables()[0];
    EXPECT_EQ(enable->extension, ast::Extension::kF16);
    ASSERT_EQ(ast.GlobalDeclarations().size(), 1u);
    EXPECT_EQ(ast.GlobalDeclarations()[0], enable);
}

}  // namespace
}  // namespace tint::reader::wgsl
