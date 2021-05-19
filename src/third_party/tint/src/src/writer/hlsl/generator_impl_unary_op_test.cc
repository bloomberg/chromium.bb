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

#include "src/writer/hlsl/test_helper.h"

namespace tint {
namespace writer {
namespace hlsl {
namespace {

struct UnaryOpData {
  const char* name;
  ast::UnaryOp op;
};
inline std::ostream& operator<<(std::ostream& out, UnaryOpData data) {
  out << data.op;
  return out;
}
using HlslUnaryOpTest = TestParamHelper<UnaryOpData>;
TEST_P(HlslUnaryOpTest, Emit) {
  auto params = GetParam();

  auto* expr = Expr("expr");
  auto* op = create<ast::UnaryOpExpression>(params.op, expr);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitExpression(pre, out, op)) << gen.error();
  EXPECT_EQ(result(), std::string(params.name) + "(expr)");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_UnaryOp,
                         HlslUnaryOpTest,
                         testing::Values(UnaryOpData{"!", ast::UnaryOp::kNot},
                                         UnaryOpData{"-",
                                                     ast::UnaryOp::kNegation}));

}  // namespace
}  // namespace hlsl
}  // namespace writer
}  // namespace tint
