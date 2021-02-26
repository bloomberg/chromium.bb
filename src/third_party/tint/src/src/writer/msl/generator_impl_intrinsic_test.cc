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
#include "src/ast/call_expression.h"
#include "src/ast/identifier_expression.h"
#include "src/ast/module.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type/vector_type.h"
#include "src/context.h"
#include "src/type_determiner.h"
#include "src/writer/msl/generator_impl.h"

namespace tint {
namespace writer {
namespace msl {
namespace {

using MslGeneratorImplTest = testing::Test;

struct IntrinsicData {
  ast::Intrinsic intrinsic;
  const char* msl_name;
};
inline std::ostream& operator<<(std::ostream& out, IntrinsicData data) {
  out << data.msl_name;
  return out;
}
using MslIntrinsicTest = testing::TestWithParam<IntrinsicData>;
TEST_P(MslIntrinsicTest, Emit) {
  auto param = GetParam();

  ast::Module m;
  GeneratorImpl g(&m);
  EXPECT_EQ(g.generate_intrinsic_name(param.intrinsic), param.msl_name);
}
INSTANTIATE_TEST_SUITE_P(
    MslGeneratorImplTest,
    MslIntrinsicTest,
    testing::Values(IntrinsicData{ast::Intrinsic::kAny, "any"},
                    IntrinsicData{ast::Intrinsic::kAll, "all"},
                    IntrinsicData{ast::Intrinsic::kCountOneBits, "popcount"},
                    IntrinsicData{ast::Intrinsic::kDot, "dot"},
                    IntrinsicData{ast::Intrinsic::kDpdx, "dfdx"},
                    IntrinsicData{ast::Intrinsic::kDpdxCoarse, "dfdx"},
                    IntrinsicData{ast::Intrinsic::kDpdxFine, "dfdx"},
                    IntrinsicData{ast::Intrinsic::kDpdy, "dfdy"},
                    IntrinsicData{ast::Intrinsic::kDpdyCoarse, "dfdy"},
                    IntrinsicData{ast::Intrinsic::kDpdyFine, "dfdy"},
                    IntrinsicData{ast::Intrinsic::kFwidth, "fwidth"},
                    IntrinsicData{ast::Intrinsic::kFwidthCoarse, "fwidth"},
                    IntrinsicData{ast::Intrinsic::kFwidthFine, "fwidth"},
                    IntrinsicData{ast::Intrinsic::kIsFinite, "isfinite"},
                    IntrinsicData{ast::Intrinsic::kIsInf, "isinf"},
                    IntrinsicData{ast::Intrinsic::kIsNan, "isnan"},
                    IntrinsicData{ast::Intrinsic::kIsNormal, "isnormal"},
                    IntrinsicData{ast::Intrinsic::kReverseBits, "reverse_bits"},
                    IntrinsicData{ast::Intrinsic::kSelect, "select"}));

TEST_F(MslGeneratorImplTest, DISABLED_Intrinsic_OuterProduct) {
  ast::type::F32Type f32;
  ast::type::VectorType vec2(&f32, 2);
  ast::type::VectorType vec3(&f32, 3);

  auto a =
      std::make_unique<ast::Variable>("a", ast::StorageClass::kNone, &vec2);
  auto b =
      std::make_unique<ast::Variable>("b", ast::StorageClass::kNone, &vec3);

  ast::ExpressionList params;
  params.push_back(std::make_unique<ast::IdentifierExpression>("a"));
  params.push_back(std::make_unique<ast::IdentifierExpression>("b"));

  ast::CallExpression call(
      std::make_unique<ast::IdentifierExpression>("outer_product"),
      std::move(params));

  Context ctx;
  ast::Module m;
  TypeDeterminer td(&ctx, &m);
  td.RegisterVariableForTesting(a.get());
  td.RegisterVariableForTesting(b.get());

  m.AddGlobalVariable(std::move(a));
  m.AddGlobalVariable(std::move(b));

  ASSERT_TRUE(td.Determine()) << td.error();
  ASSERT_TRUE(td.DetermineResultType(&call)) << td.error();

  GeneratorImpl g(&m);

  g.increment_indent();
  ASSERT_TRUE(g.EmitExpression(&call)) << g.error();
  EXPECT_EQ(g.result(), "  float3x2(a * b[0], a * b[1], a * b[2])");
}

TEST_F(MslGeneratorImplTest, Intrinsic_Bad_Name) {
  ast::Module m;
  GeneratorImpl g(&m);
  EXPECT_EQ(g.generate_intrinsic_name(ast::Intrinsic::kNone), "");
}

TEST_F(MslGeneratorImplTest, Intrinsic_Call) {
  ast::type::F32Type f32;
  ast::type::VectorType vec(&f32, 3);

  ast::ExpressionList params;
  params.push_back(std::make_unique<ast::IdentifierExpression>("param1"));
  params.push_back(std::make_unique<ast::IdentifierExpression>("param2"));

  ast::CallExpression call(std::make_unique<ast::IdentifierExpression>("dot"),
                           std::move(params));

  Context ctx;
  ast::Module m;
  TypeDeterminer td(&ctx, &m);

  ast::Variable v1("param1", ast::StorageClass::kFunction, &vec);
  ast::Variable v2("param2", ast::StorageClass::kFunction, &vec);

  td.RegisterVariableForTesting(&v1);
  td.RegisterVariableForTesting(&v2);

  ASSERT_TRUE(td.DetermineResultType(&call)) << td.error();

  GeneratorImpl g(&m);
  g.increment_indent();
  ASSERT_TRUE(g.EmitExpression(&call)) << g.error();
  EXPECT_EQ(g.result(), "  dot(param1, param2)");
}

}  // namespace
}  // namespace msl
}  // namespace writer
}  // namespace tint
