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

#include "gmock/gmock.h"
#include "src/reader/spirv/function.h"
#include "src/reader/spirv/parser_impl_test_helper.h"
#include "src/reader/spirv/spirv_tools_helpers_test.h"

namespace tint {
namespace reader {
namespace spirv {
namespace {

using ::testing::HasSubstr;

std::string CommonTypes() {
  return R"(
  %void = OpTypeVoid
  %voidfn = OpTypeFunction %void

  %uint = OpTypeInt 32 0
  %int = OpTypeInt 32 1
  %float = OpTypeFloat 32

  %uint_10 = OpConstant %uint 10
  %uint_20 = OpConstant %uint 20
  %int_30 = OpConstant %int 30
  %int_40 = OpConstant %int 40
  %float_50 = OpConstant %float 50
  %float_60 = OpConstant %float 60
  %float_70 = OpConstant %float 70

  %ptr_uint = OpTypePointer Function %uint
  %ptr_int = OpTypePointer Function %int
  %ptr_float = OpTypePointer Function %float

  %v2uint = OpTypeVector %uint 2
  %v2int = OpTypeVector %int 2
  %v2float = OpTypeVector %float 2
  %v3float = OpTypeVector %float 3

  %v2uint_10_20 = OpConstantComposite %v2uint %uint_10 %uint_20
  %v2uint_20_10 = OpConstantComposite %v2uint %uint_20 %uint_10
  %v2int_30_40 = OpConstantComposite %v2int %int_30 %int_40
  %v2int_40_30 = OpConstantComposite %v2int %int_40 %int_30
  %v2float_50_60 = OpConstantComposite %v2float %float_50 %float_60
  %v2float_60_50 = OpConstantComposite %v2float %float_60 %float_50
  %v3float_50_60_70 = OpConstantComposite %v2float %float_50 %float_60 %float_70

  %m2v2float = OpTypeMatrix %v2float 2
  %m2v2float_a = OpConstantComposite %m2v2float %v2float_50_60 %v2float_60_50
  %m2v2float_b = OpConstantComposite %m2v2float %v2float_60_50 %v2float_50_60
  %m2v3float = OpTypeMatrix %v3float 2
)";
}

// Returns the AST dump for a given SPIR-V assembly constant.
std::string AstFor(std::string assembly) {
  if (assembly == "v2uint_10_20") {
    return R"(TypeConstructor[not set]{
          __vec_2__u32
          ScalarConstructor[not set]{10}
          ScalarConstructor[not set]{20}
        })";
  }
  if (assembly == "v2uint_20_10") {
    return R"(TypeConstructor[not set]{
          __vec_2__u32
          ScalarConstructor[not set]{20}
          ScalarConstructor[not set]{10}
        })";
  }
  if (assembly == "v2int_30_40") {
    return R"(TypeConstructor[not set]{
          __vec_2__i32
          ScalarConstructor[not set]{30}
          ScalarConstructor[not set]{40}
        })";
  }
  if (assembly == "v2int_40_30") {
    return R"(TypeConstructor[not set]{
          __vec_2__i32
          ScalarConstructor[not set]{40}
          ScalarConstructor[not set]{30}
        })";
  }
  if (assembly == "cast_int_v2uint_10_20") {
    return R"(Bitcast[not set]<__vec_2__i32>{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
        })";
  }
  if (assembly == "cast_uint_v2int_40_30") {
    return R"(Bitcast[not set]<__vec_2__u32>{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{40}
            ScalarConstructor[not set]{30}
          }
        })";
  }
  if (assembly == "v2float_50_60") {
    return R"(TypeConstructor[not set]{
          __vec_2__f32
          ScalarConstructor[not set]{50.000000}
          ScalarConstructor[not set]{60.000000}
        })";
  }
  if (assembly == "v2float_60_50") {
    return R"(TypeConstructor[not set]{
          __vec_2__f32
          ScalarConstructor[not set]{60.000000}
          ScalarConstructor[not set]{50.000000}
        })";
  }
  return "bad case";
}

using SpvUnaryArithTest = SpvParserTestBase<::testing::Test>;

