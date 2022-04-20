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

using GlslGeneratorImplTest_Import = TestHelper;

struct GlslImportData {
  const char* name;
  const char* glsl_name;
};
inline std::ostream& operator<<(std::ostream& out, GlslImportData data) {
  out << data.name;
  return out;
}

using GlslImportData_SingleParamTest = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_SingleParamTest, FloatScalar) {
  auto param = GetParam();

  auto* ident = Expr(param.name);
  auto* expr = Call(ident, 1.f);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string(param.glsl_name) + "(1.0f)");
}
INSTANTIATE_TEST_SUITE_P(GlslGeneratorImplTest_Import,
                         GlslImportData_SingleParamTest,
                         testing::Values(GlslImportData{"abs", "abs"},
                                         GlslImportData{"acos", "acos"},
                                         GlslImportData{"asin", "asin"},
                                         GlslImportData{"atan", "atan"},
                                         GlslImportData{"cos", "cos"},
                                         GlslImportData{"cosh", "cosh"},
                                         GlslImportData{"ceil", "ceil"},
                                         GlslImportData{"exp", "exp"},
                                         GlslImportData{"exp2", "exp2"},
                                         GlslImportData{"floor", "floor"},
                                         GlslImportData{"fract", "fract"},
                                         GlslImportData{"inverseSqrt",
                                                        "inversesqrt"},
                                         GlslImportData{"length", "length"},
                                         GlslImportData{"log", "log"},
                                         GlslImportData{"log2", "log2"},
                                         GlslImportData{"round", "round"},
                                         GlslImportData{"sign", "sign"},
                                         GlslImportData{"sin", "sin"},
                                         GlslImportData{"sinh", "sinh"},
                                         GlslImportData{"sqrt", "sqrt"},
                                         GlslImportData{"tan", "tan"},
                                         GlslImportData{"tanh", "tanh"},
                                         GlslImportData{"trunc", "trunc"}));

using GlslImportData_SingleIntParamTest = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_SingleIntParamTest, IntScalar) {
  auto param = GetParam();

  auto* expr = Call(param.name, Expr(1));
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string(param.glsl_name) + "(1)");
}
INSTANTIATE_TEST_SUITE_P(GlslGeneratorImplTest_Import,
                         GlslImportData_SingleIntParamTest,
                         testing::Values(GlslImportData{"abs", "abs"}));

using GlslImportData_SingleVectorParamTest = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_SingleVectorParamTest, FloatVector) {
  auto param = GetParam();

  auto* ident = Expr(param.name);
  auto* expr = Call(ident, vec3<f32>(1.f, 2.f, 3.f));
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(),
            std::string(param.glsl_name) + "(vec3(1.0f, 2.0f, 3.0f))");
}
INSTANTIATE_TEST_SUITE_P(
    GlslGeneratorImplTest_Import,
    GlslImportData_SingleVectorParamTest,
    testing::Values(GlslImportData{"abs", "abs"},
                    GlslImportData{"acos", "acos"},
                    GlslImportData{"asin", "asin"},
                    GlslImportData{"atan", "atan"},
                    GlslImportData{"cos", "cos"},
                    GlslImportData{"cosh", "cosh"},
                    GlslImportData{"ceil", "ceil"},
                    GlslImportData{"exp", "exp"},
                    GlslImportData{"exp2", "exp2"},
                    GlslImportData{"floor", "floor"},
                    GlslImportData{"fract", "fract"},
                    GlslImportData{"inverseSqrt", "inversesqrt"},
                    GlslImportData{"length", "length"},
                    GlslImportData{"log", "log"},
                    GlslImportData{"log2", "log2"},
                    GlslImportData{"normalize", "normalize"},
                    GlslImportData{"round", "round"},
                    GlslImportData{"sign", "sign"},
                    GlslImportData{"sin", "sin"},
                    GlslImportData{"sinh", "sinh"},
                    GlslImportData{"sqrt", "sqrt"},
                    GlslImportData{"tan", "tan"},
                    GlslImportData{"tanh", "tanh"},
                    GlslImportData{"trunc", "trunc"}));

using GlslImportData_DualParam_ScalarTest = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_DualParam_ScalarTest, Float) {
  auto param = GetParam();

  auto* expr = Call(param.name, 1.f, 2.f);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string(param.glsl_name) + "(1.0f, 2.0f)");
}
INSTANTIATE_TEST_SUITE_P(GlslGeneratorImplTest_Import,
                         GlslImportData_DualParam_ScalarTest,
                         testing::Values(GlslImportData{"atan2", "atan"},
                                         GlslImportData{"distance", "distance"},
                                         GlslImportData{"max", "max"},
                                         GlslImportData{"min", "min"},
                                         GlslImportData{"pow", "pow"},
                                         GlslImportData{"step", "step"}));

