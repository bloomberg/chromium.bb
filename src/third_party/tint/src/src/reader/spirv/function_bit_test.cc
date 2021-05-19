// Copyright 2020 The Tint Authors.  //
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

  %ptr_uint = OpTypePointer Function %uint
  %ptr_int = OpTypePointer Function %int
  %ptr_float = OpTypePointer Function %float

  %v2uint = OpTypeVector %uint 2
  %v2int = OpTypeVector %int 2
  %v2float = OpTypeVector %float 2

  %v2uint_10_20 = OpConstantComposite %v2uint %uint_10 %uint_20
  %v2uint_20_10 = OpConstantComposite %v2uint %uint_20 %uint_10
  %v2int_30_40 = OpConstantComposite %v2int %int_30 %int_40
  %v2int_40_30 = OpConstantComposite %v2int %int_40 %int_30
  %v2float_50_60 = OpConstantComposite %v2float %float_50 %float_60
  %v2float_60_50 = OpConstantComposite %v2float %float_60 %float_50
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

using SpvUnaryBitTest = SpvParserTestBase<::testing::Test>;

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

using SpvBinaryBitTest =
    SpvParserTestBase<::testing::TestWithParam<BinaryData>>;
using SpvBinaryBitTestBasic = SpvParserTestBase<::testing::Test>;

