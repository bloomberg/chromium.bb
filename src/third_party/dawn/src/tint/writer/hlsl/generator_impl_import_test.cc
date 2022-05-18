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

#include "src/tint/writer/hlsl/test_helper.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::writer::hlsl {
namespace {

using HlslGeneratorImplTest_Import = TestHelper;

struct HlslImportData {
    const char* name;
    const char* hlsl_name;
};
inline std::ostream& operator<<(std::ostream& out, HlslImportData data) {
    out << data.name;
    return out;
}

using HlslImportData_SingleParamTest = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_SingleParamTest, FloatScalar) {
    auto param = GetParam();

    auto* ident = Expr(param.name);
    auto* expr = Call(ident, 1_f);
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) + "(1.0f)");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_SingleParamTest,
                         testing::Values(HlslImportData{"abs", "abs"},
                                         HlslImportData{"acos", "acos"},
                                         HlslImportData{"asin", "asin"},
                                         HlslImportData{"atan", "atan"},
                                         HlslImportData{"cos", "cos"},
                                         HlslImportData{"cosh", "cosh"},
                                         HlslImportData{"ceil", "ceil"},
                                         HlslImportData{"exp", "exp"},
                                         HlslImportData{"exp2", "exp2"},
                                         HlslImportData{"floor", "floor"},
                                         HlslImportData{"fract", "frac"},
                                         HlslImportData{"inverseSqrt", "rsqrt"},
                                         HlslImportData{"length", "length"},
                                         HlslImportData{"log", "log"},
                                         HlslImportData{"log2", "log2"},
                                         HlslImportData{"round", "round"},
                                         HlslImportData{"sign", "sign"},
                                         HlslImportData{"sin", "sin"},
                                         HlslImportData{"sinh", "sinh"},
                                         HlslImportData{"sqrt", "sqrt"},
                                         HlslImportData{"tan", "tan"},
                                         HlslImportData{"tanh", "tanh"},
                                         HlslImportData{"trunc", "trunc"}));

using HlslImportData_SingleIntParamTest = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_SingleIntParamTest, IntScalar) {
    auto param = GetParam();

    auto* expr = Call(param.name, Expr(1_i));
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) + "(1)");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_SingleIntParamTest,
                         testing::Values(HlslImportData{"abs", "abs"}));

using HlslImportData_SingleVectorParamTest = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_SingleVectorParamTest, FloatVector) {
    auto param = GetParam();

    auto* ident = Expr(param.name);
    auto* expr = Call(ident, vec3<f32>(1_f, 2_f, 3_f));
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) + "(float3(1.0f, 2.0f, 3.0f))");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_SingleVectorParamTest,
                         testing::Values(HlslImportData{"abs", "abs"},
                                         HlslImportData{"acos", "acos"},
                                         HlslImportData{"asin", "asin"},
                                         HlslImportData{"atan", "atan"},
                                         HlslImportData{"cos", "cos"},
                                         HlslImportData{"cosh", "cosh"},
                                         HlslImportData{"ceil", "ceil"},
                                         HlslImportData{"exp", "exp"},
                                         HlslImportData{"exp2", "exp2"},
                                         HlslImportData{"floor", "floor"},
                                         HlslImportData{"fract", "frac"},
                                         HlslImportData{"inverseSqrt", "rsqrt"},
                                         HlslImportData{"length", "length"},
                                         HlslImportData{"log", "log"},
                                         HlslImportData{"log2", "log2"},
                                         HlslImportData{"normalize", "normalize"},
                                         HlslImportData{"round", "round"},
                                         HlslImportData{"sign", "sign"},
                                         HlslImportData{"sin", "sin"},
                                         HlslImportData{"sinh", "sinh"},
                                         HlslImportData{"sqrt", "sqrt"},
                                         HlslImportData{"tan", "tan"},
                                         HlslImportData{"tanh", "tanh"},
                                         HlslImportData{"trunc", "trunc"}));

