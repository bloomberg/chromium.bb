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
#include "src/ast/identifier_expression.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type_constructor_expression.h"
#include "src/writer/wgsl/generator_impl.h"

namespace tint {
namespace writer {
namespace wgsl {
namespace {

using WgslGeneratorImplTest = testing::Test;

TEST_F(WgslGeneratorImplTest, EmitExpression_Cast) {
  ast::type::F32Type f32;

  ast::ExpressionList params;
  params.push_back(std::make_unique<ast::IdentifierExpression>("id"));

  ast::TypeConstructorExpression cast(&f32, std::move(params));

  GeneratorImpl g;
  ASSERT_TRUE(g.EmitExpression(&cast)) << g.error();
  EXPECT_EQ(g.result(), "f32(id)");
}

}  // namespace
}  // namespace wgsl
}  // namespace writer
}  // namespace tint