TEST_P(SpvBinaryBitTest, EmitExpression) {
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
  EXPECT_THAT(ToString(p->builder(), fe.ast_body()), HasSubstr(ss.str()))
      << assembly;
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

using SpvBinaryBitGeneralTest =
    SpvParserTestBase<::testing::TestWithParam<BinaryDataGeneral>>;

TEST_P(SpvBinaryBitGeneralTest, EmitExpression) {
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
    SpvParserTest_ShiftLeftLogical,
    SpvBinaryBitTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpShiftLeftLogical", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "shift_left",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpShiftLeftLogical", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "shift_left",
                   "ScalarConstructor[not set]{40}"},
        // Mixed, returning uint
        BinaryData{"uint", "int_30", "OpShiftLeftLogical", "uint_10", "__u32",
                   "ScalarConstructor[not set]{30}", "shift_left",
                   "ScalarConstructor[not set]{10}"},
        // Mixed, returning int
        BinaryData{"int", "int_30", "OpShiftLeftLogical", "uint_10", "__i32",
                   "ScalarConstructor[not set]{30}", "shift_left",
                   "ScalarConstructor[not set]{10}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpShiftLeftLogical",
                   "v2uint_20_10", "__vec_2__u32", AstFor("v2uint_10_20"),
                   "shift_left", AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpShiftLeftLogical", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "shift_left",
                   AstFor("v2int_40_30")},
        // Mixed, returning v2uint
        BinaryData{"v2uint", "v2int_30_40", "OpShiftLeftLogical",
                   "v2uint_10_20", "__vec_2__u32", AstFor("v2int_30_40"),
                   "shift_left", AstFor("v2uint_10_20")},
        // Mixed, returning v2int
        BinaryData{"v2int", "v2int_40_30", "OpShiftLeftLogical", "v2uint_20_10",
                   "__vec_2__i32", AstFor("v2int_40_30"), "shift_left",
                   AstFor("v2uint_20_10")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_ShiftRightLogical,
    SpvBinaryBitTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpShiftRightLogical", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "shift_right",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpShiftRightLogical", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "shift_right",
                   "ScalarConstructor[not set]{40}"},
        // Mixed, returning uint
        BinaryData{"uint", "int_30", "OpShiftRightLogical", "uint_10", "__u32",
                   "ScalarConstructor[not set]{30}", "shift_right",
                   "ScalarConstructor[not set]{10}"},
        // Mixed, returning int
        BinaryData{"int", "int_30", "OpShiftRightLogical", "uint_10", "__i32",
                   "ScalarConstructor[not set]{30}", "shift_right",
                   "ScalarConstructor[not set]{10}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpShiftRightLogical",
                   "v2uint_20_10", "__vec_2__u32", AstFor("v2uint_10_20"),
                   "shift_right", AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpShiftRightLogical", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "shift_right",
                   AstFor("v2int_40_30")},
        // Mixed, returning v2uint
        BinaryData{"v2uint", "v2int_30_40", "OpShiftRightLogical",
                   "v2uint_10_20", "__vec_2__u32", AstFor("v2int_30_40"),
                   "shift_right", AstFor("v2uint_10_20")},
        // Mixed, returning v2int
        BinaryData{"v2int", "v2int_40_30", "OpShiftRightLogical",
                   "v2uint_20_10", "__vec_2__i32", AstFor("v2int_40_30"),
                   "shift_right", AstFor("v2uint_20_10")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_ShiftRightArithmetic,
    SpvBinaryBitTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpShiftRightArithmetic", "uint_20",
                   "__u32", "ScalarConstructor[not set]{10}", "shift_right",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpShiftRightArithmetic", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "shift_right",
                   "ScalarConstructor[not set]{40}"},
        // Mixed, returning uint
        BinaryData{"uint", "int_30", "OpShiftRightArithmetic", "uint_10",
                   "__u32", "ScalarConstructor[not set]{30}", "shift_right",
                   "ScalarConstructor[not set]{10}"},
        // Mixed, returning int
        BinaryData{"int", "int_30", "OpShiftRightArithmetic", "uint_10",
                   "__i32", "ScalarConstructor[not set]{30}", "shift_right",
                   "ScalarConstructor[not set]{10}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpShiftRightArithmetic",
                   "v2uint_20_10", "__vec_2__u32", AstFor("v2uint_10_20"),
                   "shift_right", AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpShiftRightArithmetic",
                   "v2int_40_30", "__vec_2__i32", AstFor("v2int_30_40"),
                   "shift_right", AstFor("v2int_40_30")},
        // Mixed, returning v2uint
        BinaryData{"v2uint", "v2int_30_40", "OpShiftRightArithmetic",
                   "v2uint_10_20", "__vec_2__u32", AstFor("v2int_30_40"),
                   "shift_right", AstFor("v2uint_10_20")},
        // Mixed, returning v2int
        BinaryData{"v2int", "v2int_40_30", "OpShiftRightArithmetic",
                   "v2uint_20_10", "__vec_2__i32", AstFor("v2int_40_30"),
                   "shift_right", AstFor("v2uint_20_10")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_BitwiseAnd,
    SpvBinaryBitTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpBitwiseAnd", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "and",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpBitwiseAnd", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "and",
                   "ScalarConstructor[not set]{40}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpBitwiseAnd", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "and",
                   AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpBitwiseAnd", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "and",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_BitwiseAnd_MixedSignedness,
    SpvBinaryBitGeneralTest,
    ::testing::Values(
        // Mixed, uint <- int uint
        BinaryDataGeneral{"uint", "int_30", "OpBitwiseAnd", "uint_10",
                          R"(__u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          and
          Bitcast[not set]<__i32>{
            ScalarConstructor[not set]{10}
          }
        }
      }
    })"},
        // Mixed, int <- int uint
        BinaryDataGeneral{"int", "int_30", "OpBitwiseAnd", "uint_10",
                          R"(__i32
    {
      Binary[not set]{
        ScalarConstructor[not set]{30}
        and
        Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, uint <- uint int
        BinaryDataGeneral{"uint", "uint_10", "OpBitwiseAnd", "int_30",
                          R"(__u32
    {
      Binary[not set]{
        ScalarConstructor[not set]{10}
        and
        Bitcast[not set]<__u32>{
          ScalarConstructor[not set]{30}
        }
      }
    })"},
        // Mixed, int <- uint uint
        BinaryDataGeneral{"int", "uint_20", "OpBitwiseAnd", "uint_10",
                          R"(__i32
    {
      Bitcast[not set]<__i32>{
        Binary[not set]{
          ScalarConstructor[not set]{20}
          and
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, returning v2uint
        BinaryDataGeneral{"v2uint", "v2int_30_40", "OpBitwiseAnd",
                          "v2uint_10_20",
                          R"(__vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          and
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
        BinaryDataGeneral{"v2int", "v2uint_10_20", "OpBitwiseAnd",
                          "v2int_40_30",
                          R"(__vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
          and
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
    SpvParserTest_BitwiseOr,
    SpvBinaryBitTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpBitwiseOr", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "or",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpBitwiseOr", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "or",
                   "ScalarConstructor[not set]{40}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpBitwiseOr", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "or",
                   AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpBitwiseOr", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "or",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_BitwiseOr_MixedSignedness,
    SpvBinaryBitGeneralTest,
    ::testing::Values(
        // Mixed, uint <- int uint
        BinaryDataGeneral{"uint", "int_30", "OpBitwiseOr", "uint_10",
                          R"(__u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          or
          Bitcast[not set]<__i32>{
            ScalarConstructor[not set]{10}
          }
        }
      }
    })"},
        // Mixed, int <- int uint
        BinaryDataGeneral{"int", "int_30", "OpBitwiseOr", "uint_10",
                          R"(__i32
    {
      Binary[not set]{
        ScalarConstructor[not set]{30}
        or
        Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, uint <- uint int
        BinaryDataGeneral{"uint", "uint_10", "OpBitwiseOr", "int_30",
                          R"(__u32
    {
      Binary[not set]{
        ScalarConstructor[not set]{10}
        or
        Bitcast[not set]<__u32>{
          ScalarConstructor[not set]{30}
        }
      }
    })"},
        // Mixed, int <- uint uint
        BinaryDataGeneral{"int", "uint_20", "OpBitwiseOr", "uint_10",
                          R"(__i32
    {
      Bitcast[not set]<__i32>{
        Binary[not set]{
          ScalarConstructor[not set]{20}
          or
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, returning v2uint
        BinaryDataGeneral{"v2uint", "v2int_30_40", "OpBitwiseOr",
                          "v2uint_10_20",
                          R"(__vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          or
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
        BinaryDataGeneral{"v2int", "v2uint_10_20", "OpBitwiseOr", "v2int_40_30",
                          R"(__vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
          or
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
    SpvParserTest_BitwiseXor,
    SpvBinaryBitTest,
    ::testing::Values(
        // Both uint
        BinaryData{"uint", "uint_10", "OpBitwiseXor", "uint_20", "__u32",
                   "ScalarConstructor[not set]{10}", "xor",
                   "ScalarConstructor[not set]{20}"},
        // Both int
        BinaryData{"int", "int_30", "OpBitwiseXor", "int_40", "__i32",
                   "ScalarConstructor[not set]{30}", "xor",
                   "ScalarConstructor[not set]{40}"},
        // Both v2uint
        BinaryData{"v2uint", "v2uint_10_20", "OpBitwiseXor", "v2uint_20_10",
                   "__vec_2__u32", AstFor("v2uint_10_20"), "xor",
                   AstFor("v2uint_20_10")},
        // Both v2int
        BinaryData{"v2int", "v2int_30_40", "OpBitwiseXor", "v2int_40_30",
                   "__vec_2__i32", AstFor("v2int_30_40"), "xor",
                   AstFor("v2int_40_30")}));

INSTANTIATE_TEST_SUITE_P(
    SpvParserTest_BitwiseXor_MixedSignedness,
    SpvBinaryBitGeneralTest,
    ::testing::Values(
        // Mixed, uint <- int uint
        BinaryDataGeneral{"uint", "int_30", "OpBitwiseXor", "uint_10",
                          R"(__u32
    {
      Bitcast[not set]<__u32>{
        Binary[not set]{
          ScalarConstructor[not set]{30}
          xor
          Bitcast[not set]<__i32>{
            ScalarConstructor[not set]{10}
          }
        }
      }
    })"},
        // Mixed, int <- int uint
        BinaryDataGeneral{"int", "int_30", "OpBitwiseXor", "uint_10",
                          R"(__i32
    {
      Binary[not set]{
        ScalarConstructor[not set]{30}
        xor
        Bitcast[not set]<__i32>{
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, uint <- uint int
        BinaryDataGeneral{"uint", "uint_10", "OpBitwiseXor", "int_30",
                          R"(__u32
    {
      Binary[not set]{
        ScalarConstructor[not set]{10}
        xor
        Bitcast[not set]<__u32>{
          ScalarConstructor[not set]{30}
        }
      }
    })"},
        // Mixed, int <- uint uint
        BinaryDataGeneral{"int", "uint_20", "OpBitwiseXor", "uint_10",
                          R"(__i32
    {
      Bitcast[not set]<__i32>{
        Binary[not set]{
          ScalarConstructor[not set]{20}
          xor
          ScalarConstructor[not set]{10}
        }
      }
    })"},
        // Mixed, returning v2uint
        BinaryDataGeneral{"v2uint", "v2int_30_40", "OpBitwiseXor",
                          "v2uint_10_20",
                          R"(__vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__i32
            ScalarConstructor[not set]{30}
            ScalarConstructor[not set]{40}
          }
          xor
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
        BinaryDataGeneral{"v2int", "v2uint_10_20", "OpBitwiseXor",
                          "v2int_40_30",
                          R"(__vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Binary[not set]{
          TypeConstructor[not set]{
            __vec_2__u32
            ScalarConstructor[not set]{10}
            ScalarConstructor[not set]{20}
          }
          xor
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

TEST_F(SpvUnaryBitTest, Not_Int_Int) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %int %int_30
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
        not
        ScalarConstructor[not set]{30}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryBitTest, Not_Int_Uint) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %int %uint_10
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
      Bitcast[not set]<__i32>{
        UnaryOp[not set]{
          not
          ScalarConstructor[not set]{10}
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryBitTest, Not_Uint_Int) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %uint %int_30
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
          not
          ScalarConstructor[not set]{30}
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryBitTest, Not_Uint_Uint) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %uint %uint_10
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
      UnaryOp[not set]{
        not
        ScalarConstructor[not set]{10}
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

TEST_F(SpvUnaryBitTest, Not_SignedVec_SignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %v2int %v2int_30_40
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
        not
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

TEST_F(SpvUnaryBitTest, Not_SignedVec_UnsignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %v2int %v2uint_10_20
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
      Bitcast[not set]<__vec_2__i32>{
        UnaryOp[not set]{
          not
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

TEST_F(SpvUnaryBitTest, Not_UnsignedVec_SignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %v2uint %v2int_30_40
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
          not
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
TEST_F(SpvUnaryBitTest, Not_UnsignedVec_UnsignedVec) {
  const auto assembly = CommonTypes() + R"(
     %100 = OpFunction %void None %voidfn
     %entry = OpLabel
     %1 = OpNot %v2uint %v2uint_10_20
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
      UnaryOp[not set]{
        not
        TypeConstructor[not set]{
          __vec_2__u32
          ScalarConstructor[not set]{10}
          ScalarConstructor[not set]{20}
        }
      }
    }
  })"))
      << ToString(p->builder(), fe.ast_body());
}

std::string BitTestPreamble() {
  return R"(
  OpCapability Shader
  %glsl = OpExtInstImport "GLSL.std.450"
  OpMemoryModel Logical GLSL450
  OpEntryPoint GLCompute %100 "main"
  OpExecutionMode %100 LocalSize 1 1 1

  OpName %u1 "u1"
  OpName %i1 "i1"
  OpName %v2u1 "v2u1"
  OpName %v2i1 "v2i1"

)" + CommonTypes() +
         R"(

  %100 = OpFunction %void None %voidfn
  %entry = OpLabel

  %u1 = OpCopyObject %uint %uint_10
  %i1 = OpCopyObject %int %int_30
  %v2u1 = OpCopyObject %v2uint %v2uint_10_20
  %v2i1 = OpCopyObject %v2int %v2int_30_40
)";
}

TEST_F(SpvUnaryBitTest, BitCount_Uint_Uint) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %uint %u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Call[not set]{
        Identifier[not set]{countOneBits}
        (
          Identifier[not set]{u1}
        )
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitCount_Uint_Int) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %uint %i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Bitcast[not set]<__u32>{
        Call[not set]{
          Identifier[not set]{countOneBits}
          (
            Identifier[not set]{i1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitCount_Int_Uint) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %int %u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __i32
    {
      Bitcast[not set]<__i32>{
        Call[not set]{
          Identifier[not set]{countOneBits}
          (
            Identifier[not set]{u1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitCount_Int_Int) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %int %i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __i32
    {
      Call[not set]{
        Identifier[not set]{countOneBits}
        (
          Identifier[not set]{i1}
        )
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitCount_UintVector_UintVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %v2uint %v2u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Call[not set]{
        Identifier[not set]{countOneBits}
        (
          Identifier[not set]{v2u1}
        )
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitCount_UintVector_IntVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %v2uint %v2i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Call[not set]{
          Identifier[not set]{countOneBits}
          (
            Identifier[not set]{v2i1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitCount_IntVector_UintVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %v2int %v2u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Call[not set]{
          Identifier[not set]{countOneBits}
          (
            Identifier[not set]{v2u1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitCount_IntVector_IntVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitCount %v2int %v2i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__i32
    {
      Call[not set]{
        Identifier[not set]{countOneBits}
        (
          Identifier[not set]{v2i1}
        )
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_Uint_Uint) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %uint %u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Call[not set]{
        Identifier[not set]{reverseBits}
        (
          Identifier[not set]{u1}
        )
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_Uint_Int) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %uint %i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __u32
    {
      Bitcast[not set]<__u32>{
        Call[not set]{
          Identifier[not set]{reverseBits}
          (
            Identifier[not set]{i1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_Int_Uint) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %int %u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __i32
    {
      Bitcast[not set]<__i32>{
        Call[not set]{
          Identifier[not set]{reverseBits}
          (
            Identifier[not set]{u1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_Int_Int) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %int %i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __i32
    {
      Call[not set]{
        Identifier[not set]{reverseBits}
        (
          Identifier[not set]{i1}
        )
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_UintVector_UintVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %v2uint %v2u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Call[not set]{
        Identifier[not set]{reverseBits}
        (
          Identifier[not set]{v2u1}
        )
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_UintVector_IntVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %v2uint %v2i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__u32
    {
      Bitcast[not set]<__vec_2__u32>{
        Call[not set]{
          Identifier[not set]{reverseBits}
          (
            Identifier[not set]{v2i1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_IntVector_UintVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %v2int %v2u1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__i32
    {
      Bitcast[not set]<__vec_2__i32>{
        Call[not set]{
          Identifier[not set]{reverseBits}
          (
            Identifier[not set]{v2u1}
          )
        }
      }
    }
  })"))
      << body;
}

TEST_F(SpvUnaryBitTest, BitReverse_IntVector_IntVector) {
  const auto assembly = BitTestPreamble() + R"(
     %1 = OpBitReverse %v2int %v2i1
     OpReturn
     OpFunctionEnd
  )";
  auto p = parser(test::Assemble(assembly));
  ASSERT_TRUE(p->BuildAndParseInternalModuleExceptFunctions());
  FunctionEmitter fe(p.get(), *spirv_function(p.get(), 100));
  EXPECT_TRUE(fe.EmitBody()) << p->error();
  const auto body = ToString(p->builder(), fe.ast_body());
  EXPECT_THAT(body, HasSubstr(R"(
  VariableConst{
    x_1
    none
    __vec_2__i32
    {
      Call[not set]{
        Identifier[not set]{reverseBits}
        (
          Identifier[not set]{v2i1}
        )
      }
    }
  })"))
      << body;
}

// TODO(dneto): OpBitFieldInsert
// TODO(dneto): OpBitFieldSExtract
// TODO(dneto): OpBitFieldUExtract

}  // namespace
}  // namespace spirv
}  // namespace reader
}  // namespace tint
