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

#include "gtest/gtest.h"
#include "src/ast/decorated_variable.h"
#include "src/ast/discard_statement.h"
#include "src/ast/function.h"
#include "src/ast/member_accessor_expression.h"
#include "src/ast/module.h"
#include "src/ast/pipeline_stage.h"
#include "src/ast/return_statement.h"
#include "src/ast/stage_decoration.h"
#include "src/ast/struct_block_decoration.h"
#include "src/ast/struct_member_offset_decoration.h"
#include "src/ast/type/access_control_type.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/void_type.h"
#include "src/ast/variable.h"
#include "src/ast/variable_decl_statement.h"
#include "src/ast/workgroup_decoration.h"
#include "src/context.h"
#include "src/type_determiner.h"
#include "src/writer/wgsl/generator_impl.h"

namespace tint {
namespace writer {
namespace wgsl {
namespace {

using WgslGeneratorImplTest = testing::Test;

TEST_F(WgslGeneratorImplTest, Emit_Function) {
  auto body = std::make_unique<ast::BlockStatement>();
  body->append(std::make_unique<ast::DiscardStatement>());
  body->append(std::make_unique<ast::ReturnStatement>());

  ast::type::VoidType void_type;
  ast::Function func("my_func", {}, &void_type);
  func.set_body(std::move(body));

  GeneratorImpl g;
  g.increment_indent();

  ASSERT_TRUE(g.EmitFunction(&func));
  EXPECT_EQ(g.result(), R"(  fn my_func() -> void {
    discard;
    return;
  }
)");
}

TEST_F(WgslGeneratorImplTest, Emit_Function_WithParams) {
  auto body = std::make_unique<ast::BlockStatement>();
  body->append(std::make_unique<ast::DiscardStatement>());
  body->append(std::make_unique<ast::ReturnStatement>());

  ast::type::F32Type f32;
  ast::type::I32Type i32;
  ast::VariableList params;
  params.push_back(
      std::make_unique<ast::Variable>("a", ast::StorageClass::kNone, &f32));
  params.push_back(
      std::make_unique<ast::Variable>("b", ast::StorageClass::kNone, &i32));

  ast::type::VoidType void_type;
  ast::Function func("my_func", std::move(params), &void_type);
  func.set_body(std::move(body));

  GeneratorImpl g;
  g.increment_indent();

  ASSERT_TRUE(g.EmitFunction(&func));
  EXPECT_EQ(g.result(), R"(  fn my_func(a : f32, b : i32) -> void {
    discard;
    return;
  }
)");
}

TEST_F(WgslGeneratorImplTest, Emit_Function_WithDecoration_WorkgroupSize) {
  auto body = std::make_unique<ast::BlockStatement>();
  body->append(std::make_unique<ast::DiscardStatement>());
  body->append(std::make_unique<ast::ReturnStatement>());

  ast::type::VoidType void_type;
  ast::Function func("my_func", {}, &void_type);
  func.add_decoration(
      std::make_unique<ast::WorkgroupDecoration>(2u, 4u, 6u, Source{}));
  func.set_body(std::move(body));

  GeneratorImpl g;
  g.increment_indent();

  ASSERT_TRUE(g.EmitFunction(&func));
  EXPECT_EQ(g.result(), R"(  [[workgroup_size(2, 4, 6)]]
  fn my_func() -> void {
    discard;
    return;
  }
)");
}

TEST_F(WgslGeneratorImplTest, Emit_Function_WithDecoration_Stage) {
  auto body = std::make_unique<ast::BlockStatement>();
  body->append(std::make_unique<ast::DiscardStatement>());
  body->append(std::make_unique<ast::ReturnStatement>());

  ast::type::VoidType void_type;
  ast::Function func("my_func", {}, &void_type);
  func.add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kFragment, Source{}));
  func.set_body(std::move(body));

  GeneratorImpl g;
  g.increment_indent();

  ASSERT_TRUE(g.EmitFunction(&func));
  EXPECT_EQ(g.result(), R"(  [[stage(fragment)]]
  fn my_func() -> void {
    discard;
    return;
  }
)");
}