TEST_F(SpvUnaryArithTest, SNegate_Int_Int) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %int %int_30
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __i32
    {
      UnaryOp[not set]{
        negation
        ScalarConstructor[not set]{30}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, SNegate_Int_Uint) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %int %uint_10
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __i32
    {
      UnaryOp[not set]{
        negation
        Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, SNegate_Uint_Int) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %uint %int_30
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Bitcast[not set]<__u32>{
        UnaryOp[not set]{
          negation
          ScalarConstructor[not set]{30}
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, SNegate_Uint_Uint) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %uint %uint_10
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Bitcast[not set]<__u32>{
        UnaryOp[not set]{
          negation
          Bitcast[not set]<__i32>{
            ScalarConstructor[not set]{10}
          }
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, SNegate_SignedVec_SignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %v2int %v2int_30_40
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__i32
    {
      UnaryOp[not set]{
        negation
        TypeConstructor[not set]{
          __vec_2__i32
          ScalarConstructor[not set]{30}
          ScalarConstructor[not set]{40}
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, SNegate_SignedVec_UnsignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %v2int %v2uint_10_20
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__i32
    {
      UnaryOp[not set]{
        negation
        Bitcast[not set]<__vec_2__i32>{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, SNegate_UnsignedVec_SignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %v2uint %v2int_30_40
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        UnaryOp[not set]{
          negation
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, SNegate_UnsignedVec_UnsignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSNegate %v2uint %v2uint_10_20
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        UnaryOp[not set]{
          negation
          Bitcast[not set]<__vec_2__i32>{
            TypeConstructor[not set]{
              __vec_2__u32
              ScalarConstructor[not set]{10}
              ScalarConstructor[not set]{20}
            }
          }
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, FNegate_Scalar) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpFNegate %float %float_50
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __f32
    {
      UnaryOp[not set]{
        negation
        ScalarConstructor[not set]{50.000000}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryArithTest, FNegate_Vector) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpFNegate %v2float %v2float_50_60
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__f32
    {
      UnaryOp[not set]{
        negation
        TypeConstructor[not set]{
          __vec_2__f32
          ScalarConstructor[not set]{50.000000}
          ScalarConstructor[not set]{60.000000}
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

struct BinaryData {
  const std::string res_type;
  const std::string lhs;
  const std::string op;
  const std::string rhs;
  const std::string ast_type;
  const std::string ast_lhs;
  const std::string ast_op;
  const std::string ast_rhs;
};
inline std::ostream& operator<<(std::ostream& out, BinaryData data) {
  out << "BinaryData{" << data.res_type << "," << data.lhs << "," << data.op
      << "," << data.rhs << "," << data.ast_type << "," << data.ast_lhs << ","
      << data.ast_op << "," << data.ast_rhs << "}";
  return out;
}

using SpvBinaryArithTest =
    SpvParserTestBase<::testing::TestWithParam<BinaryData>>;
using SpvBinaryArithTestBasic = SpvParserTestBase<::testing::Test>;

TEST_P(SpvBinaryArithTest, EmitExpression) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = )" + GetParam().op +
                        " %" + GetParam().res_type + " %" + GetParam().lhs +
                        " %" + GetParam().rhs + R"(
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions())
      << p->error() << "\n"
      << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  std::ostringstream ss;
  ss << R"(VariableConst{
    x_1
    none
    )"
     << GetParam().ast_type << "\n    {\n      Binary[not set]{"
     << "\n        " << GetParam().ast_lhs << "\n        " << GetParam().ast_op
     << "\n        " << GetParam().ast_rhs;
  auto got = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(got, HasSubstr(ss.str())) << "got:\n" << got << assembly;
}

// Use this when the result might have extra bitcasts on the outside.
struct BinaryDataGeneral {
  const std::string res_type;
  const std::string lhs;
  const std::string op;
  const std::string rhs;
  const std::string expected;
};
inline std::ostream& operator<<(std::ostream& out, BinaryDataGeneral data) {
  out << "BinaryDataGeneral{" << data.res_type << "," << data.lhs << ","
      << data.op << "," << data.rhs << "," << data.expected << "}";
  return out;
}

using SpvBinaryArithGeneralTest =
    SpvParserTestBase<::testing::TestWithParam<BinaryDataGeneral>>;

TEST_P(SpvBinaryArithGeneralTest, EmitExpression) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = )" + GetParam().op +
                        " %" + GetParam().res_type + " %" + GetParam().lhs +
                        " %" + GetParam().rhs + R"(
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions())
      << p->error() << "\n"
      << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  std::ostringstream ss;
  ss << R"(VariableConst{
    x_1
    none
    )"
     << GetParam().expected;
  auto got = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(got, HasSubstr(ss.str())) << "got:\n" << got << assembly;
}

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_IAdd,
    SpvBinaryArithTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpIAdd", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "add",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpIAdd", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "add",
                   "ScalarConstructor[not set]{40}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpIAdd", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "add",
                   AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpIAdd", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "add",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_IAdd_MixedSignedness,
    SpvBinaryArithGeneralTest,
    ::testing::Values(
        // Mixed, uint <- int uint
        BinaryDataGeneral{"uint", "int_30", "OpIAdd", "uint_10",
                          R"(__u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          add
          Bitcast[not set]<__i32>{
            ScalarConstructor[not set]{10}
          }
        }
      }
    })"},
        // Mixed, int <- int uint
        BinaryDataGeneral{"int", "int_30", "OpIAdd", "uint_10",
                          R"(__i32
    {
      Binary[not set]{
        ScalarConstructor[not set]{30}
        add
        Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, uint <- uint int
        BinaryDataGeneral{"uint", "uint_10", "OpIAdd", "int_30",
                          R"(__u32
    {
      Binary[not set]{
        ScalarConstructor[not set]{10}
        add
        Bitcast[not set]<__u32>{
          ScalarConstructor[not set]{30}
        }
      }
    })"},
        // Mixed, int <- uint uint
        BinaryDataGeneral{"int", "uint_20", "OpIAdd", "uint_10",
                          R"(__i32
    {
      Bitcast[not set]<__i32>{
        Binary[not set]{
          ScalarConstructor[not set]{20}
          add
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, returning v2uint
        BinaryDataGeneral{"v2uint", "v2int_30_40", "OpIAdd", "v2uint_10_20",
                          R"(__vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          add
          Bitcast[not set]<__vec_2__i32>{
            TypeConstructor[not set]{
              __vec_2__u32
              ScalarConstructor[not set]{10}
              ScalarConstructor[not set]{20}
            }
          }
        }
      }
    })"},
        // Mixed, returning v2int
        BinaryDataGeneral{"v2int", "v2uint_10_20", "OpIAdd", "v2int_40_30",
                          R"(__vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
          add
          Bitcast[not set]<__vec_2__u32>{
            TypeConstructor[not set]{
              __vec_2__i32
              ScalarConstructor[not set]{40}
              ScalarConstructor[not set]{30}
            }
          }
        }
      }
    })"}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_FAdd,
    SpvBinaryArithTest,
    ::testing::Values(
        // Scalar float
        BinaryData{"float", "float_50", "OpFAdd", "float_60", "__f32",
                   "ScalarConstructor[not set]{50.000000}", "add",
                   "ScalarConstructor[not set]{60.000000}"},
        // Vector float
        BinaryData{"v2float", "v2float_50_60", "OpFAdd", "v2float_60_50",
                   "__vec_2__f32", AstFor("v2float_50_60"), "add",
                   AstFor("v2float_60_50")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_ISub,
    SpvBinaryArithTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpISub", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "subtract",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpISub", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "subtract",
                   "ScalarConstructor[not set]{40}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpISub", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "subtract",
                   AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpISub", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "subtract",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_ISub_MixedSignedness,
    SpvBinaryArithGeneralTest,
    ::testing::Values(
        // Mixed, uint <- int uint
        BinaryDataGeneral{"uint", "int_30", "OpISub", "uint_10",
                          R"(__u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          subtract
          Bitcast[not set]<__i32>{
            ScalarConstructor[not set]{10}
          }
        }
      }
    })"},
        // Mixed, int <- int uint
        BinaryDataGeneral{"int", "int_30", "OpISub", "uint_10",
                          R"(__i32
    {
      Binary[not set]{
        ScalarConstructor[not set]{30}
        subtract
        Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, uint <- uint int
        BinaryDataGeneral{"uint", "uint_10", "OpISub", "int_30",
                          R"(__u32
    {
      Binary[not set]{
        ScalarConstructor[not set]{10}
        subtract
        Bitcast[not set]<__u32>{
          ScalarConstructor[not set]{30}
        }
      }
    })"},
        // Mixed, int <- uint uint
        BinaryDataGeneral{"int", "uint_20", "OpISub", "uint_10",
                          R"(__i32
    {
      Bitcast[not set]<__i32>{
        Binary[not set]{
          ScalarConstructor[not set]{20}
          subtract
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, returning v2uint
        BinaryDataGeneral{"v2uint", "v2int_30_40", "OpISub", "v2uint_10_20",
                          R"(__vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          subtract
          Bitcast[not set]<__vec_2__i32>{
            TypeConstructor[not set]{
              __vec_2__u32
              ScalarConstructor[not set]{10}
              ScalarConstructor[not set]{20}
            }
          }
        }
      }
    })"},
        // Mixed, returning v2int
        BinaryDataGeneral{"v2int", "v2uint_10_20", "OpISub", "v2int_40_30",
                          R"(__vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
          subtract
          Bitcast[not set]<__vec_2__u32>{
            TypeConstructor[not set]{
              __vec_2__i32
              ScalarConstructor[not set]{40}
              ScalarConstructor[not set]{30}
            }
          }
        }
      }
    })"}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_FSub,
    SpvBinaryArithTest,
    ::testing::Values(
        // Scalar float
        BinaryData{"float", "float_50", "OpFSub", "float_60", "__f32",
                   "ScalarConstructor[not set]{50.000000}", "subtract",
                   "ScalarConstructor[not set]{60.000000}"},
        // Vector float
        BinaryData{"v2float", "v2float_50_60", "OpFSub", "v2float_60_50",
                   "__vec_2__f32", AstFor("v2float_50_60"), "subtract",
                   AstFor("v2float_60_50")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_IMul,
    SpvBinaryArithTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpIMul", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "multiply",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpIMul", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "multiply",
                   "ScalarConstructor[not set]{40}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpIMul", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "multiply",
                   AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpIMul", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "multiply",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_IMul_MixedSignedness,
    SpvBinaryArithGeneralTest,
    ::testing::Values(
        // Mixed, uint <- int uint
        BinaryDataGeneral{"uint", "int_30", "OpIMul", "uint_10",
                          R"(__u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          multiply
          Bitcast[not set]<__i32>{
            ScalarConstructor[not set]{10}
          }
        }
      }
    })"},
        // Mixed, int <- int uint
        BinaryDataGeneral{"int", "int_30", "OpIMul", "uint_10",
                          R"(__i32
    {
      Binary[not set]{
        ScalarConstructor[not set]{30}
        multiply
        Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, uint <- uint int
        BinaryDataGeneral{"uint", "uint_10", "OpIMul", "int_30",
                          R"(__u32
    {
      Binary[not set]{
        ScalarConstructor[not set]{10}
        multiply
        Bitcast[not set]<__u32>{
          ScalarConstructor[not set]{30}
        }
      }
    })"},
        // Mixed, int <- uint uint
        BinaryDataGeneral{"int", "uint_20", "OpIMul", "uint_10",
                          R"(__i32
    {
      Bitcast[not set]<__i32>{
        Binary[not set]{
          ScalarConstructor[not set]{20}
          multiply
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, returning v2uint
        BinaryDataGeneral{"v2uint", "v2int_30_40", "OpIMul", "v2uint_10_20",
                          R"(__vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          multiply
          Bitcast[not set]<__vec_2__i32>{
            TypeConstructor[not set]{
              __vec_2__u32
              ScalarConstructor[not set]{10}
              ScalarConstructor[not set]{20}
            }
          }
        }
      }
    })"},
        // Mixed, returning v2int
        BinaryDataGeneral{"v2int", "v2uint_10_20", "OpIMul", "v2int_40_30",
                          R"(__vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
          multiply
          Bitcast[not set]<__vec_2__u32>{
            TypeConstructor[not set]{
              __vec_2__i32
              ScalarConstructor[not set]{40}
              ScalarConstructor[not set]{30}
            }
          }
        }
      }
    })"}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_FMul,
    SpvBinaryArithTest,
    ::testing::Values(
        // Scalar float
        BinaryData{"float", "float_50", "OpFMul", "float_60", "__f32",
                   "ScalarConstructor[not set]{50.000000}", "multiply",
                   "ScalarConstructor[not set]{60.000000}"},
        // Vector float
        BinaryData{"v2float", "v2float_50_60", "OpFMul", "v2float_60_50",
                   "__vec_2__f32", AstFor("v2float_50_60"), "multiply",
                   AstFor("v2float_60_50")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_UDiv,
    SpvBinaryArithTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpUDiv", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "divide",
                   "ScalarConstructor[not set]{20}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpUDiv", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "divide",
                   AstFor("v2uint_20_10")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_SDiv,
    SpvBinaryArithTest,
    ::testing::Values(
        // Both int
        BinaryData{"int", "int_30", "OpSDiv", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "divide",
                   "ScalarConstructor[not set]{40}"},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpSDiv", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "divide",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_SDiv_MixedSignednessOperands,
    SpvBinaryArithTest,
    ::testing::Values(
        // Mixed, returning int, second arg uint
        BinaryData{"int", "int_30", "OpSDiv", "uint_10", "__i32",
                   "ScalarConstructor[not set]{30}", "divide",
                   R"(Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        })"},
        // Mixed, returning int, first arg uint
        BinaryData{"int", "uint_10", "OpSDiv", "int_30", "__i32",
                   R"(Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        })",
                   "divide", "ScalarConstructor[not set]{30}"},
        // Mixed, returning v2int, first arg v2uint
        BinaryData{"v2int", "v2uint_10_20", "OpSDiv", "v2int_30_40",
                   "__vec_2__i32", AstFor("cast_int_v2uint_10_20"), "divide",
                   AstFor("v2int_30_40")},
        // Mixed, returning v2int, second arg v2uint
        BinaryData{"v2int", "v2int_30_40", "OpSDiv", "v2uint_10_20",
                   "__vec_2__i32", AstFor("v2int_30_40"), "divide",
                   AstFor("cast_int_v2uint_10_20")}));

TEST_F(SpvBinaryArithTestBasic, SDiv_Scalar_UnsignedResult) {
  // The WGSL signed division operator expects both operands to be signed
  // and the result is signed as well.
  // In this test SPIR-V demands an unsigned result, so we have to
  // wrap the result with an as-cast.
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSDiv %uint %int_30 %int_40
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions())
      << p->error() << "\n"
      << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          divide
          ScalarConstructor[not set]{40}
        }
      }
    }
  })"));
}

TEST_F(SpvBinaryArithTestBasic, SDiv_Vector_UnsignedResult) {
  // The WGSL signed division operator expects both operands to be signed
  // and the result is signed as well.
  // In this test SPIR-V demands an unsigned result, so we have to
  // wrap the result with an as-cast.
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSDiv %v2uint %v2int_30_40 %v2int_40_30
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions())
      << p->error() << "\n"
      << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          divide
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{40}
            ScalarConstructor[not set]{30}
          }
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_FDiv,
    SpvBinaryArithTest,
    ::testing::Values(
        // Scalar float
        BinaryData{"float", "float_50", "OpFDiv", "float_60", "__f32",
                   "ScalarConstructor[not set]{50.000000}", "divide",
                   "ScalarConstructor[not set]{60.000000}"},
        // Vector float
        BinaryData{"v2float", "v2float_50_60", "OpFDiv", "v2float_60_50",
                   "__vec_2__f32", AstFor("v2float_50_60"), "divide",
                   AstFor("v2float_60_50")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_UMod,
    SpvBinaryArithTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpUMod", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "modulo",
                   "ScalarConstructor[not set]{20}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpUMod", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "modulo",
                   AstFor("v2uint_20_10")}));

// Currently WGSL is missing a mapping for OpSRem
// https://github.com/gpuweb/gpuweb/issues/702

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_SMod,
    SpvBinaryArithTest,
    ::testing::Values(
        // Both int
        BinaryData{"int", "int_30", "OpSMod", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "modulo",
                   "ScalarConstructor[not set]{40}"},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpSMod", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "modulo",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_SMod_MixedSignednessOperands,
    SpvBinaryArithTest,
    ::testing::Values(
        // Mixed, returning int, second arg uint
        BinaryData{"int", "int_30", "OpSMod", "uint_10", "__i32",
                   "ScalarConstructor[not set]{30}", "modulo",
                   R"(Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        })"},
        // Mixed, returning int, first arg uint
        BinaryData{"int", "uint_10", "OpSMod", "int_30", "__i32",
                   R"(Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        })",
                   "modulo", "ScalarConstructor[not set]{30}"},
        // Mixed, returning v2int, first arg v2uint
        BinaryData{"v2int", "v2uint_10_20", "OpSMod", "v2int_30_40",
                   "__vec_2__i32", AstFor("cast_int_v2uint_10_20"), "modulo",
                   AstFor("v2int_30_40")},
        // Mixed, returning v2int, second arg v2uint
        BinaryData{"v2int", "v2int_30_40", "OpSMod", "v2uint_10_20",
                   "__vec_2__i32", AstFor("v2int_30_40"), "modulo",
                   AstFor("cast_int_v2uint_10_20")}));

TEST_F(SpvBinaryArithTestBasic, SMod_Scalar_UnsignedResult) {
  // The WGSL signed modulus operator expects both operands to be signed
  // and the result is signed as well.
  // In this test SPIR-V demands an unsigned result, so we have to
  // wrap the result with an as-cast.
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSMod %uint %int_30 %int_40
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions())
      << p->error() << "\n"
      << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          modulo
          ScalarConstructor[not set]{40}
        }
      }
    }
  })"));
}

