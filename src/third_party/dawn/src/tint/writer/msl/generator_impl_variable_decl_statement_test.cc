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

#include "gmock/gmock.h"
#include "src/tint/ast/variable_decl_statement.h"
#include "src/tint/writer/msl/test_helper.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::writer::msl {
namespace {

using ::testing::HasSubstr;

using MslGeneratorImplTest = TestHelper;

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement) {
    auto* var = Var("a", ty.f32(), ast::StorageClass::kNone);
    auto* stmt = Decl(var);
    WrapInFunction(stmt);

    GeneratorImpl& gen = Build();

    gen.increment_indent();

    ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
    EXPECT_EQ(gen.result(), "  float a = 0.0f;\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Const) {
    auto* var = Let("a", ty.f32(), Construct(ty.f32()));
    auto* stmt = Decl(var);
    WrapInFunction(stmt);

    GeneratorImpl& gen = Build();

    gen.increment_indent();

    ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
    EXPECT_EQ(gen.result(), "  float const a = float();\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Array) {
    auto* var = Var("a", ty.array<f32, 5>(), ast::StorageClass::kNone);
    auto* stmt = Decl(var);
    WrapInFunction(stmt);

    GeneratorImpl& gen = Build();

    gen.increment_indent();

    ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
    EXPECT_EQ(gen.result(), "  float a[5] = {0.0f};\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Struct) {
    auto* s = Structure("S", {
                                 Member("a", ty.f32()),
                                 Member("b", ty.f32()),
                             });

    auto* var = Var("a", ty.Of(s), ast::StorageClass::kNone);
    auto* stmt = Decl(var);
    WrapInFunction(stmt);

    GeneratorImpl& gen = Build();

    gen.increment_indent();

    ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
    EXPECT_EQ(gen.result(), R"(  S a = {};
)");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Vector) {
    auto* var = Var("a", ty.vec2<f32>());
    auto* stmt = Decl(var);
    WrapInFunction(stmt);

    GeneratorImpl& gen = Build();

    gen.increment_indent();

    ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
    EXPECT_EQ(gen.result(), "  float2 a = 0.0f;\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Matrix) {
    auto* var = Var("a", ty.mat3x2<f32>());

    auto* stmt = Decl(var);
    WrapInFunction(stmt);

    GeneratorImpl& gen = Build();

    gen.increment_indent();

    ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
    EXPECT_EQ(gen.result(), "  float3x2 a = float3x2(0.0f);\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Private) {
    Global("a", ty.f32(), ast::StorageClass::kPrivate);

    WrapInFunction(Expr("a"));

    GeneratorImpl& gen = SanitizeAndBuild();

    gen.increment_indent();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_THAT(gen.result(), HasSubstr("thread float tint_symbol_1 = 0.0f;\n"));
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Initializer_Private) {
    GlobalConst("initializer", ty.f32(), Expr(0_f));
    Global("a", ty.f32(), ast::StorageClass::kPrivate, Expr("initializer"));

    WrapInFunction(Expr("a"));

    GeneratorImpl& gen = SanitizeAndBuild();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_THAT(gen.result(), HasSubstr("thread float tint_symbol_1 = initializer;\n"));
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Workgroup) {
    Global("a", ty.f32(), ast::StorageClass::kWorkgroup);

    WrapInFunction(Expr("a"));

    GeneratorImpl& gen = SanitizeAndBuild();

    gen.increment_indent();

    ASSERT_TRUE(gen.Generate()) << gen.error();
    EXPECT_THAT(gen.result(), HasSubstr("threadgroup float tint_symbol_2;\n"));
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Initializer_ZeroVec) {
    auto* zero_vec = vec3<f32>();

    auto* var = Var("a", ty.vec3<f32>(), ast::StorageClass::kNone, zero_vec);
    auto* stmt = Decl(var);
    WrapInFunction(stmt);

    GeneratorImpl& gen = Build();

    ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
    EXPECT_EQ(gen.result(), R"(float3 a = float3();
)");
}

}  // namespace
}  // namespace tint::writer::msl
