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

#include "src/tint/writer/glsl/test_helper.h"

namespace tint::writer::glsl {
namespace {

using GlslGeneratorImplTest_Assign = TestHelper;

TEST_F(GlslGeneratorImplTest_Assign, Emit_Assign) {
  Global("lhs", ty.i32(), ast::StorageClass::kPrivate);
  Global("rhs", ty.i32(), ast::StorageClass::kPrivate);
  auto* assign = Assign("lhs", "rhs");
  WrapInFunction(assign);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(assign)) << gen.error();
  EXPECT_EQ(gen.result(), "  lhs = rhs;\n");
}

}  // namespace
}  // namespace tint::writer::glsl