using HlslImportData_DualParam_ScalarTest = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_DualParam_ScalarTest, Float) {
    auto param = GetParam();

    auto* expr = Call(param.name, 1_f, 2_f);
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) + "(1.0f, 2.0f)");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_DualParam_ScalarTest,
                         testing::Values(HlslImportData{"atan2", "atan2"},
                                         HlslImportData{"distance", "distance"},
                                         HlslImportData{"max", "max"},
                                         HlslImportData{"min", "min"},
                                         HlslImportData{"pow", "pow"},
                                         HlslImportData{"step", "step"}));

using HlslImportData_DualParam_VectorTest = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_DualParam_VectorTest, Float) {
    auto param = GetParam();

    auto* expr = Call(param.name, vec3<f32>(1_f, 2_f, 3_f), vec3<f32>(4_f, 5_f, 6_f));
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) +
                             "(float3(1.0f, 2.0f, 3.0f), float3(4.0f, 5.0f, 6.0f))");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_DualParam_VectorTest,
                         testing::Values(HlslImportData{"atan2", "atan2"},
                                         HlslImportData{"cross", "cross"},
                                         HlslImportData{"distance", "distance"},
                                         HlslImportData{"max", "max"},
                                         HlslImportData{"min", "min"},
                                         HlslImportData{"pow", "pow"},
                                         HlslImportData{"reflect", "reflect"},
                                         HlslImportData{"step", "step"}));

using HlslImportData_DualParam_Int_Test = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_DualParam_Int_Test, IntScalar) {
    auto param = GetParam();

    auto* expr = Call(param.name, 1_i, 2_i);
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) + "(1, 2)");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_DualParam_Int_Test,
                         testing::Values(HlslImportData{"max", "max"},
                                         HlslImportData{"min", "min"}));

using HlslImportData_TripleParam_ScalarTest = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_TripleParam_ScalarTest, Float) {
    auto param = GetParam();

    auto* expr = Call(param.name, 1_f, 2_f, 3_f);
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) + "(1.0f, 2.0f, 3.0f)");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_TripleParam_ScalarTest,
                         testing::Values(HlslImportData{"fma", "mad"},
                                         HlslImportData{"mix", "lerp"},
                                         HlslImportData{"clamp", "clamp"},
                                         HlslImportData{"smoothstep", "smoothstep"}));

using HlslImportData_TripleParam_VectorTest = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_TripleParam_VectorTest, Float) {
    auto param = GetParam();

    auto* expr = Call(param.name, vec3<f32>(1_f, 2_f, 3_f), vec3<f32>(4_f, 5_f, 6_f),
                      vec3<f32>(7_f, 8_f, 9_f));
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(
        out.str(),
        std::string(param.hlsl_name) +
            R"((float3(1.0f, 2.0f, 3.0f), float3(4.0f, 5.0f, 6.0f), float3(7.0f, 8.0f, 9.0f)))");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_TripleParam_VectorTest,
                         testing::Values(HlslImportData{"faceForward", "faceforward"},
                                         HlslImportData{"fma", "mad"},
                                         HlslImportData{"clamp", "clamp"},
                                         HlslImportData{"smoothstep", "smoothstep"}));

TEST_F(HlslGeneratorImplTest_Import, DISABLED_HlslImportData_FMix) {
    FAIL();
}

using HlslImportData_TripleParam_Int_Test = TestParamHelper<HlslImportData>;
TEST_P(HlslImportData_TripleParam_Int_Test, IntScalar) {
    auto param = GetParam();

    auto* expr = Call(param.name, 1_i, 2_i, 3_i);
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string(param.hlsl_name) + "(1, 2, 3)");
}
INSTANTIATE_TEST_SUITE_P(HlslGeneratorImplTest_Import,
                         HlslImportData_TripleParam_Int_Test,
                         testing::Values(HlslImportData{"clamp", "clamp"}));

TEST_F(HlslGeneratorImplTest_Import, HlslImportData_Determinant) {
    Global("var", ty.mat3x3<f32>(), ast::StorageClass::kPrivate);

    auto* expr = Call("determinant", "var");
    WrapInFunction(expr);

    GeneratorImpl& gen = Build();

    std::stringstream out;
    ASSERT_TRUE(gen.EmitCall(out, expr)) << gen.error();
    EXPECT_EQ(out.str(), std::string("determinant(var)"));
}

}  // namespace
}  // namespace tint::writer::hlsl
