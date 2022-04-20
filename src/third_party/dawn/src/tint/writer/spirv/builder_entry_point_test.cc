// Copyright 2021 The Tint Authors.
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

#include <memory>

#include "gtest/gtest.h"
#include "src/tint/ast/builtin.h"
#include "src/tint/ast/builtin_attribute.h"
#include "src/tint/ast/location_attribute.h"
#include "src/tint/ast/return_statement.h"
#include "src/tint/ast/stage_attribute.h"
#include "src/tint/ast/storage_class.h"
#include "src/tint/ast/variable.h"
#include "src/tint/program.h"
#include "src/tint/sem/f32_type.h"
#include "src/tint/sem/vector_type.h"
#include "src/tint/writer/spirv/builder.h"
#include "src/tint/writer/spirv/spv_dump.h"
#include "src/tint/writer/spirv/test_helper.h"

namespace tint::writer::spirv {
namespace {

using BuilderTest = TestHelper;

TEST_F(BuilderTest, EntryPoint_Parameters) {
  // @stage(fragment)
  // fn frag_main(@builtin(position) coord : vec4<f32>,
  //              @location(1) loc1 : f32) {
  //   var col : f32 = (coord.x * loc1);
  // }
  auto* coord =
      Param("coord", ty.vec4<f32>(), {Builtin(ast::Builtin::kPosition)});
  auto* loc1 = Param("loc1", ty.f32(), {Location(1u)});
  auto* mul = Mul(Expr(MemberAccessor("coord", "x")), Expr("loc1"));
  auto* col = Var("col", ty.f32(), ast::StorageClass::kNone, mul);
  Func("frag_main", ast::VariableList{coord, loc1}, ty.void_(),
       ast::StatementList{WrapInStatement(col)},
       ast::AttributeList{
           Stage(ast::PipelineStage::kFragment),
       });

  spirv::Builder& b = SanitizeAndBuild();

  ASSERT_TRUE(b.Build());

  // Test that "coord" and "loc1" get hoisted out to global variables with the
  // Input storage class, retaining their decorations.
  EXPECT_EQ(DumpBuilder(b), R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %19 "frag_main" %1 %5
OpExecutionMode %19 OriginUpperLeft
OpName %1 "coord_1"
OpName %5 "loc1_1"
OpName %9 "frag_main_inner"
OpName %10 "coord"
OpName %11 "loc1"
OpName %15 "col"
OpName %19 "frag_main"
OpDecorate %1 BuiltIn FragCoord
OpDecorate %5 Location 1
%4 = OpTypeFloat 32
%3 = OpTypeVector %4 4
%2 = OpTypePointer Input %3
%1 = OpVariable %2 Input
%6 = OpTypePointer Input %4
%5 = OpVariable %6 Input
%8 = OpTypeVoid
%7 = OpTypeFunction %8 %3 %4
%16 = OpTypePointer Function %4
%17 = OpConstantNull %4
%18 = OpTypeFunction %8
%9 = OpFunction %8 None %7
%10 = OpFunctionParameter %3
%11 = OpFunctionParameter %4
%12 = OpLabel
%15 = OpVariable %16 Function %17
%13 = OpCompositeExtract %4 %10 0
%14 = OpFMul %4 %13 %11
OpStore %15 %14
OpReturn
OpFunctionEnd
%19 = OpFunction %8 None %18
%20 = OpLabel
%22 = OpLoad %3 %1
%23 = OpLoad %4 %5
%21 = OpFunctionCall %8 %9 %22 %23
OpReturn
OpFunctionEnd
)");

  Validate(b);
}

TEST_F(BuilderTest, EntryPoint_ReturnValue) {
  // @stage(fragment)
  // fn frag_main(@location(0) @interpolate(flat) loc_in : u32)
  //     -> @location(0) f32 {
  //   if (loc_in > 10) {
  //     return 0.5;
  //   }
  //   return 1.0;
  // }
  auto* loc_in = Param("loc_in", ty.u32(), {Location(0), Flat()});
  auto* cond = create<ast::BinaryExpression>(ast::BinaryOp::kGreaterThan,
                                             Expr("loc_in"), Expr(10u));
  Func("frag_main", ast::VariableList{loc_in}, ty.f32(),
       ast::StatementList{
           If(cond, Block(Return(0.5f))),
           Return(1.0f),
       },
       ast::AttributeList{
           Stage(ast::PipelineStage::kFragment),
       },
       ast::AttributeList{Location(0)});

  spirv::Builder& b = SanitizeAndBuild();

  ASSERT_TRUE(b.Build());

  // Test that the return value gets hoisted out to a global variable with the
  // Output storage class, and the return statements are replaced with stores.
  EXPECT_EQ(DumpBuilder(b), R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Fragment %21 "frag_main" %1 %4
OpExecutionMode %21 OriginUpperLeft
OpName %1 "loc_in_1"
OpName %4 "value"
OpName %9 "frag_main_inner"
OpName %10 "loc_in"
OpName %21 "frag_main"
OpDecorate %1 Location 0
OpDecorate %1 Flat
OpDecorate %4 Location 0
%3 = OpTypeInt 32 0
%2 = OpTypePointer Input %3
%1 = OpVariable %2 Input
%6 = OpTypeFloat 32
%5 = OpTypePointer Output %6
%7 = OpConstantNull %6
%4 = OpVariable %5 Output %7
%8 = OpTypeFunction %6 %3
%12 = OpConstant %3 10
%14 = OpTypeBool
%17 = OpConstant %6 0.5
%18 = OpConstant %6 1
%20 = OpTypeVoid
%19 = OpTypeFunction %20
%9 = OpFunction %6 None %8
%10 = OpFunctionParameter %3
%11 = OpLabel
%13 = OpUGreaterThan %14 %10 %12
OpSelectionMerge %15 None
OpBranchConditional %13 %16 %15
%16 = OpLabel
OpReturnValue %17
%15 = OpLabel
OpReturnValue %18
OpFunctionEnd
%21 = OpFunction %20 None %19
%22 = OpLabel
%24 = OpLoad %3 %1
%23 = OpFunctionCall %6 %9 %24
OpStore %4 %23
OpReturn
OpFunctionEnd
)");

  Validate(b);
}

TEST_F(BuilderTest, EntryPoint_SharedStruct) {
  // struct Interface {
  //   @location(1) value : f32;
  //   @builtin(position) pos : vec4<f32>;
  // };
  //
  // @stage(vertex)
  // fn vert_main() -> Interface {
  //   return Interface(42.0, vec4<f32>());
  // }
  //
  // @stage(fragment)
  // fn frag_main(inputs : Interface) -> @builtin(frag_depth) f32 {
  //   return inputs.value;
  // }

  auto* interface = Structure(
      "Interface",
      {
          Member("value", ty.f32(), ast::AttributeList{Location(1u)}),
          Member("pos", ty.vec4<f32>(),
                 ast::AttributeList{Builtin(ast::Builtin::kPosition)}),
      });

  auto* vert_retval =
      Construct(ty.Of(interface), 42.f, Construct(ty.vec4<f32>()));
  Func("vert_main", ast::VariableList{}, ty.Of(interface),
       {Return(vert_retval)}, {Stage(ast::PipelineStage::kVertex)});

  auto* frag_inputs = Param("inputs", ty.Of(interface));
  Func("frag_main", ast::VariableList{frag_inputs}, ty.f32(),
       {Return(MemberAccessor(Expr("inputs"), "value"))},
       {Stage(ast::PipelineStage::kFragment)},
       {Builtin(ast::Builtin::kFragDepth)});

  spirv::Builder& b = SanitizeAndBuild();

  ASSERT_TRUE(b.Build()) << b.error();

  EXPECT_EQ(DumpBuilder(b), R"(OpCapability Shader
OpMemoryModel Logical GLSL450
OpEntryPoint Vertex %23 "vert_main" %1 %5 %9
OpEntryPoint Fragment %34 "frag_main" %10 %12 %14
OpExecutionMode %34 OriginUpperLeft
OpExecutionMode %34 DepthReplacing
OpName %1 "value_1"
OpName %5 "pos_1"
OpName %9 "vertex_point_size"
OpName %10 "value_2"
OpName %12 "pos_2"
OpName %14 "value_3"
OpName %16 "Interface"
OpMemberName %16 0 "value"
OpMemberName %16 1 "pos"
OpName %17 "vert_main_inner"
OpName %23 "vert_main"
OpName %30 "frag_main_inner"
OpName %31 "inputs"
OpName %34 "frag_main"
OpDecorate %1 Location 1
OpDecorate %5 BuiltIn Position
OpDecorate %9 BuiltIn PointSize
OpDecorate %10 Location 1
OpDecorate %12 BuiltIn FragCoord
OpDecorate %14 BuiltIn FragDepth
OpMemberDecorate %16 0 Offset 0
OpMemberDecorate %16 1 Offset 16
%3 = OpTypeFloat 32
%2 = OpTypePointer Output %3
%4 = OpConstantNull %3
%1 = OpVariable %2 Output %4
%7 = OpTypeVector %3 4
%6 = OpTypePointer Output %7
%8 = OpConstantNull %7
%5 = OpVariable %6 Output %8
%9 = OpVariable %2 Output %4
%11 = OpTypePointer Input %3
%10 = OpVariable %11 Input
%13 = OpTypePointer Input %7
%12 = OpVariable %13 Input
%14 = OpVariable %2 Output %4
%16 = OpTypeStruct %3 %7
%15 = OpTypeFunction %16
%19 = OpConstant %3 42
%20 = OpConstantComposite %16 %19 %8
%22 = OpTypeVoid
%21 = OpTypeFunction %22
%28 = OpConstant %3 1
%29 = OpTypeFunction %3 %16
%17 = OpFunction %16 None %15
%18 = OpLabel
OpReturnValue %20
OpFunctionEnd
%23 = OpFunction %22 None %21
%24 = OpLabel
%25 = OpFunctionCall %16 %17
%26 = OpCompositeExtract %3 %25 0
OpStore %1 %26
%27 = OpCompositeExtract %7 %25 1
OpStore %5 %27
OpStore %9 %28
OpReturn
OpFunctionEnd
%30 = OpFunction %3 None %29
%31 = OpFunctionParameter %16
%32 = OpLabel
%33 = OpCompositeExtract %3 %31 0
OpReturnValue %33
OpFunctionEnd
%34 = OpFunction %22 None %21
%35 = OpLabel
%37 = OpLoad %3 %10
%38 = OpLoad %7 %12
%39 = OpCompositeConstruct %16 %37 %38
%36 = OpFunctionCall %3 %30 %39
OpStore %14 %36
OpReturn
OpFunctionEnd
)");

  Validate(b);
}

TEST_F(BuilderTest, SampleIndex_SampleRateShadingCapability) {
  Func("main",
       {Param("sample_index", ty.u32(), {Builtin(ast::Builtin::kSampleIndex)})},
       ty.void_(), {}, {Stage(ast::PipelineStage::kFragment)});

  spirv::Builder& b = SanitizeAndBuild();

  ASSERT_TRUE(b.Build()) << b.error();

  // Make sure we generate the SampleRateShading capability.
  EXPECT_EQ(DumpInstructions(b.capabilities()),
            "OpCapability Shader\n"
            "OpCapability SampleRateShading\n");
  EXPECT_EQ(DumpInstructions(b.annots()), "OpDecorate %1 BuiltIn SampleId\n");
}

}  // namespace
}  // namespace tint::writer::spirv
