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

#include "src/tint/ast/test_helper.h"

namespace tint::ast {
namespace {

using IntLiteralExpressionTest = TestHelper;

TEST_F(IntLiteralExpressionTest, Sint_IsInt) {
  auto* i = create<SintLiteralExpression>(47);
  ASSERT_TRUE(i->Is<IntLiteralExpression>());
}

TEST_F(IntLiteralExpressionTest, Uint_IsInt) {
  auto* i = create<UintLiteralExpression>(42);
  EXPECT_TRUE(i->Is<IntLiteralExpression>());
}

}  // namespace
}  // namespace tint::ast
