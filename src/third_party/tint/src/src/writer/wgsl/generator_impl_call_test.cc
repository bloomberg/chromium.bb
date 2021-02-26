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

#include <memory>

#include "gtest/gtest.h"
#include "src/ast/call_expression.h"
#include "src/ast/call_statement.h"
#include "src/ast/identifier_expression.h"
#include "src/writer/wgsl/generator_impl.h"

namespace tint {
namespace writer {
namespace wgsl {
namespace {

using WgslGeneratorImplTest = testing::Test;

TEST_F(WgslGeneratorImplTest, EmitExpression_Call_WithoutParams) {
  auto id = std::make_unique<ast::IdentifierExpression>("my_func");
  ast::CallExpression call(std::move(id), {});

  GeneratorImpl g;
  ASSERT_TRUE(g.EmitExpression(&call)) << g.error();
  EXPECT_EQ(g.result(), "my_func()");
}

TEST_F(WgslGeneratorImplTest, EmitExpression_Call_WithParams) {
  auto id = std::make_unique<ast::IdentifierExpression>("my_func");
  ast::ExpressionList params;
  params.push_back(std::make_unique<ast::IdentifierExpression>("param1"));
  params.push_back(std::make_unique<ast::IdentifierExpression>("param2"));
  ast::CallExpression call(std::move(id), std::move(params));

  GeneratorImpl g;
  ASSERT_TRUE(g.EmitExpression(&call)) << g.error();
  EXPECT_EQ(g.result(), "my_func(param1, param2)");
}

TEST_F(WgslGeneratorImplTest, EmitStatement_Call) {
  auto id = std::make_unique<ast::IdentifierExpression>("my_func");
  ast::ExpressionList params;
  params.push_back(std::make_unique<ast::IdentifierExpression>("param1"));
  params.push_back(std::make_unique<ast::IdentifierExpression>("param2"));

  ast::CallStatement call(
      std::make_unique<ast::CallExpression>(std::move(id), std::move(params)));

  GeneratorImpl g;
  g.increment_indent();
  ASSERT_TRUE(g.EmitStatement(&call)) << g.error();
  EXPECT_EQ(g.result(), "  my_func(param1, param2);\n");
}

}  // namespace
}  // namespace wgsl
}  // namespace writer
}  // namespace tint
