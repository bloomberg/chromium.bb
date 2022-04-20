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

#include "src/tint/ast/call_statement.h"
#include "src/tint/writer/hlsl/test_helper.h"

namespace tint::writer::hlsl {
namespace {

using HlslGeneratorImplTest_Call = TestHelper;

TEST_F(HlslGeneratorImplTest_Call, EmitExpression_Call_WithoutParams) {
  Func("my_func", {}, ty.f32(), {Return(1.23f)});

  auto* call = Call("my_func");
  WrapInFunction(call);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitExpression(out, call)) << gen.error();
  EXPECT_EQ(out.str(), "my_func()");
}

TEST_F(HlslGeneratorImplTest_Call, EmitExpression_Call_WithParams) {
  Func("my_func",
       {
           Param(Sym(), ty.f32()),
           Param(Sym(), ty.f32()),
       },
       ty.f32(), {Return(1.23f)});
  Global("param1", ty.f32(), ast::StorageClass::kPrivate);
  Global("param2", ty.f32(), ast::StorageClass::kPrivate);

  auto* call = Call("my_func", "param1", "param2");
  WrapInFunction(call);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitExpression(out, call)) << gen.error();
  EXPECT_EQ(out.str(), "my_func(param1, param2)");
}

TEST_F(HlslGeneratorImplTest_Call, EmitStatement_Call) {
  Func("my_func",
       {
           Param(Sym(), ty.f32()),
           Param(Sym(), ty.f32()),
       },
       ty.void_(), ast::StatementList{}, ast::AttributeList{});
  Global("param1", ty.f32(), ast::StorageClass::kPrivate);
  Global("param2", ty.f32(), ast::StorageClass::kPrivate);

  auto* call = CallStmt(Call("my_func", "param1", "param2"));
  WrapInFunction(call);

  GeneratorImpl& gen = Build();

  gen.increment_indent();
  ASSERT_TRUE(gen.EmitStatement(call)) << gen.error();
  EXPECT_EQ(gen.result(), "  my_func(param1, param2);\n");
}

}  // namespace
}  // namespace tint::writer::hlsl
