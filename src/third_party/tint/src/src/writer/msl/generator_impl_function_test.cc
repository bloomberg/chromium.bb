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

#include "src/ast/stage_decoration.h"
#include "src/ast/struct_block_decoration.h"
#include "src/ast/variable_decl_statement.h"
#include "src/type/access_control_type.h"
#include "src/writer/msl/test_helper.h"

namespace tint {
namespace writer {
namespace msl {
namespace {

using MslGeneratorImplTest = TestHelper;

TEST_F(MslGeneratorImplTest, Emit_Function) {
  Func("my_func", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{});

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
  void my_func() {
    return;
  }

)");
}

TEST_F(MslGeneratorImplTest, Emit_Function_WithParams) {
  ast::VariableList params;
  params.push_back(Var("a", ty.f32(), ast::StorageClass::kNone));
  params.push_back(Var("b", ty.i32(), ast::StorageClass::kNone));

  Func("my_func", params, ty.void_(),
       ast::StatementList{
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{});

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
  void my_func(float a, int b) {
    return;
  }

)");
}

TEST_F(MslGeneratorImplTest, Emit_Decoration_EntryPoint_NoReturn_Void) {
  Func("main", ast::VariableList{}, ty.void_(),
       ast::StatementList{/* no explicit return */},
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment)});

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
fragment void main() {
  return;
}

)");
}

TEST_F(MslGeneratorImplTest, Emit_Decoration_EntryPoint_WithInOutVars) {
  // fn frag_main([[location(0)]] foo : f32) -> [[location(1)]] f32 {
  //   return foo;
  // }
  auto* foo_in =
      Const("foo", ty.f32(), nullptr, {create<ast::LocationDecoration>(0)});
  Func("frag_main", ast::VariableList{foo_in}, ty.f32(),
       {create<ast::ReturnStatement>(Expr("foo"))},
       {create<ast::StageDecoration>(ast::PipelineStage::kFragment)},
       {create<ast::LocationDecoration>(1)});

  GeneratorImpl& gen = SanitizeAndBuild();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct tint_symbol_3 {
  float foo [[user(locn0)]];
};
struct tint_symbol_5 {
  float value [[color(1)]];
};

fragment tint_symbol_5 frag_main(tint_symbol_3 tint_symbol_1 [[stage_in]]) {
  const float foo = tint_symbol_1.foo;
  return {foo};
}

)");
}

TEST_F(MslGeneratorImplTest, Emit_Decoration_EntryPoint_WithInOut_Builtins) {
  // fn frag_main([[position(0)]] coord : vec4<f32>) -> [[frag_depth]] f32 {
  //   return coord.x;
  // }
  auto* coord_in =
      Const("coord", ty.vec4<f32>(), nullptr,
            {create<ast::BuiltinDecoration>(ast::Builtin::kFragCoord)});
  Func("frag_main", ast::VariableList{coord_in}, ty.f32(),
       {create<ast::ReturnStatement>(MemberAccessor("coord", "x"))},
       {create<ast::StageDecoration>(ast::PipelineStage::kFragment)},
       {create<ast::BuiltinDecoration>(ast::Builtin::kFragDepth)});

  GeneratorImpl& gen = SanitizeAndBuild();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct tint_symbol_3 {
  float4 coord [[position]];
};
struct tint_symbol_5 {
  float value [[depth(any)]];
};

fragment tint_symbol_5 frag_main(tint_symbol_3 tint_symbol_1 [[stage_in]]) {
  const float4 coord = tint_symbol_1.coord;
  return {coord.x};
}

)");
}

