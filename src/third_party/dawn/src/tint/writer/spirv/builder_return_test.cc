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

#include "src/tint/writer/spirv/spv_dump.h"
#include "src/tint/writer/spirv/test_helper.h"

namespace tint::writer::spirv {
namespace {

using BuilderTest = TestHelper;

TEST_F(BuilderTest, Return) {
  auto* ret = Return();
  WrapInFunction(ret);

  spirv::Builder& b = Build();

  b.push_function(Function{});
  EXPECT_TRUE(b.GenerateReturnStatement(ret));
  ASSERT_FALSE(b.has_error()) << b.error();

  EXPECT_EQ(DumpInstructions(b.functions()[0].instructions()), R"(OpReturn
)");
}

TEST_F(BuilderTest, Return_WithValue) {
  auto* val = vec3<f32>(1.f, 1.f, 3.f);

  auto* ret = Return(val);
  Func("test", {}, ty.vec3<f32>(), {ret}, {});

  spirv::Builder& b = Build();

  b.push_function(Function{});
  EXPECT_TRUE(b.GenerateReturnStatement(ret));
  ASSERT_FALSE(b.has_error()) << b.error();

  EXPECT_EQ(DumpInstructions(b.types()), R"(%2 = OpTypeFloat 32
%1 = OpTypeVector %2 3
%3 = OpConstant %2 1
%4 = OpConstant %2 3
%5 = OpConstantComposite %1 %3 %3 %4
)");
  EXPECT_EQ(DumpInstructions(b.functions()[0].instructions()),
            R"(OpReturnValue %5
)");
}

TEST_F(BuilderTest, Return_WithValue_GeneratesLoad) {
  auto* var = Var("param", ty.f32());

  auto* ret = Return(var);
  Func("test", {}, ty.f32(), {Decl(var), ret}, {});

  spirv::Builder& b = Build();

  b.push_function(Function{});
  EXPECT_TRUE(b.GenerateFunctionVariable(var)) << b.error();
  EXPECT_TRUE(b.GenerateReturnStatement(ret)) << b.error();
  ASSERT_FALSE(b.has_error()) << b.error();

  EXPECT_EQ(DumpInstructions(b.types()), R"(%3 = OpTypeFloat 32
%2 = OpTypePointer Function %3
%4 = OpConstantNull %3
)");
  EXPECT_EQ(DumpInstructions(b.functions()[0].variables()),
            R"(%1 = OpVariable %2 Function %4
)");
  EXPECT_EQ(DumpInstructions(b.functions()[0].instructions()),
            R"(%5 = OpLoad %3 %1
OpReturnValue %5
)");
}

}  // namespace
}  // namespace tint::writer::spirv
