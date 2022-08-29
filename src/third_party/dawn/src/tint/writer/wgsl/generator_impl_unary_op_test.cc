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

#include "src/tint/writer/wgsl/test_helper.h"

namespace tint::writer::wgsl {
namespace {

using WgslUnaryOpTest = TestHelper;

TEST_F(WgslUnaryOpTest, AddressOf) {
    Global("expr", ty.f32(), ast::StorageClass::kPrivate);
    auto* op = create<ast::UnaryOpExpression>(ast::UnaryOp::kAddressOf, Expr("expr"));
    WrapInFunction(op);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitExpression(out, op)) << gen.error();
    EXPECT_EQ(out.str(), "&(expr)");
}

TEST_F(WgslUnaryOpTest, Complement) {
    Global("expr", ty.u32(), ast::StorageClass::kPrivate);
    auto* op = create<ast::UnaryOpExpression>(ast::UnaryOp::kComplement, Expr("expr"));
    WrapInFunction(op);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitExpression(out, op)) << gen.error();
    EXPECT_EQ(out.str(), "~(expr)");
}

TEST_F(WgslUnaryOpTest, Indirection) {
    Global("G", ty.f32(), ast::StorageClass::kPrivate);
    auto* p =
        Let("expr", nullptr, create<ast::UnaryOpExpression>(ast::UnaryOp::kAddressOf, Expr("G")));
    auto* op = create<ast::UnaryOpExpression>(ast::UnaryOp::kIndirection, Expr("expr"));
    WrapInFunction(p, op);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitExpression(out, op)) << gen.error();
    EXPECT_EQ(out.str(), "*(expr)");
}

TEST_F(WgslUnaryOpTest, Not) {
    Global("expr", ty.bool_(), ast::StorageClass::kPrivate);
    auto* op = create<ast::UnaryOpExpression>(ast::UnaryOp::kNot, Expr("expr"));
    WrapInFunction(op);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitExpression(out, op)) << gen.error();
    EXPECT_EQ(out.str(), "!(expr)");
}

TEST_F(WgslUnaryOpTest, Negation) {
    Global("expr", ty.i32(), ast::StorageClass::kPrivate);
    auto* op = create<ast::UnaryOpExpression>(ast::UnaryOp::kNegation, Expr("expr"));
    WrapInFunction(op);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitExpression(out, op)) << gen.error();
    EXPECT_EQ(out.str(), "-(expr)");
}

}  // namespace
}  // namespace tint::writer::wgsl
