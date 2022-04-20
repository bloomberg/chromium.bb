
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
#include "src/tint/writer/spirv/spv_dump.h"
#include "src/tint/writer/spirv/test_helper.h"

namespace tint::writer::spirv {
namespace {

using BuilderTest = TestHelper;

TEST_F(BuilderTest, Expression_Call) {
  ast::VariableList func_params;
  func_params.push_back(Param("a", ty.f32()));
  func_params.push_back(Param("b", ty.f32()));

  auto* a_func = Func("a_func", func_params, ty.f32(), {Return(Add("a", "b"))});
  auto* func =
      Func("main", {}, ty.void_(), {Assign(Phony(), Call("a_func", 1.f, 1.f))});

  spirv::Builder& b = Build();

  ASSERT_TRUE(b.GenerateFunction(a_func)) << b.error();
  ASSERT_TRUE(b.GenerateFunction(func)) << b.error();

  EXPECT_EQ(DumpBuilder(b), R"(OpName %3 "a_func"
OpName %4 "a"
OpName %5 "b"
OpName %10 "main"
%2 = OpTypeFloat 32
%1 = OpTypeFunction %2 %2 %2
%9 = OpTypeVoid
%8 = OpTypeFunction %9
%13 = OpConstant %2 1
%3 = OpFunction %2 None %1
%4 = OpFunctionParameter %2
%5 = OpFunctionParameter %2
%6 = OpLabel
%7 = OpFAdd %2 %4 %5
OpReturnValue %7
OpFunctionEnd
%10 = OpFunction %9 None %8
%11 = OpLabel
%12 = OpFunctionCall %2 %3 %13 %13
OpReturn
OpFunctionEnd
)");
}

TEST_F(BuilderTest, Statement_Call) {
  ast::VariableList func_params;
  func_params.push_back(Param("a", ty.f32()));
  func_params.push_back(Param("b", ty.f32()));

  auto* a_func = Func("a_func", func_params, ty.f32(), {Return(Add("a", "b"))});

  auto* func =
      Func("main", {}, ty.void_(), {CallStmt(Call("a_func", 1.f, 1.f))});

  spirv::Builder& b = Build();

  ASSERT_TRUE(b.GenerateFunction(a_func)) << b.error();
  ASSERT_TRUE(b.GenerateFunction(func)) << b.error();

  EXPECT_EQ(DumpBuilder(b), R"(OpName %3 "a_func"
OpName %4 "a"
OpName %5 "b"
OpName %10 "main"
%2 = OpTypeFloat 32
%1 = OpTypeFunction %2 %2 %2
%9 = OpTypeVoid
%8 = OpTypeFunction %9
%13 = OpConstant %2 1
%3 = OpFunction %2 None %1
%4 = OpFunctionParameter %2
%5 = OpFunctionParameter %2
%6 = OpLabel
%7 = OpFAdd %2 %4 %5
OpReturnValue %7
OpFunctionEnd
%10 = OpFunction %9 None %8
%11 = OpLabel
%12 = OpFunctionCall %2 %3 %13 %13
OpReturn
OpFunctionEnd
)");
}

}  // namespace
}  // namespace tint::writer::spirv
