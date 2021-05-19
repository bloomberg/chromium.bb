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

#include "src/ast/variable_decl_statement.h"
#include "src/writer/msl/test_helper.h"

namespace tint {
namespace writer {
namespace msl {
namespace {

using MslGeneratorImplTest = TestHelper;

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement) {
  auto* var = Var("a", ty.f32(), ast::StorageClass::kNone);
  auto* stmt = create<ast::VariableDeclStatement>(var);
  WrapInFunction(stmt);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), "  float a = 0.0f;\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Const) {
  auto* var = Const("a", ty.f32());
  auto* stmt = create<ast::VariableDeclStatement>(var);
  WrapInFunction(stmt);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), "  const float a = 0.0f;\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Array) {
  type::Array ary(ty.f32(), 5, ast::DecorationList{});

  auto* var = Var("a", &ary, ast::StorageClass::kNone);
  auto* stmt = create<ast::VariableDeclStatement>(var);
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

  auto* var = Var("a", s, ast::StorageClass::kNone);
  auto* stmt = create<ast::VariableDeclStatement>(var);
  WrapInFunction(stmt);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), R"(  S a = {};
)");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Vector) {
  auto* var = Var("a", ty.vec2<f32>(), ast::StorageClass::kFunction);
  auto* stmt = create<ast::VariableDeclStatement>(var);
  WrapInFunction(stmt);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), "  float2 a = 0.0f;\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Matrix) {
  auto* var = Var("a", ty.mat3x2<f32>(), ast::StorageClass::kFunction);

  auto* stmt = create<ast::VariableDeclStatement>(var);
  WrapInFunction(stmt);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), "  float3x2 a = 0.0f;\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Private) {
  auto* var = Global("a", ty.f32(), ast::StorageClass::kPrivate);
  auto* stmt = create<ast::VariableDeclStatement>(var);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), "  float a = 0.0f;\n");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Initializer_Private) {
  Global("initializer", ty.f32(), ast::StorageClass::kInput);
  auto* var =
      Global("a", ty.f32(), ast::StorageClass::kPrivate, Expr("initializer"));
  auto* stmt = create<ast::VariableDeclStatement>(var);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), R"(float a = initializer;
)");
}

TEST_F(MslGeneratorImplTest, Emit_VariableDeclStatement_Initializer_ZeroVec) {
  auto* zero_vec = vec3<f32>();

  auto* var = Var("a", ty.vec3<f32>(), ast::StorageClass::kFunction, zero_vec);
  auto* stmt = create<ast::VariableDeclStatement>(var);
  WrapInFunction(stmt);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStatement(stmt)) << gen.error();
  EXPECT_EQ(gen.result(), R"(float3 a = float3(0.0f);
)");
}

}  // namespace
}  // namespace msl
}  // namespace writer
}  // namespace tint