TEST_F(MslGeneratorImplTest,
       Emit_Decoration_EntryPoint_SharedStruct_DifferentStages) {
  // struct Interface {
  //   [[location(1)]] col1 : f32;
  //   [[location(2)]] col2 : f32;
  // };
  // fn vert_main() -> Interface {
  //   return Interface(0.4, 0.6);
  // }
  // fn frag_main(colors : Interface) -> void {
  //   const r = colors.col1;
  //   const g = colors.col2;
  // }
  auto* interface_struct = Structure(
      "Interface",
      {Member("col1", ty.f32(), {create<ast::LocationDecoration>(1)}),
       Member("col2", ty.f32(), {create<ast::LocationDecoration>(2)})});

  Func("vert_main", {}, interface_struct,
       {create<ast::ReturnStatement>(
           Construct(interface_struct, Expr(0.5f), Expr(0.25f)))},
       {create<ast::StageDecoration>(ast::PipelineStage::kVertex)});

  Func("frag_main", {Const("colors", interface_struct)}, ty.void_(),
       {
           WrapInStatement(
               Const("r", ty.f32(), MemberAccessor(Expr("colors"), "col1"))),
           WrapInStatement(
               Const("g", ty.f32(), MemberAccessor(Expr("colors"), "col2"))),
       },
       {create<ast::StageDecoration>(ast::PipelineStage::kFragment)});

  GeneratorImpl& gen = SanitizeAndBuild();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct Interface {
  float col1;
  float col2;
};
struct tint_symbol_4 {
  float col1 [[user(locn1)]];
  float col2 [[user(locn2)]];
};
struct tint_symbol_9 {
  float col1 [[user(locn1)]];
  float col2 [[user(locn2)]];
};

vertex tint_symbol_4 vert_main() {
  const Interface tint_symbol_5 = {0.5f, 0.25f};
  return {tint_symbol_5.col1, tint_symbol_5.col2};
}

fragment void frag_main(tint_symbol_9 tint_symbol_7 [[stage_in]]) {
  const Interface colors = {tint_symbol_7.col1, tint_symbol_7.col2};
  const float r = colors.col1;
  const float g = colors.col2;
  return;
}

)");
}

TEST_F(MslGeneratorImplTest,
       Emit_Decoration_EntryPoint_SharedStruct_HelperFunction) {
  // struct VertexOutput {
  //   [[builtin(position)]] pos : vec4<f32>;
  // };
  // fn foo(x : f32) -> VertexOutput {
  //   return VertexOutput(vec4<f32>(x, x, x, 1.0));
  // }
  // fn vert_main1() -> VertexOutput {
  //   return foo(0.5);
  // }
  // fn vert_main2() -> VertexOutput {
  //   return foo(0.25);
  // }
  auto* vertex_output_struct = Structure(
      "VertexOutput",
      {Member("pos", ty.vec4<f32>(),
              {create<ast::BuiltinDecoration>(ast::Builtin::kPosition)})});

  Func("foo", {Const("x", ty.f32())}, vertex_output_struct,
       {create<ast::ReturnStatement>(Construct(
           vertex_output_struct, Construct(ty.vec4<f32>(), Expr("x"), Expr("x"),
                                           Expr("x"), Expr(1.f))))},
       {});

  Func("vert_main1", {}, vertex_output_struct,
       {create<ast::ReturnStatement>(
           Construct(vertex_output_struct, Expr(Call("foo", Expr(0.5f)))))},
       {create<ast::StageDecoration>(ast::PipelineStage::kVertex)});

  Func("vert_main2", {}, vertex_output_struct,
       {create<ast::ReturnStatement>(
           Construct(vertex_output_struct, Expr(Call("foo", Expr(0.25f)))))},
       {create<ast::StageDecoration>(ast::PipelineStage::kVertex)});

  GeneratorImpl& gen = SanitizeAndBuild();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct VertexOutput {
  float4 pos;
};
struct tint_symbol_3 {
  float4 pos [[position]];
};
struct tint_symbol_7 {
  float4 pos [[position]];
};

VertexOutput foo(float x) {
  return {float4(x, x, x, 1.0f)};
}

vertex tint_symbol_3 vert_main1() {
  const VertexOutput tint_symbol_5 = {foo(0.5f)};
  return {tint_symbol_5.pos};
}

vertex tint_symbol_7 vert_main2() {
  const VertexOutput tint_symbol_8 = {foo(0.25f)};
  return {tint_symbol_8.pos};
}

)");
}

TEST_F(MslGeneratorImplTest,
       Emit_FunctionDecoration_EntryPoint_With_RW_StorageBuffer) {
  auto* s = Structure("Data", {
                                  Member("a", ty.i32()),
                                  Member("b", ty.f32()),
                              });

  type::AccessControl ac(ast::AccessControl::kReadWrite, s);

  Global("coord", &ac, ast::StorageClass::kStorage, nullptr,
         ast::DecorationList{create<ast::BindingDecoration>(0),
                             create<ast::GroupDecoration>(1)});

  auto* var = Var("v", ty.f32(), ast::StorageClass::kFunction,
                  MemberAccessor("coord", "b"));

  Func("frag_main", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(var),
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct Data {
  /* 0x0000 */ int a;
  /* 0x0004 */ float b;
};

fragment void frag_main(device Data& coord [[buffer(0)]]) {
  float v = coord.b;
  return;
}

)");
}

