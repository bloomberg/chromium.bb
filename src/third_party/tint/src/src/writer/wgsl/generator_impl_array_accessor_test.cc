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

#include "src/writer/wgsl/test_helper.h"

namespace tint {
namespace writer {
namespace wgsl {
namespace {

using WgslGeneratorImplTest = TestHelper;

TEST_F(WgslGeneratorImplTest, EmitExpression_ArrayAccessor) {
  auto* idx = Expr(5);
  auto* ary = Expr("ary");

  auto* expr = create<ast::ArrayAccessorExpression>(ary, idx);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(expr)) << gen.error();
  EXPECT_EQ(gen.result(), "ary[5]");
}

TEST_F(WgslGeneratorImplTest, EmitArrayAccessor) {
  auto* ary = Expr("ary");
  auto* idx = Expr("idx");

  auto* expr = create<ast::ArrayAccessorExpression>(ary, idx);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitArrayAccessor(expr)) << gen.error();
  EXPECT_EQ(gen.result(), "ary[idx]");
}

}  // namespace
}  // namespace wgsl
}  // namespace writer
}  // namespace tint