TEST_F(WgslGeneratorImplTest, Emit_Function_WithDecoration_Multiple) {
  auto body = std::make_unique<ast::BlockStatement>();
  body->append(std::make_unique<ast::DiscardStatement>());
  body->append(std::make_unique<ast::ReturnStatement>());

  ast::type::VoidType void_type;
  ast::Function func("my_func", {}, &void_type);
  func.add_decoration(std::make_unique<ast::StageDecoration>(
      ast::PipelineStage::kFragment, Source{}));
  func.add_decoration(
      std::make_unique<ast::WorkgroupDecoration>(2u, 4u, 6u, Source{}));
  func.set_body(std::move(body));

  GeneratorImpl g;
  g.increment_indent();

  ASSERT_TRUE(g.EmitFunction(&func));
  EXPECT_EQ(g.result(), R"(  [[stage(fragment)]]
  [[workgroup_size(2, 4, 6)]]
  fn my_func() -> void {
    discard;
    return;
  }
)");
}

// https://crbug.com/tint/297
TEST_F(WgslGeneratorImplTest,
       Emit_Function_Multiple_EntryPoint_With_Same_ModuleVar) {
  // [[block]] struct Data {
  //   [[offset(0)]] d : f32;
  // };
  // [[binding(0), set(0)]] var<storage_buffer> data : Data;
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

  ast::type::VoidType void_type;
  ast::type::F32Type f32;

  ast::StructMemberList members;
  ast::StructMemberDecorationList a_deco;
  a_deco.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(0, Source{}));
  members.push_back(
      std::make_unique<ast::StructMember>("d", &f32, std::move(a_deco)));

  ast::StructDecorationList s_decos;
  s_decos.push_back(std::make_unique<ast::StructBlockDecoration>(Source{}));

  auto str =
      std::make_unique<ast::Struct>(std::move(s_decos), std::move(members));

  ast::type::StructType s("Data", std::move(str));
  ast::type::AccessControlType ac(ast::AccessControl::kReadWrite, &s);

  auto data_var =
      std::make_unique<ast::DecoratedVariable>(std::make_unique<ast::Variable>(
          "data", ast::StorageClass::kStorageBuffer, &ac));

  ast::VariableDecorationList decos;
  decos.push_back(std::make_unique<ast::BindingDecoration>(0, Source{}));
  decos.push_back(std::make_unique<ast::SetDecoration>(0, Source{}));
  data_var->set_decorations(std::move(decos));

  Context ctx;
  ast::Module mod;
  TypeDeterminer td(&ctx, &mod);

  mod.AddConstructedType(&s);

  td.RegisterVariableForTesting(data_var.get());
  mod.AddGlobalVariable(std::move(data_var));

  {
    ast::VariableList params;
    auto func =
        std::make_unique<ast::Function>("a", std::move(params), &void_type);
    func->add_decoration(std::make_unique<ast::StageDecoration>(
        ast::PipelineStage::kCompute, Source{}));

    auto var = std::make_unique<ast::Variable>(
        "v", ast::StorageClass::kFunction, &f32);
    var->set_constructor(std::make_unique<ast::MemberAccessorExpression>(
        std::make_unique<ast::IdentifierExpression>("data"),
        std::make_unique<ast::IdentifierExpression>("d")));

    auto body = std::make_unique<ast::BlockStatement>();
    body->append(std::make_unique<ast::VariableDeclStatement>(std::move(var)));
    body->append(std::make_unique<ast::ReturnStatement>());
    func->set_body(std::move(body));

    mod.AddFunction(std::move(func));
  }

  {
    ast::VariableList params;
    auto func =
        std::make_unique<ast::Function>("b", std::move(params), &void_type);
    func->add_decoration(std::make_unique<ast::StageDecoration>(
        ast::PipelineStage::kCompute, Source{}));

    auto var = std::make_unique<ast::Variable>(
        "v", ast::StorageClass::kFunction, &f32);
    var->set_constructor(std::make_unique<ast::MemberAccessorExpression>(
        std::make_unique<ast::IdentifierExpression>("data"),
        std::make_unique<ast::IdentifierExpression>("d")));

    auto body = std::make_unique<ast::BlockStatement>();
    body->append(std::make_unique<ast::VariableDeclStatement>(std::move(var)));
    body->append(std::make_unique<ast::ReturnStatement>());
    func->set_body(std::move(body));

    mod.AddFunction(std::move(func));
  }

  ASSERT_TRUE(td.Determine()) << td.error();

  GeneratorImpl g;
  ASSERT_TRUE(g.Generate(mod)) << g.error();
  EXPECT_EQ(g.result(), R"([[block]]
struct Data {
  [[offset(0)]]
  d : f32;
};

[[binding(0), set(0)]] var<storage_buffer> data : Data;

[[stage(compute)]]
fn a() -> void {
  var v : f32 = data.d;
  return;
}

[[stage(compute)]]
fn b() -> void {
  var v : f32 = data.d;
  return;
}

)");
}

}  // namespace
}  // namespace wgsl
}  // namespace writer
}  // namespace tint