TEST_F(MslGeneratorImplTest,
       Emit_FunctionDecoration_EntryPoint_With_RO_StorageBuffer) {
  auto* s = Structure("Data", {
                                  Member("a", ty.i32()),
                                  Member("b", ty.f32()),
                              });

  type::AccessControl ac(ast::AccessControl::kReadOnly, s);

  Global("coord", &ac, ast::StorageClass::kStorage, nullptr,
         ast::DecorationList{create<ast::BindingDecoration>(0),
                             create<ast::GroupDecoration>(1)});

  auto* var = Var("v", ty.f32(), ast::StorageClass::kFunction,
                  MemberAccessor("coord", "b"));

  Func("frag_main", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(var),
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct Data {
  /* 0x0000 */ int a;
  /* 0x0004 */ float b;
};

fragment void frag_main(const device Data& coord [[buffer(0)]]) {
  float v = coord.b;
  return;
}

)");
}

// TODO(crbug.com/tint/697): Remove this test
TEST_F(
    MslGeneratorImplTest,
    Emit_Decoration_Called_By_EntryPoints_WithLocationGlobals_And_Params) {  // NOLINT
  Global("foo", ty.f32(), ast::StorageClass::kInput, nullptr,
         ast::DecorationList{create<ast::LocationDecoration>(0)});

  Global("bar", ty.f32(), ast::StorageClass::kOutput, nullptr,
         ast::DecorationList{create<ast::LocationDecoration>(1)});

  Global("val", ty.f32(), ast::StorageClass::kOutput, nullptr,
         ast::DecorationList{create<ast::LocationDecoration>(0)});

  ast::VariableList params;
  params.push_back(Var("param", ty.f32(), ast::StorageClass::kNone));

  auto body = ast::StatementList{
      create<ast::AssignmentStatement>(Expr("bar"), Expr("foo")),
      create<ast::AssignmentStatement>(Expr("val"), Expr("param")),
      create<ast::ReturnStatement>(Expr("foo"))};

  Func("sub_func", params, ty.f32(), body, ast::DecorationList{});

  body = ast::StatementList{
      create<ast::AssignmentStatement>(Expr("bar"), Call("sub_func", 1.0f)),
      create<ast::ReturnStatement>(),
  };

  Func("ep_1", ast::VariableList{}, ty.void_(), body,
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct ep_1_in {
  float foo [[user(locn0)]];
};

struct ep_1_out {
  float bar [[color(1)]];
  float val [[color(0)]];
};

float sub_func_ep_1(thread ep_1_in& _tint_in, thread ep_1_out& _tint_out, float param) {
  _tint_out.bar = _tint_in.foo;
  _tint_out.val = param;
  return _tint_in.foo;
}

fragment ep_1_out ep_1(ep_1_in _tint_in [[stage_in]]) {
  ep_1_out _tint_out = {};
  _tint_out.bar = sub_func_ep_1(_tint_in, _tint_out, 1.0f);
  return _tint_out;
}

)");
}

// TODO(crbug.com/tint/697): Remove this test
TEST_F(MslGeneratorImplTest,
       Emit_Decoration_Called_By_EntryPoints_NoUsedGlobals) {
  Global("depth", ty.f32(), ast::StorageClass::kOutput, nullptr,
         ast::DecorationList{
             create<ast::BuiltinDecoration>(ast::Builtin::kFragDepth)});

  ast::VariableList params;
  params.push_back(Var("param", ty.f32(), ast::StorageClass::kFunction));

  Func("sub_func", params, ty.f32(),
       ast::StatementList{
           create<ast::ReturnStatement>(Expr("param")),
       },
       ast::DecorationList{});

  auto body = ast::StatementList{
      create<ast::AssignmentStatement>(Expr("depth"), Call("sub_func", 1.0f)),
      create<ast::ReturnStatement>(),
  };

  Func("ep_1", ast::VariableList{}, ty.void_(), body,
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct ep_1_out {
  float depth [[depth(any)]];
};

float sub_func(float param) {
  return param;
}

fragment ep_1_out ep_1() {
  ep_1_out _tint_out = {};
  _tint_out.depth = sub_func(1.0f);
  return _tint_out;
}

)");
}