using GlslImportData_DualParam_VectorTest = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_DualParam_VectorTest, Float) {
  auto param = GetParam();

  auto* expr =
      Call(param.name, vec3<f32>(1.f, 2.f, 3.f), vec3<f32>(4.f, 5.f, 6.f));
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string(param.glsl_name) +
                           "(vec3(1.0f, 2.0f, 3.0f), vec3(4.0f, 5.0f, 6.0f))");
}
INSTANTIATE_TEST_SUITE_P(GlslGeneratorImplTest_Import,
                         GlslImportData_DualParam_VectorTest,
                         testing::Values(GlslImportData{"atan2", "atan"},
                                         GlslImportData{"cross", "cross"},
                                         GlslImportData{"distance", "distance"},
                                         GlslImportData{"max", "max"},
                                         GlslImportData{"min", "min"},
                                         GlslImportData{"pow", "pow"},
                                         GlslImportData{"reflect", "reflect"},
                                         GlslImportData{"step", "step"}));

using GlslImportData_DualParam_Int_Test = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_DualParam_Int_Test, IntScalar) {
  auto param = GetParam();

  auto* expr = Call(param.name, 1, 2);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string(param.glsl_name) + "(1, 2)");
}
INSTANTIATE_TEST_SUITE_P(GlslGeneratorImplTest_Import,
                         GlslImportData_DualParam_Int_Test,
                         testing::Values(GlslImportData{"max", "max"},
                                         GlslImportData{"min", "min"}));

using GlslImportData_TripleParam_ScalarTest = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_TripleParam_ScalarTest, Float) {
  auto param = GetParam();

  auto* expr = Call(param.name, 1.f, 2.f, 3.f);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string(param.glsl_name) + "(1.0f, 2.0f, 3.0f)");
}
INSTANTIATE_TEST_SUITE_P(GlslGeneratorImplTest_Import,
                         GlslImportData_TripleParam_ScalarTest,
                         testing::Values(GlslImportData{"mix", "mix"},
                                         GlslImportData{"clamp", "clamp"},
                                         GlslImportData{"smoothStep",
                                                        "smoothstep"}));

using GlslImportData_TripleParam_VectorTest = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_TripleParam_VectorTest, Float) {
  auto param = GetParam();

  auto* expr = Call(param.name, vec3<f32>(1.f, 2.f, 3.f),
                    vec3<f32>(4.f, 5.f, 6.f), vec3<f32>(7.f, 8.f, 9.f));
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(
      out.str(),
      std::string(param.glsl_name) +
          R"((vec3(1.0f, 2.0f, 3.0f), vec3(4.0f, 5.0f, 6.0f), vec3(7.0f, 8.0f, 9.0f)))");
}
INSTANTIATE_TEST_SUITE_P(
    GlslGeneratorImplTest_Import,
    GlslImportData_TripleParam_VectorTest,
    testing::Values(GlslImportData{"faceForward", "faceforward"},
                    GlslImportData{"clamp", "clamp"},
                    GlslImportData{"smoothStep", "smoothstep"}));

TEST_F(GlslGeneratorImplTest_Import, DISABLED_GlslImportData_FMix) {
  FAIL();
}

using GlslImportData_TripleParam_Int_Test = TestParamHelper<GlslImportData>;
TEST_P(GlslImportData_TripleParam_Int_Test, IntScalar) {
  auto param = GetParam();

  auto* expr = Call(param.name, 1, 2, 3);
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string(param.glsl_name) + "(1, 2, 3)");
}
INSTANTIATE_TEST_SUITE_P(GlslGeneratorImplTest_Import,
                         GlslImportData_TripleParam_Int_Test,
                         testing::Values(GlslImportData{"clamp", "clamp"}));

TEST_F(GlslGeneratorImplTest_Import, GlslImportData_Determinant) {
  Global("var", ty.mat3x3<f32>(), ast::StorageClass::kPrivate);

  auto* expr = Call("determinant", "var");
  WrapInFunction(expr);

  GeneratorImpl& gen = Build();

  std::stringstream out;
  ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
  EXPECT_EQ(out.str(), std::string("determinant(var)"));
}

}  // namespace
}  // namespace tint::writer::glsl