TEST_F(SpvBinaryArithTestBasic, SMod_Vector_UnsignedResult) {
  // The WGSL signed modulus operator expects both operands to be signed
  // and the result is signed as well.
  // In this test SPIR-V demands an unsigned result, so we have to
  // wrap the result with an as-cast.
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpSMod %v2uint %v2int_30_40 %v2int_40_30
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions())
      << p->error() << "\n"
      << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          modulo
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{40}
            ScalarConstructor[not set]{30}
          }
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_FMod,
    SpvBinaryArithTest,
    ::testing::Values(
        // Scalar float
        BinaryData{"float", "float_50", "OpFMod", "float_60", "__f32",
                   "ScalarConstructor[not set]{50.000000}", "modulo",
                   "ScalarConstructor[not set]{60.000000}"},
        // Vector float
        BinaryData{"v2float", "v2float_50_60", "OpFMod", "v2float_60_50",
                   "__vec_2__f32", AstFor("v2float_50_60"), "modulo",
                   AstFor("v2float_60_50")}));

TEST_F(SpvBinaryArithTestBasic, VectorTimesScalar) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpCopyObject %v2float %v2float_50_60
     %2 = OpCopyObject %float %float_50
     %10 = OpVectorTimesScalar %v2float %1 %2
     OpReturn
     OpFunctionEnd
)";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(VariableConst{
    x_10
    none
    __vec_2__f32
    {
      Binary[not set]{
        Identifier[not set]{x_1}
        multiply
        Identifier[not set]{x_2}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvBinaryArithTestBasic, MatrixTimesScalar) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpCopyObject %m2v2float %m2v2float_a
     %2 = OpCopyObject %float %float_50
     %10 = OpMatrixTimesScalar %m2v2float %1 %2
     OpReturn
     OpFunctionEnd
)";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(VariableConst{
    x_10
    none
    __mat_2_2__f32
    {
      Binary[not set]{
        Identifier[not set]{x_1}
        multiply
        Identifier[not set]{x_2}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvBinaryArithTestBasic, VectorTimesMatrix) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpCopyObject %v2float %v2float_50_60
     %2 = OpCopyObject %m2v2float %m2v2float_a
     %10 = OpMatrixTimesVector %m2v2float %1 %2
     OpReturn
     OpFunctionEnd
)";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(VariableConst{
    x_10
    none
    __mat_2_2__f32
    {
      Binary[not set]{
        Identifier[not set]{x_1}
        multiply
        Identifier[not set]{x_2}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvBinaryArithTestBasic, MatrixTimesVector) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpCopyObject %m2v2float %m2v2float_a
     %2 = OpCopyObject %v2float %v2float_50_60
     %10 = OpMatrixTimesVector %m2v2float %1 %2
     OpReturn
     OpFunctionEnd
)";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(VariableConst{
    x_10
    none
    __mat_2_2__f32
    {
      Binary[not set]{
        Identifier[not set]{x_1}
        multiply
        Identifier[not set]{x_2}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvBinaryArithTestBasic, MatrixTimesMatrix) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpCopyObject %m2v2float %m2v2float_a
     %2 = OpCopyObject %m2v2float %m2v2float_b
     %10 = OpMatrixTimesMatrix %m2v2float %1 %2
     OpReturn
     OpFunctionEnd
)";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(VariableConst{
    x_10
    none
    __mat_2_2__f32
    {
      Binary[not set]{
        Identifier[not set]{x_1}
        multiply
        Identifier[not set]{x_2}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvBinaryArithTestBasic, Dot) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpCopyObject %v2float %v2float_50_60
     %2 = OpCopyObject %v2float %v2float_60_50
     %3 = OpDot %float %1 %2
     OpReturn
     OpFunctionEnd
)";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(R"(VariableConst{
    x_3
    none
    __f32
    {
      Call[not set]{
        Identifier[not set]{dot}
        (
          Identifier[not set]{x_1}
          Identifier[not set]{x_2}
        )
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvBinaryArithTestBasic, OuterProduct) {
  // OpOuterProduct is expanded to basic operations.
  // The operands, even if used once, are given their own const definitions.
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpFAdd %v3float %v3float_50_60_70 %v3float_50_60_70 ; column vector
     %2 = OpFAdd %v2float %v2float_60_50 %v2float_50_60 ; row vector
     %3 = OpOuterProduct %m2v3float %1 %2
     OpReturn
     OpFunctionEnd
)";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions()) << assembly;
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  auto got = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(got, HasSubstr(R"(VariableConst{
    x_3
    none
    __mat_3_2__f32
    {
      TypeConstructor[not set]{
        __mat_3_2__f32
        TypeConstructor[not set]{
          __vec_3__f32
          Binary[not set]{
            MemberAccessor[not set]{
              Identifier[not set]{x_2}
              Identifier[not set]{x}
            }
            multiply
            MemberAccessor[not set]{
              Identifier[not set]{x_1}
              Identifier[not set]{x}
            }
          }
          Binary[not set]{
            MemberAccessor[not set]{
              Identifier[not set]{x_2}
              Identifier[not set]{x}
            }
            multiply
            MemberAccessor[not set]{
              Identifier[not set]{x_1}
              Identifier[not set]{y}
            }
          }
          Binary[not set]{
            MemberAccessor[not set]{
              Identifier[not set]{x_2}
              Identifier[not set]{x}
            }
            multiply
            MemberAccessor[not set]{
              Identifier[not set]{x_1}
              Identifier[not set]{z}
            }
          }
        }
        TypeConstructor[not set]{
          __vec_3__f32
          Binary[not set]{
            MemberAccessor[not set]{
              Identifier[not set]{x_2}
              Identifier[not set]{y}
            }
            multiply
            MemberAccessor[not set]{
              Identifier[not set]{x_1}
              Identifier[not set]{x}
            }
          }
          Binary[not set]{
            MemberAccessor[not set]{
              Identifier[not set]{x_2}
              Identifier[not set]{y}
            }
            multiply
            MemberAccessor[not set]{
              Identifier[not set]{x_1}
              Identifier[not set]{y}
            }
          }
          Binary[not set]{
            MemberAccessor[not set]{
              Identifier[not set]{x_2}
              Identifier[not set]{y}
            }
            multiply
            MemberAccessor[not set]{
              Identifier[not set]{x_1}
              Identifier[not set]{z}
            }
          }
        }
      }
    }
  })"))
      << got;
}

// TODO(dneto): OpSRem. Missing from WGSL
// https://github.com/gpuweb/gpuweb/issues/702

// TODO(dneto): OpFRem. Missing from WGSL
// https://github.com/gpuweb/gpuweb/issues/702

// TODO(dneto): OpIAddCarry
// TODO(dneto): OpISubBorrow
// TODO(dneto): OpUMulExtended
// TODO(dneto): OpSMulExtended

}  // namespace
}  // namespace spirv
}  // namespace reader
}  // namespace tint