// TODO(crbug.com/tint/697): Remove this test
TEST_F(
    MslGeneratorImplTest,
    Emit_Decoration_Called_By_EntryPoints_WithBuiltinGlobals_And_Params) {  // NOLINT
  Global("coord", ty.vec4<f32>(), ast::StorageClass::kInput, nullptr,
         ast::DecorationList{
             create<ast::BuiltinDecoration>(ast::Builtin::kFragCoord)});

  Global("depth", ty.f32(), ast::StorageClass::kOutput, nullptr,
         ast::DecorationList{
             create<ast::BuiltinDecoration>(ast::Builtin::kFragDepth)});

  ast::VariableList params;
  params.push_back(Var("param", ty.f32(), ast::StorageClass::kNone));

  auto body = ast::StatementList{
      create<ast::AssignmentStatement>(Expr("depth"),
                                       MemberAccessor("coord", "x")),
      create<ast::ReturnStatement>(Expr("param")),
  };

  Func("sub_func", params, ty.f32(), body, ast::DecorationList{});

  body = ast::StatementList{
      create<ast::AssignmentStatement>(Expr("depth"), Call("sub_func", 1.0f)),
      create<ast::ReturnStatement>(),
  };

  Func("ep_1", ast::VariableList{}, ty.void_(), body,
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct ep_1_out {
  float depth [[depth(any)]];
};

float sub_func_ep_1(thread ep_1_out& _tint_out, thread float4& coord, float param) {
  _tint_out.depth = coord.x;
  return param;
}

fragment ep_1_out ep_1(float4 coord [[position]]) {
  ep_1_out _tint_out = {};
  _tint_out.depth = sub_func_ep_1(_tint_out, coord, 1.0f);
  return _tint_out;
}

)");
}

TEST_F(MslGeneratorImplTest,
       Emit_Decoration_Called_By_EntryPoint_With_Uniform) {
  Global("coord", ty.vec4<f32>(), ast::StorageClass::kUniform, nullptr,
         ast::DecorationList{create<ast::BindingDecoration>(0),
                             create<ast::GroupDecoration>(1)});

  ast::VariableList params;
  params.push_back(Var("param", ty.f32(), ast::StorageClass::kFunction));

  auto body = ast::StatementList{
      create<ast::ReturnStatement>(MemberAccessor("coord", "x")),
  };

  Func("sub_func", params, ty.f32(), body, ast::DecorationList{});

  ast::ExpressionList expr;
  expr.push_back(Expr(1.0f));

  auto* var =
      Var("v", ty.f32(), ast::StorageClass::kFunction, Call("sub_func", 1.0f));

  Func("frag_main", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(var),
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
float sub_func(constant float4& coord, float param) {
  return coord.x;
}

fragment void frag_main(constant float4& coord [[buffer(0)]]) {
  float v = sub_func(coord, 1.0f);
  return;
}

)");
}

TEST_F(MslGeneratorImplTest,
       Emit_FunctionDecoration_Called_By_EntryPoint_With_RW_StorageBuffer) {
  auto* s = Structure("Data", {
                                  Member("a", ty.i32()),
                                  Member("b", ty.f32()),
                              });

  type::AccessControl ac(ast::AccessControl::kReadWrite, s);

  Global("coord", &ac, ast::StorageClass::kStorage, nullptr,
         ast::DecorationList{create<ast::BindingDecoration>(0),
                             create<ast::GroupDecoration>(1)});

  ast::VariableList params;
  params.push_back(Var("param", ty.f32(), ast::StorageClass::kFunction));

  auto body = ast::StatementList{
      create<ast::ReturnStatement>(MemberAccessor("coord", "b"))};

  Func("sub_func", params, ty.f32(), body, ast::DecorationList{});

  auto* var =
      Var("v", ty.f32(), ast::StorageClass::kFunction, Call("sub_func", 1.0f));

  Func("frag_main", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(var),
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct Data {
  /* 0x0000 */ int a;
  /* 0x0004 */ float b;
};

float sub_func(device Data& coord, float param) {
  return coord.b;
}

fragment void frag_main(device Data& coord [[buffer(0)]]) {
  float v = sub_func(coord, 1.0f);
  return;
}

)");
}

TEST_F(MslGeneratorImplTest,
       Emit_FunctionDecoration_Called_By_EntryPoint_With_RO_StorageBuffer) {
  auto* s = Structure("Data", {
                                  Member("a", ty.i32()),
                                  Member("b", ty.f32()),
                              });

  type::AccessControl ac(ast::AccessControl::kReadOnly, s);

  Global("coord", &ac, ast::StorageClass::kStorage, nullptr,
         ast::DecorationList{create<ast::BindingDecoration>(0),
                             create<ast::GroupDecoration>(1)});

  ast::VariableList params;
  params.push_back(Var("param", ty.f32(), ast::StorageClass::kFunction));

  auto body = ast::StatementList{
      create<ast::ReturnStatement>(MemberAccessor("coord", "b"))};

  Func("sub_func", params, ty.f32(), body, ast::DecorationList{});

  ast::ExpressionList expr;
  expr.push_back(Expr(1.0f));

  auto* var =
      Var("v", ty.f32(), ast::StorageClass::kFunction, Call("sub_func", 1.0f));

  Func("frag_main", ast::VariableList{}, ty.void_(),
       ast::StatementList{
           create<ast::VariableDeclStatement>(var),
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct Data {
  /* 0x0000 */ int a;
  /* 0x0004 */ float b;
};

float sub_func(const device Data& coord, float param) {
  return coord.b;
}

fragment void frag_main(const device Data& coord [[buffer(0)]]) {
  float v = sub_func(coord, 1.0f);
  return;
}

)");
}

// TODO(crbug.com/tint/697): Remove this test
TEST_F(MslGeneratorImplTest,
       Emit_Decoration_EntryPoints_WithGlobal_Nested_Return) {
  Global("bar", ty.f32(), ast::StorageClass::kOutput, nullptr,
         ast::DecorationList{create<ast::LocationDecoration>(1)});

  auto* list = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });

  auto body = ast::StatementList{
      create<ast::AssignmentStatement>(Expr("bar"), Expr(1.f)),
      create<ast::IfStatement>(create<ast::BinaryExpression>(
                                   ast::BinaryOp::kEqual, Expr(1), Expr(1)),
                               list, ast::ElseStatementList{}),
      create<ast::ReturnStatement>(),
  };

  Func("ep_1", ast::VariableList{}, ty.void_(), body,
       ast::DecorationList{
           create<ast::StageDecoration>(ast::PipelineStage::kFragment),
       });

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct ep_1_out {
  float bar [[color(1)]];
};

fragment ep_1_out ep_1() {
  ep_1_out _tint_out = {};
  _tint_out.bar = 1.0f;
  if ((1 == 1)) {
    return _tint_out;
  }
  return _tint_out;
}

)");
}

TEST_F(MslGeneratorImplTest, Emit_Function_WithArrayParams) {
  ast::VariableList params;
  params.push_back(Var("a", ty.array<f32, 5>(), ast::StorageClass::kNone));

  Func("my_func", params, ty.void_(),
       ast::StatementList{
           create<ast::ReturnStatement>(),
       },
       ast::DecorationList{});

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
  void my_func(float a[5]) {
    return;
  }

)");
}

// https://crbug.com/tint/297
TEST_F(MslGeneratorImplTest,
       Emit_Function_Multiple_EntryPoint_With_Same_ModuleVar) {
  // [[block]] struct Data {
  //   d : f32;
  // };
  // [[binding(0), group(0)]] var<storage> data : Data;
  //
  // [[stage(compute)]]
  // fn a() -> void {
  //   return;
  // }
  //
  // [[stage(compute)]]
  // fn b() -> void {
  //   return;
  // }

  auto* s = Structure("Data", {Member("d", ty.f32())},
                      {create<ast::StructBlockDecoration>()});

  type::AccessControl ac(ast::AccessControl::kReadWrite, s);

  Global("data", &ac, ast::StorageClass::kStorage, nullptr,
         ast::DecorationList{create<ast::BindingDecoration>(0),
                             create<ast::GroupDecoration>(0)});

  {
    auto* var = Var("v", ty.f32(), ast::StorageClass::kFunction,
                    MemberAccessor("data", "d"));

    Func("a", ast::VariableList{}, ty.void_(),
         ast::StatementList{
             create<ast::VariableDeclStatement>(var),
             create<ast::ReturnStatement>(),
         },
         ast::DecorationList{
             create<ast::StageDecoration>(ast::PipelineStage::kCompute),
         });
  }

  {
    auto* var = Var("v", ty.f32(), ast::StorageClass::kFunction,
                    MemberAccessor("data", "d"));

    Func("b", ast::VariableList{}, ty.void_(),
         ast::StatementList{create<ast::VariableDeclStatement>(var),
                            create<ast::ReturnStatement>()},
         ast::DecorationList{
             create<ast::StageDecoration>(ast::PipelineStage::kCompute)});
  }

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.Generate()) << gen.error();
  EXPECT_EQ(gen.result(), R"(#include <metal_stdlib>

using namespace metal;
struct Data {
  /* 0x0000 */ float d;
};

kernel void a(device Data& data [[buffer(0)]]) {
  float v = data.d;
  return;
}

kernel void b(device Data& data [[buffer(0)]]) {
  float v = data.d;
  return;
}

)");
}

}  // namespace
}  // namespace msl
}  // namespace writer
}  // namespace tint
