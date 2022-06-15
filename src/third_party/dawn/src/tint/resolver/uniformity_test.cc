// Copyright 2022 The Tint Authors.
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
#include <string>
#include <tuple>
#include <utility>

#include "src/tint/program_builder.h"
#include "src/tint/reader/wgsl/parser.h"
#include "src/tint/resolver/uniformity.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::resolver {
namespace {

class UniformityAnalysisTestBase {
  protected:
    /// Parse and resolve a WGSL shader.
    /// @param src the WGSL source code
    /// @param should_pass true if `src` should pass the analysis, otherwise false
    void RunTest(std::string src, bool should_pass) {
        auto file = std::make_unique<Source::File>("test", src);
        auto program = reader::wgsl::Parse(file.get());

        diag::Formatter::Style style;
        style.print_newline_at_end = false;
        error_ = diag::Formatter(style).format(program.Diagnostics());

        bool valid = program.IsValid();
        if (should_pass) {
            EXPECT_TRUE(valid) << error_;
            if (program.Diagnostics().count() == 1u) {
                EXPECT_THAT(program.Diagnostics().str(), ::testing::HasSubstr("unreachable"));
            } else {
                EXPECT_EQ(program.Diagnostics().count(), 0u) << error_;
            }
        } else {
            // TODO(jrprice): expect false when uniformity issues become errors.
            EXPECT_TRUE(valid) << error_;
        }
    }

    /// Build and resolve a program from a ProgramBuilder object.
    /// @param builder the program builder
    /// @returns true on success, false on failure
    bool RunTest(ProgramBuilder&& builder) {
        auto program = Program(std::move(builder));

        diag::Formatter::Style style;
        style.print_newline_at_end = false;
        error_ = diag::Formatter(style).format(program.Diagnostics());

        return program.IsValid();
    }

    /// The error message from the parser or resolver, if any.
    std::string error_;
};

class UniformityAnalysisTest : public UniformityAnalysisTestBase, public ::testing::Test {};

class BasicTest : public UniformityAnalysisTestBase,
                  public ::testing::TestWithParam<std::tuple<int, int>> {
  public:
    /// Enum for the if-statement condition guarding a function call.
    enum Condition {
        // Uniform conditions:
        kTrue,
        kFalse,
        kLiteral,
        kModuleLet,
        kPipelineOverridable,
        kFuncLetUniformRhs,
        kFuncVarUniform,
        kFuncUniformRetVal,
        kUniformBuffer,
        kROStorageBuffer,
        kLastUniformCondition = kROStorageBuffer,
        // MayBeNonUniform conditions:
        kFuncLetNonUniformRhs,
        kFuncVarNonUniform,
        kFuncNonUniformRetVal,
        kRWStorageBuffer,
        // End of range marker:
        kEndOfConditionRange,
    };

    /// Enum for the function call statement.
    enum Function {
        // NoRestrictionFunctions:
        kUserNoRestriction,
        kMin,
        kTextureSampleLevel,
        kLastNoRestrictionFunction = kTextureSampleLevel,
        // RequiredToBeUniform functions:
        kUserRequiredToBeUniform,
        kWorkgroupBarrier,
        kStorageBarrier,
        kTextureSample,
        kTextureSampleBias,
        kTextureSampleCompare,
        kDpdx,
        kDpdxCoarse,
        kDpdxFine,
        kDpdy,
        kDpdyCoarse,
        kDpdyFine,
        kFwidth,
        kFwidthCoarse,
        kFwidthFine,
        // End of range marker:
        kEndOfFunctionRange,
    };

    /// Convert a condition to its string representation.
    static std::string ConditionToStr(Condition c) {
        switch (c) {
            case kTrue:
                return "true";
            case kFalse:
                return "false";
            case kLiteral:
                return "7 == 7";
            case kModuleLet:
                return "module_let == 0";
            case kPipelineOverridable:
                return "pipeline_overridable == 0";
            case kFuncLetUniformRhs:
                return "let_uniform_rhs == 0";
            case kFuncVarUniform:
                return "func_uniform == 0";
            case kFuncUniformRetVal:
                return "func_uniform_retval() == 0";
            case kUniformBuffer:
                return "u == 0";
            case kROStorageBuffer:
                return "ro == 0";
            case kFuncLetNonUniformRhs:
                return "let_nonuniform_rhs == 0";
            case kFuncVarNonUniform:
                return "func_non_uniform == 0";
            case kFuncNonUniformRetVal:
                return "func_nonuniform_retval() == 0";
            case kRWStorageBuffer:
                return "rw == 0";
            case kEndOfConditionRange:
                return "<invalid>";
        }
        return "<invalid>";
    }

    /// Convert a function call to its string representation.
    static std::string FunctionToStr(Function f) {
        switch (f) {
            case kUserNoRestriction:
                return "user_no_restriction()";
            case kMin:
                return "min(1, 1)";
            case kTextureSampleLevel:
                return "textureSampleLevel(t, s, vec2(0.5, 0.5), 0.0)";
            case kUserRequiredToBeUniform:
                return "user_required_to_be_uniform()";
            case kWorkgroupBarrier:
                return "workgroupBarrier()";
            case kStorageBarrier:
                return "storageBarrier()";
            case kTextureSample:
                return "textureSample(t, s, vec2(0.5, 0.5))";
            case kTextureSampleBias:
                return "textureSampleBias(t, s, vec2(0.5, 0.5), 2.0)";
            case kTextureSampleCompare:
                return "textureSampleCompare(td, sc, vec2(0.5, 0.5), 0.5)";
            case kDpdx:
                return "dpdx(1.0)";
            case kDpdxCoarse:
                return "dpdxCoarse(1.0)";
            case kDpdxFine:
                return "dpdxFine(1.0)";
            case kDpdy:
                return "dpdy(1.0)";
            case kDpdyCoarse:
                return "dpdyCoarse(1.0)";
            case kDpdyFine:
                return "dpdyFine(1.0)";
            case kFwidth:
                return "fwidth(1.0)";
            case kFwidthCoarse:
                return "fwidthCoarse(1.0)";
            case kFwidthFine:
                return "fwidthFine(1.0)";
            case kEndOfFunctionRange:
                return "<invalid>";
        }
        return "<invalid>";
    }

    /// @returns true if `c` is a condition that may be non-uniform.
    static bool MayBeNonUniform(Condition c) { return c > kLastUniformCondition; }

    /// @returns true if `f` is a function call that is required to be uniform.
    static bool RequiredToBeUniform(Function f) { return f > kLastNoRestrictionFunction; }

    /// Convert a test parameter pair of condition+function to a string that can be used as part of
    /// a test name.
    static std::string ParamsToName(::testing::TestParamInfo<ParamType> params) {
        Condition c = static_cast<Condition>(std::get<0>(params.param));
        Function f = static_cast<Function>(std::get<1>(params.param));
        std::string name;
#define CASE(c)     \
    case c:         \
        name += #c; \
        break

        switch (c) {
            CASE(kTrue);
            CASE(kFalse);
            CASE(kLiteral);
            CASE(kModuleLet);
            CASE(kPipelineOverridable);
            CASE(kFuncLetUniformRhs);
            CASE(kFuncVarUniform);
            CASE(kFuncUniformRetVal);
            CASE(kUniformBuffer);
            CASE(kROStorageBuffer);
            CASE(kFuncLetNonUniformRhs);
            CASE(kFuncVarNonUniform);
            CASE(kFuncNonUniformRetVal);
            CASE(kRWStorageBuffer);
            case kEndOfConditionRange:
                break;
        }
        name += "_";
        switch (f) {
            CASE(kUserNoRestriction);
            CASE(kMin);
            CASE(kTextureSampleLevel);
            CASE(kUserRequiredToBeUniform);
            CASE(kWorkgroupBarrier);
            CASE(kStorageBarrier);
            CASE(kTextureSample);
            CASE(kTextureSampleBias);
            CASE(kTextureSampleCompare);
            CASE(kDpdx);
            CASE(kDpdxCoarse);
            CASE(kDpdxFine);
            CASE(kDpdy);
            CASE(kDpdyCoarse);
            CASE(kDpdyFine);
            CASE(kFwidth);
            CASE(kFwidthCoarse);
            CASE(kFwidthFine);
            case kEndOfFunctionRange:
                break;
        }
#undef CASE

        return name;
    }
};

// Test the uniformity constraints for a function call inside a conditional statement.
TEST_P(BasicTest, ConditionalFunctionCall) {
    auto condition = static_cast<Condition>(std::get<0>(GetParam()));
    auto function = static_cast<Function>(std::get<1>(GetParam()));
    std::string src = R"(
var<private> p : i32;
var<workgroup> w : i32;
@group(0) @binding(0) var<uniform> u : i32;
@group(0) @binding(0) var<storage, read> ro : i32;
@group(0) @binding(0) var<storage, read_write> rw : i32;

@group(1) @binding(0) var t : texture_2d<f32>;
@group(1) @binding(1) var td : texture_depth_2d;
@group(1) @binding(2) var s : sampler;
@group(1) @binding(3) var sc : sampler_comparison;

let module_let : i32 = 42;
@id(42) override pipeline_overridable : i32;

fn user_no_restriction() {}
fn user_required_to_be_uniform() { workgroupBarrier(); }

fn func_uniform_retval() -> i32 { return u; }
fn func_nonuniform_retval() -> i32 { return rw; }

fn foo() {
  let let_uniform_rhs = 7;
  let let_nonuniform_rhs = rw;

  var func_uniform = 7;
  var func_non_uniform = 7;
  func_non_uniform = rw;

  if ()" + ConditionToStr(condition) +
                      R"() {
    )" + FunctionToStr(function) +
                      R"(;
  }
}
)";

    bool should_pass = !(MayBeNonUniform(condition) && RequiredToBeUniform(function));
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_THAT(error_, ::testing::StartsWith("test:31:5 warning: "));
        EXPECT_THAT(error_, ::testing::HasSubstr("must only be called from uniform control flow"));
    }
}

INSTANTIATE_TEST_SUITE_P(
    UniformityAnalysisTest,
    BasicTest,
    ::testing::Combine(::testing::Range<int>(0, BasicTest::kEndOfConditionRange),
                       ::testing::Range<int>(0, BasicTest::kEndOfFunctionRange)),
    BasicTest::ParamsToName);

////////////////////////////////////////////////////////////////////////////////
/// Test specific function and parameter tags that are not tested above.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, SubsequentControlFlowMayBeNonUniform_Pass) {
    // Call a function that causes subsequent control flow to be non-uniform, and then call another
    // function that doesn't require uniformity.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo() {
  if (rw == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn bar() {
  if (p == 42) {
    p = 7;
  }
}

fn main() {
  foo();
  bar();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, SubsequentControlFlowMayBeNonUniform_Fail) {
    // Call a function that causes subsequent control flow to be non-uniform, and then call another
    // function that requires uniformity.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo() {
  if (rw == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn main() {
  foo();
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:17:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:16:3 note: calling 'foo' may cause subsequent control flow to be non-uniform
  foo();
  ^^^

test:7:3 note: control flow depends on non-uniform value
  if (rw == 0) {
  ^^

test:7:7 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  if (rw == 0) {
      ^^
)");
}

TEST_F(UniformityAnalysisTest, SubsequentControlFlowMayBeNonUniform_Nested_Fail) {
    // Indirectly call a function that causes subsequent control flow to be non-uniform, and then
    // call another function that requires uniformity.
    // The lack of return statement in `foo()` requires that we implicitly add an edge from
    // CF_return to that last control flow node of the function.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn bar() {
  if (rw == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn foo() {
  bar();
}

fn main() {
  foo();
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:21:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:20:3 note: calling 'foo' may cause subsequent control flow to be non-uniform
  foo();
  ^^^

test:16:3 note: calling 'bar' may cause subsequent control flow to be non-uniform
  bar();
  ^^^

test:7:3 note: control flow depends on non-uniform value
  if (rw == 0) {
  ^^

test:7:7 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  if (rw == 0) {
      ^^
)");
}

TEST_F(UniformityAnalysisTest, ParameterNoRestriction_Pass) {
    // Pass a non-uniform value as an argument, and then try to use the return value for
    // control-flow guarding a barrier.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo(i : i32) -> i32 {
  if (i == 0) {
    // This assignment is non-uniform, but shouldn't affect the return value.
    p = 42;
  }
  return 7;
}

fn bar() {
  let x = foo(rw);
  if (x == 7) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniform_Pass) {
    // Pass a uniform value as an argument to a function that uses that parameter for control-flow
    // guarding a barrier.
    std::string src = R"(
@group(0) @binding(0) var<storage, read> ro : i32;

fn foo(i : i32) {
  if (i == 0) {
    workgroupBarrier();
  }
}

fn bar() {
  foo(ro);
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniform_Fail) {
    // Pass a non-uniform value as an argument to a function that uses that parameter for
    // control-flow guarding a barrier.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo(i : i32) {
  if (i == 0) {
    workgroupBarrier();
  }
}

fn bar() {
  foo(rw);
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:7 warning: parameter 'i' of 'foo' must be uniform
  foo(rw);
      ^^

test:6:5 note: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:7 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  foo(rw);
      ^^
)");
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForReturnValue_Pass) {
    // Pass a uniform value as an argument to a function that uses that parameter to produce the
    // return value, and then use the return value for control-flow guarding a barrier.
    std::string src = R"(
@group(0) @binding(0) var<storage, read> ro : i32;

fn foo(i : i32) -> i32 {
  return 1 + i;
}

fn bar() {
  if (foo(ro) == 7) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForReturnValue_Fail) {
    // Pass a non-uniform value as an argument to a function that uses that parameter to produce the
    // return value, and then use the return value for control-flow guarding a barrier.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo(i : i32) -> i32 {
  return 1 + i;
}

fn bar() {
  if (foo(rw) == 7) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:9:3 note: control flow depends on non-uniform value
  if (foo(rw) == 7) {
  ^^

test:9:11 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  if (foo(rw) == 7) {
          ^^
)");
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForSubsequentControlFlow_Pass) {
    // Pass a uniform value as an argument to a function that uses that parameter return early, and
    // then invoke a barrier after calling that function.
    std::string src = R"(
@group(0) @binding(0) var<storage, read> ro : i32;

var<private> p : i32;

fn foo(i : i32) {
  if (i == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn bar() {
  foo(ro);
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ParameterRequiredToBeUniformForSubsequentControlFlow_Fail) {
    // Pass a non-uniform value as an argument to a function that uses that parameter return early,
    // and then invoke a barrier after calling that function.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

var<private> p : i32;

fn foo(i : i32) {
  if (i == 0) {
    p = 42;
    return;
  }
  p = 5;
  return;
}

fn bar() {
  foo(rw);
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:17:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:16:7 note: non-uniform function call argument causes subsequent control flow to be non-uniform
  foo(rw);
      ^^

test:7:3 note: control flow depends on non-uniform value
  if (i == 0) {
  ^^

test:7:7 note: reading from 'i' may result in a non-uniform value
  if (i == 0) {
      ^

test:16:7 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  foo(rw);
      ^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Test shader IO attributes.
////////////////////////////////////////////////////////////////////////////////

struct BuiltinEntry {
    std::string name;
    std::string type;
    bool uniform;
    BuiltinEntry(std::string n, std::string t, bool u) : name(n), type(t), uniform(u) {}
};

class ComputeBuiltin : public UniformityAnalysisTestBase,
                       public ::testing::TestWithParam<BuiltinEntry> {};
TEST_P(ComputeBuiltin, AsParam) {
    std::string src = R"(
@compute @workgroup_size(64)
fn main(@builtin()" + GetParam().name +
                      R"() b : )" + GetParam().type + R"() {
  if (all(vec3(b) == vec3(0u))) {
    workgroupBarrier();
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(
            error_,
            R"(test:5:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:4:3 note: control flow depends on non-uniform value
  if (all(vec3(b) == vec3(0u))) {
  ^^

test:4:16 note: reading from builtin 'b' may result in a non-uniform value
  if (all(vec3(b) == vec3(0u))) {
               ^
)");
    }
}

TEST_P(ComputeBuiltin, InStruct) {
    std::string src = R"(
struct S {
  @builtin()" + GetParam().name +
                      R"() b : )" + GetParam().type + R"(
}

@compute @workgroup_size(64)
fn main(s : S) {
  if (all(vec3(s.b) == vec3(0u))) {
    workgroupBarrier();
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(
            error_,
            R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (all(vec3(s.b) == vec3(0u))) {
  ^^

test:8:16 note: reading from 's' may result in a non-uniform value
  if (all(vec3(s.b) == vec3(0u))) {
               ^
)");
    }
}

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         ComputeBuiltin,
                         ::testing::Values(BuiltinEntry{"local_invocation_id", "vec3<u32>", false},
                                           BuiltinEntry{"local_invocation_index", "u32", false},
                                           BuiltinEntry{"global_invocation_id", "vec3<u32>", false},
                                           BuiltinEntry{"workgroup_id", "vec3<u32>", true},
                                           BuiltinEntry{"num_workgroups", "vec3<u32>", true}),
                         [](const ::testing::TestParamInfo<ComputeBuiltin::ParamType>& p) {
                             return p.param.name;
                         });

TEST_F(UniformityAnalysisTest, ComputeBuiltin_MixedAttributesInStruct) {
    // Mix both non-uniform and uniform shader IO attributes in the same structure. Even accessing
    // just uniform member causes non-uniformity in this case.
    std::string src = R"(
struct S {
  @builtin(num_workgroups) num_groups : vec3<u32>,
  @builtin(local_invocation_index) idx : u32,
}

@compute @workgroup_size(64)
fn main(s : S) {
  if (s.num_groups.x == 0u) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:9:3 note: control flow depends on non-uniform value
  if (s.num_groups.x == 0u) {
  ^^

test:9:7 note: reading from 's' may result in a non-uniform value
  if (s.num_groups.x == 0u) {
      ^
)");
}

class FragmentBuiltin : public UniformityAnalysisTestBase,
                        public ::testing::TestWithParam<BuiltinEntry> {};
TEST_P(FragmentBuiltin, AsParam) {
    std::string src = R"(
@fragment
fn main(@builtin()" + GetParam().name +
                      R"() b : )" + GetParam().type + R"() {
  if (u32(vec4(b).x) == 0u) {
    dpdx(0.5);
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(error_,
                  R"(test:5:5 warning: 'dpdx' must only be called from uniform control flow
    dpdx(0.5);
    ^^^^

test:4:3 note: control flow depends on non-uniform value
  if (u32(vec4(b).x) == 0u) {
  ^^

test:4:16 note: reading from builtin 'b' may result in a non-uniform value
  if (u32(vec4(b).x) == 0u) {
               ^
)");
    }
}

TEST_P(FragmentBuiltin, InStruct) {
    std::string src = R"(
struct S {
  @builtin()" + GetParam().name +
                      R"() b : )" + GetParam().type + R"(
}

@fragment
fn main(s : S) {
  if (u32(vec4(s.b).x) == 0u) {
    dpdx(0.5);
  }
}
)";

    bool should_pass = GetParam().uniform;
    RunTest(src, should_pass);
    if (!should_pass) {
        EXPECT_EQ(error_,
                  R"(test:9:5 warning: 'dpdx' must only be called from uniform control flow
    dpdx(0.5);
    ^^^^

test:8:3 note: control flow depends on non-uniform value
  if (u32(vec4(s.b).x) == 0u) {
  ^^

test:8:16 note: reading from 's' may result in a non-uniform value
  if (u32(vec4(s.b).x) == 0u) {
               ^
)");
    }
}

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         FragmentBuiltin,
                         ::testing::Values(BuiltinEntry{"position", "vec4<f32>", false},
                                           BuiltinEntry{"front_facing", "bool", false},
                                           BuiltinEntry{"sample_index", "u32", false},
                                           BuiltinEntry{"sample_mask", "u32", false}),
                         [](const ::testing::TestParamInfo<FragmentBuiltin::ParamType>& p) {
                             return p.param.name;
                         });

TEST_F(UniformityAnalysisTest, FragmentLocation) {
    std::string src = R"(
@fragment
fn main(@location(0) l : f32) {
  if (l == 0.0) {
    dpdx(0.5);
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:5:5 warning: 'dpdx' must only be called from uniform control flow
    dpdx(0.5);
    ^^^^

test:4:3 note: control flow depends on non-uniform value
  if (l == 0.0) {
  ^^

test:4:7 note: reading from user-defined input 'l' may result in a non-uniform value
  if (l == 0.0) {
      ^
)");
}

TEST_F(UniformityAnalysisTest, FragmentLocation_InStruct) {
    std::string src = R"(
struct S {
  @location(0) l : f32
}

@fragment
fn main(s : S) {
  if (s.l == 0.0) {
    dpdx(0.5);
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'dpdx' must only be called from uniform control flow
    dpdx(0.5);
    ^^^^

test:8:3 note: control flow depends on non-uniform value
  if (s.l == 0.0) {
  ^^

test:8:7 note: reading from 's' may result in a non-uniform value
  if (s.l == 0.0) {
      ^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Test loop conditions and conditional break/continue statements.
////////////////////////////////////////////////////////////////////////////////

namespace LoopTest {

enum ControlFlowInterrupt {
    kBreak,
    kContinue,
    kReturn,
    kDiscard,
};
enum Condition {
    kNone,
    kUniform,
    kNonUniform,
};

using LoopTestParams = std::tuple<int, int>;

static std::string ToStr(ControlFlowInterrupt interrupt) {
    switch (interrupt) {
        case kBreak:
            return "break";
        case kContinue:
            return "continue";
        case kReturn:
            return "return";
        case kDiscard:
            return "discard";
    }
    return "";
}

static std::string ToStr(Condition condition) {
    switch (condition) {
        case kNone:
            return "uncondtiional";
        case kUniform:
            return "uniform";
        case kNonUniform:
            return "nonuniform";
    }
    return "";
}

class LoopTest : public UniformityAnalysisTestBase,
                 public ::testing::TestWithParam<LoopTestParams> {
  protected:
    std::string MakeInterrupt(ControlFlowInterrupt interrupt, Condition condition) {
        switch (condition) {
            case kNone:
                return ToStr(interrupt);
            case kUniform:
                return "if (uniform_var == 42) { " + ToStr(interrupt) + "; }";
            case kNonUniform:
                return "if (nonuniform_var == 42) { " + ToStr(interrupt) + "; }";
        }
        return "<invalid>";
    }
};

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         LoopTest,
                         ::testing::Combine(::testing::Range<int>(0, kDiscard + 1),
                                            ::testing::Range<int>(0, kNonUniform + 1)),
                         [](const ::testing::TestParamInfo<LoopTestParams>& p) {
                             ControlFlowInterrupt interrupt =
                                 static_cast<ControlFlowInterrupt>(std::get<0>(p.param));
                             auto condition = static_cast<Condition>(std::get<1>(p.param));
                             return ToStr(interrupt) + "_" + ToStr(condition);
                         });

TEST_P(LoopTest, CallInBody_InterruptAfter) {
    // Test control-flow interrupt in a loop after a function call that requires uniform control
    // flow.
    auto interrupt = static_cast<ControlFlowInterrupt>(std::get<0>(GetParam()));
    auto condition = static_cast<Condition>(std::get<1>(GetParam()));
    std::string src = R"(
@group(0) @binding(0) var<storage, read> uniform_var : i32;
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn foo() {
  loop {
    // Pretend that this isn't an infinite loop, in case the interrupt is a
    // continue statement.
    if (false) {
      break;
    }

    workgroupBarrier();
    )" + MakeInterrupt(interrupt, condition) +
                      R"(;
  }
}
)";

    if (condition == kNonUniform) {
        RunTest(src, false);
        EXPECT_THAT(
            error_,
            ::testing::StartsWith(
                R"(test:13:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();)"));
        EXPECT_THAT(error_,
                    ::testing::HasSubstr("test:14:9 note: reading from read_write storage buffer "
                                         "'nonuniform_var' may result in a non-uniform value"));
    } else {
        RunTest(src, true);
    }
}

TEST_P(LoopTest, CallInBody_InterruptBefore) {
    // Test control-flow interrupt in a loop before a function call that requires uniform control
    // flow.
    auto interrupt = static_cast<ControlFlowInterrupt>(std::get<0>(GetParam()));
    auto condition = static_cast<Condition>(std::get<1>(GetParam()));
    std::string src = R"(
@group(0) @binding(0) var<storage, read> uniform_var : i32;
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn foo() {
  loop {
    // Pretend that this isn't an infinite loop, in case the interrupt is a
    // continue statement.
    if (false) {
      break;
    }

    )" + MakeInterrupt(interrupt, condition) +
                      R"(;
    workgroupBarrier();
  }
}
)";

    if (condition == kNonUniform) {
        RunTest(src, false);

        EXPECT_THAT(
            error_,
            ::testing::StartsWith(
                R"(test:14:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();)"));
        EXPECT_THAT(error_,
                    ::testing::HasSubstr("test:13:9 note: reading from read_write storage buffer "
                                         "'nonuniform_var' may result in a non-uniform value"));
    } else {
        RunTest(src, true);
    }
}

TEST_P(LoopTest, CallInContinuing_InterruptInBody) {
    // Test control-flow interrupt in a loop with a function call that requires uniform control flow
    // in the continuing statement.
    auto interrupt = static_cast<ControlFlowInterrupt>(std::get<0>(GetParam()));
    auto condition = static_cast<Condition>(std::get<1>(GetParam()));
    std::string src = R"(
@group(0) @binding(0) var<storage, read> uniform_var : i32;
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn foo() {
  loop {
    // Pretend that this isn't an infinite loop, in case the interrupt is a
    // continue statement.
    if (false) {
      break;
    }

    )" + MakeInterrupt(interrupt, condition) +
                      R"(;
    continuing {
      workgroupBarrier();
    }
  }
}
)";

    if (condition == kNonUniform) {
        RunTest(src, false);
        EXPECT_THAT(
            error_,
            ::testing::StartsWith(
                R"(test:15:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();)"));
        EXPECT_THAT(error_,
                    ::testing::HasSubstr("test:13:9 note: reading from read_write storage buffer "
                                         "'nonuniform_var' may result in a non-uniform value"));
    } else {
        RunTest(src, true);
    }
}

TEST_F(UniformityAnalysisTest, Loop_CallInBody_UniformBreakInContinuing) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> n : i32;

fn foo() {
  var i = 0;
  loop {
    workgroupBarrier();
    continuing {
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_CallInBody_NonUniformBreakInContinuing) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  var i = 0;
  loop {
    workgroupBarrier();
    continuing {
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:10:7 note: control flow depends on non-uniform value
      if (i == n) {
      ^^

test:10:16 note: reading from read_write storage buffer 'n' may result in a non-uniform value
      if (i == n) {
               ^
)");
}

TEST_F(UniformityAnalysisTest, Loop_CallInContinuing_UniformBreakInContinuing) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> n : i32;

fn foo() {
  var i = 0;
  loop {
    continuing {
      workgroupBarrier();
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_CallInContinuing_NonUniformBreakInContinuing) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  var i = 0;
  loop {
    continuing {
      workgroupBarrier();
      i = i + 1;
      if (i == n) {
        break;
      }
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:10:7 note: control flow depends on non-uniform value
      if (i == n) {
      ^^

test:10:16 note: reading from read_write storage buffer 'n' may result in a non-uniform value
      if (i == n) {
               ^
)");
}

class LoopDeadCodeTest : public UniformityAnalysisTestBase, public ::testing::TestWithParam<int> {};

INSTANTIATE_TEST_SUITE_P(UniformityAnalysisTest,
                         LoopDeadCodeTest,
                         ::testing::Range<int>(0, kDiscard + 1),
                         [](const ::testing::TestParamInfo<LoopDeadCodeTest::ParamType>& p) {
                             return ToStr(static_cast<ControlFlowInterrupt>(p.param));
                         });

TEST_P(LoopDeadCodeTest, AfterInterrupt) {
    // Dead code after a control-flow interrupt in a loop shouldn't cause uniformity errors.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  loop {
    )" + ToStr(static_cast<ControlFlowInterrupt>(GetParam())) +
                      R"(;
    if (n == 42) {
      workgroupBarrier();
    }
    continuing {
      // Pretend that this isn't an infinite loop, in case the interrupt is a
      // continue statement.
      if (false) {
        break;
      }
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_VarBecomesNonUniformInLoopAfterBarrier) {
    // Use a variable for a conditional barrier in a loop, and then assign a non-uniform value to
    // that variable later in that loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    v = non_uniform;
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:12:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_VarBecomesNonUniformInLoopAfterBarrier_BreakAtEnd) {
    // Use a variable for a conditional barrier in a loop, and then assign a non-uniform value to
    // that variable later in that loop. End the loop with a break statement to prevent the
    // non-uniform value from causing an issue.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (v == 0) {
      workgroupBarrier();
    }

    v = non_uniform;
    break;
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_ConditionalAssignNonUniformWithBreak_BarrierInLoop) {
    // In a conditional block, assign a non-uniform value and then break, then use a variable for a
    // conditional barrier later in the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (true) {
      v = non_uniform;
      break;
    }
    if (v == 0) {
      workgroupBarrier();
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_ConditionalAssignNonUniformWithConditionalBreak_BarrierInLoop) {
    // In a conditional block, assign a non-uniform value and then conditionally break, then use a
    // variable for a conditional barrier later in the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (true) {
      v = non_uniform;
      if (true) {
        break;
      }
    }
    if (v == 0) {
      workgroupBarrier();
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:13:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:8:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_ConditionalAssignNonUniformWithBreak_BarrierAfterLoop) {
    // In a conditional block, assign a non-uniform value and then break, then use a variable for a
    // conditional barrier after the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (true) {
      v = non_uniform;
      break;
    }
    v = 5;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:14:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:8:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_VarBecomesUniformBeforeSomeExits_BarrierAfterLoop) {
    // Assign a non-uniform value, have two exit points only one of which assigns a uniform value,
    // then use a variable for a conditional barrier after the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (true) {
      break;
    }

    v = non_uniform;

    if (false) {
      v = 6;
      break;
    }
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:20:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:19:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:11:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_VarBecomesUniformBeforeAllExits_BarrierAfterLoop) {
    // Assign a non-uniform value, have two exit points both of which assigns a uniform value,
    // then use a variable for a conditional barrier after the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (true) {
      v = 5;
      break;
    }

    v = non_uniform;

    if (false) {
      v = 6;
      break;
    }
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_AssignNonUniformBeforeConditionalBreak_BarrierAfterLoop) {
    // Assign a non-uniform value and then break in a conditional block, then use a variable for a
    // conditional barrier after the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    v = non_uniform;
    if (true) {
      if (false) {
        v = 5;
      } else {
        break;
      }
      v = 5;
    }
    v = 5;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:20:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:19:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:7:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_VarBecomesNonUniformBeforeConditionalContinue_BarrierAtStart) {
    // Use a variable for a conditional barrier in a loop, assign a non-uniform value to
    // that variable later in that loop, then perform a conditional continue before assigning a
    // uniform value to that variable.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    v = non_uniform;
    if (true) {
      continue;
    }

    v = 5;
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:12:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest,
       Loop_VarBecomesUniformBeforeConditionalContinue_BarrierInContinuing) {
    // Use a variable for a conditional barrier in the continuing statement of a loop, assign a
    // non-uniform value to that variable later in that loop, then conditionally assign a uniform
    // value before continuing.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    v = non_uniform;

    if (false) {
      v = 5;
      continue;
    }

    continuing {
      if (v == 0) {
        workgroupBarrier();
      }
      if (true) {
        break;
      }
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:16:9 warning: 'workgroupBarrier' must only be called from uniform control flow
        workgroupBarrier();
        ^^^^^^^^^^^^^^^^

test:15:7 note: control flow depends on non-uniform value
      if (v == 0) {
      ^^

test:7:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_VarBecomesNonUniformBeforeConditionalContinue) {
    // Use a variable for a conditional barrier in a loop, assign a non-uniform value to
    // that variable later in that loop, then perform a conditional continue before assigning a
    // uniform value to that variable.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    v = non_uniform;
    if (true) {
      continue;
    }

    v = 5;
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:12:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Loop_VarBecomesNonUniformInNestedLoopWithBreak_BarrierInLoop) {
    // Use a variable for a conditional barrier in a loop, then conditionally assign a non-uniform
    // value to that variable followed by a break in a nested loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    loop {
      if (true) {
        v = non_uniform;
        break;
      }
      v = 5;
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:14:13 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
        v = non_uniform;
            ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest,
       Loop_VarBecomesNonUniformInNestedLoopWithBreak_BecomesUniformAgain_BarrierAfterLoop) {
    // Conditionally assign a non-uniform value followed by a break in a nested loop, assign a
    // uniform value in the outer loop, and then use a variable for a conditional barrier after the
    // loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  loop {
    if (false) {
      break;
    }

    loop {
      if (true) {
        v = non_uniform;
        break;
      }
    }
    v = 5;
  }
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_NonUniformValueNeverReachesContinuing) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  loop {
    var v = non_uniform;
    return;

    continuing {
      if (v == 0) {
        workgroupBarrier();
      }
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_NonUniformBreakInBody_Reconverge) {
    // Loops reconverge at exit, so test that we can call workgroupBarrier() after a loop that
    // contains a non-uniform conditional break.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  var i = 0;
  loop {
    if (i == n) {
      break;
    }
    i = i + 1;
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_NonUniformFunctionInBody_Reconverge) {
    // Loops reconverge at exit, so test that we can call workgroupBarrier() after a loop that
    // contains a call to a function that causes non-uniform control flow.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    return;
  } else {
    return;
  }
}

fn foo() {
  loop {
    bar();
    break;
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Loop_NonUniformFunctionDiscard_NoReconvergence) {
    // Loops should not reconverge after non-uniform discard statements.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    discard;
  }
}

fn foo() {
  loop {
    bar();
    break;
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:12:5 note: calling 'bar' may cause subsequent control flow to be non-uniform
    bar();
    ^^^

test:5:3 note: control flow depends on non-uniform value
  if (n == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'n' may result in a non-uniform value
  if (n == 42) {
      ^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_UniformCondition) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> n : i32;

fn foo() {
  for (var i = 0; i < n; i = i + 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_NonUniformCondition) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  for (var i = 0; i < n; i = i + 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  for (var i = 0; i < n; i = i + 1) {
  ^^^

test:5:23 note: reading from read_write storage buffer 'n' may result in a non-uniform value
  for (var i = 0; i < n; i = i + 1) {
                      ^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_InitializerCausesNonUniformFlow) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() -> i32 {
  if (n == 42) {
    return 1;
  } else {
    return 2;
  }
}

fn foo() {
  for (var i = bar(); i < 10; i = i + 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:16 note: calling 'bar' may cause subsequent control flow to be non-uniform
  for (var i = bar(); i < 10; i = i + 1) {
               ^^^

test:5:3 note: control flow depends on non-uniform value
  if (n == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'n' may result in a non-uniform value
  if (n == 42) {
      ^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_CallInside_ContinuingCausesNonUniformFlow) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() -> i32 {
  if (n == 42) {
    return 1;
  } else {
    return 2;
  }
}

fn foo() {
  for (var i = 0; i < 10; i = i + bar()) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:35 note: calling 'bar' may cause subsequent control flow to be non-uniform
  for (var i = 0; i < 10; i = i + bar()) {
                                  ^^^

test:5:3 note: control flow depends on non-uniform value
  if (n == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'n' may result in a non-uniform value
  if (n == 42) {
      ^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_VarBecomesNonUniformInContinuing_BarrierInLoop) {
    // Use a variable for a conditional barrier in a loop, and then assign a non-uniform value to
    // that variable in the continuing statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; v = non_uniform) {
    if (v == 0) {
      workgroupBarrier();
      break;
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:6:31 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  for (var i = 0; i < 10; v = non_uniform) {
                              ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_VarBecomesUniformInContinuing_BarrierInLoop) {
    // Use a variable for a conditional barrier in a loop, and then assign a uniform value to that
    // variable in the continuing statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; v = 5) {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    v = non_uniform;
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ForLoop_VarBecomesNonUniformInContinuing_BarrierAfterLoop) {
    // Use a variable for a conditional barrier after a loop, and assign a non-uniform value to
    // that variable in the continuing statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; v = non_uniform) {
    v = 5;
  }
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:9:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:6:31 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  for (var i = 0; i < 10; v = non_uniform) {
                              ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_VarBecomesUniformInContinuing_BarrierAfterLoop) {
    // Use a variable for a conditional barrier after a loop, and assign a uniform value to that
    // variable in the continuing statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; v = 5) {
    v = non_uniform;
  }
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ForLoop_VarBecomesNonUniformInLoopAfterBarrier) {
    // Use a variable for a conditional barrier in a loop, and then assign a non-uniform value to
    // that variable later in that loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; i++) {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    v = non_uniform;
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:12:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_ConditionalAssignNonUniformWithBreak_BarrierInLoop) {
    // In a conditional block, assign a non-uniform value and then break, then use a variable for a
    // conditional barrier later in the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; i++) {
    if (true) {
      v = non_uniform;
      break;
    }
    if (v == 0) {
      workgroupBarrier();
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ForLoop_ConditionalAssignNonUniformWithBreak_BarrierAfterLoop) {
    // In a conditional block, assign a non-uniform value and then break, then use a variable for a
    // conditional barrier after the loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; i++) {
    if (true) {
      v = non_uniform;
      break;
    }
    v = 5;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:14:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:8:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_VarRemainsNonUniformAtLoopEnd_BarrierAfterLoop) {
    // Assign a non-uniform value, assign a uniform value before all explicit break points but leave
    // the value non-uniform at loop exit, then use a variable for a conditional barrier after the
    // loop.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; i++) {
    if (true) {
      v = 5;
      break;
    }

    v = non_uniform;

    if (true) {
      v = 6;
      break;
    }
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:21:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:20:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:12:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest,
       ForLoop_VarBecomesNonUniformBeforeConditionalContinue_BarrierAtStart) {
    // Use a variable for a conditional barrier in a loop, assign a non-uniform value to
    // that variable later in that loop, then perform a conditional continue before assigning a
    // uniform value to that variable.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; i++) {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    v = non_uniform;
    if (true) {
      continue;
    }

    v = 5;
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:12:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_VarBecomesNonUniformBeforeConditionalContinue) {
    // Use a variable for a conditional barrier in a loop, assign a non-uniform value to
    // that variable later in that loop, then perform a conditional continue before assigning a
    // uniform value to that variable.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  for (var i = 0; i < 10; i++) {
    if (v == 0) {
      workgroupBarrier();
      break;
    }

    v = non_uniform;
    if (true) {
      continue;
    }

    v = 5;
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:7:5 note: control flow depends on non-uniform value
    if (v == 0) {
    ^^

test:12:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
    v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ForLoop_NonUniformCondition_Reconverge) {
    // Loops reconverge at exit, so test that we can call workgroupBarrier() after a loop that has a
    // non-uniform condition.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn foo() {
  for (var i = 0; i < n; i = i + 1) {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

}  // namespace LoopTest

////////////////////////////////////////////////////////////////////////////////
/// If-else statement tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, IfElse_UniformCondition_BarrierInTrueBlock) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> uniform_global : i32;

fn foo() {
  if (uniform_global == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_UniformCondition_BarrierInElseBlock) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> uniform_global : i32;

fn foo() {
  if (uniform_global == 42) {
  } else {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_UniformCondition_BarrierInElseIfBlock) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> uniform_global : i32;

fn foo() {
  if (uniform_global == 42) {
  } else if (true) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_BarrierInTrueBlock) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (non_uniform == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 42) {
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_BarrierInElseBlock) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
  } else {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (non_uniform == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 42) {
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingCondition_NonUniformLHS_And) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn main() {
  if ((non_uniform_global == 42) && false) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:34 note: control flow depends on non-uniform value
  if ((non_uniform_global == 42) && false) {
                                 ^^

test:7:8 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if ((non_uniform_global == 42) && false) {
       ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingCondition_NonUniformRHS_And) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn main() {
  if (false && (non_uniform_global == 42)) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (false && (non_uniform_global == 42)) {
  ^^

test:7:17 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (false && (non_uniform_global == 42)) {
                ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingCondition_NonUniformLHS_Or) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn main() {
  if ((non_uniform_global == 42) || true) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:34 note: control flow depends on non-uniform value
  if ((non_uniform_global == 42) || true) {
                                 ^^

test:7:8 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if ((non_uniform_global == 42) || true) {
       ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingCondition_NonUniformRHS_Or) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn main() {
  if (true || (non_uniform_global == 42)) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (true || (non_uniform_global == 42)) {
  ^^

test:7:16 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (true || (non_uniform_global == 42)) {
               ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_BarrierInElseIfBlock) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
  } else if (true) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (non_uniform == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 42) {
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_VarBecomesNonUniform_BeforeCondition) {
    // Use a function-scope variable for control-flow guarding a barrier, and then assign to that
    // variable before checking the condition.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = 0;
  v = rw;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:6:7 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  v = rw;
      ^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_VarBecomesNonUniform_AfterCondition) {
    // Use a function-scope variable for control-flow guarding a barrier, and then assign to that
    // variable after checking the condition.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = 0;
  if (v == 0) {
    v = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_VarBecomesNonUniformInIf_BarrierInElse) {
    // Assign a non-uniform value to a variable in an if-block, and then use that variable for a
    // conditional barrier in the else block.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  if (true) {
    v = non_uniform;
  } else {
    if (v == 0) {
      workgroupBarrier();
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_AssignNonUniformInIf_AssignUniformInElse) {
    // Assign a non-uniform value to a variable in an if-block and a uniform value in the else
    // block, and then use that variable for a conditional barrier after the if-else statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  if (true) {
    if (true) {
      v = non_uniform;
    } else {
      v = 5;
    }
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:14:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:8:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_AssignNonUniformInIfWithReturn) {
    // Assign a non-uniform value to a variable in an if-block followed by a return, and then use
    // that variable for a conditional barrier after the if-else statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  if (true) {
    v = non_uniform;
    return;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_AssignNonUniformBeforeIf_BothBranchesAssignUniform) {
    // Assign a non-uniform value to a variable before and if-else statement, assign uniform values
    // in both branch of the if-else, and then use that variable for a conditional barrier after
    // the if-else statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  v = non_uniform;
  if (true) {
    v = 5;
  } else {
    v = 6;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_AssignNonUniformBeforeIf_OnlyTrueBranchAssignsUniform) {
    // Assign a non-uniform value to a variable before and if-else statement, assign a uniform value
    // in the true branch of the if-else, and then use that variable for a conditional barrier after
    // the if-else statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  v = non_uniform;
  if (true) {
    v = 5;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:6:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  v = non_uniform;
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_AssignNonUniformBeforeIf_OnlyFalseBranchAssignsUniform) {
    // Assign a non-uniform value to a variable before and if-else statement, assign a uniform value
    // in the false branch of the if-else, and then use that variable for a conditional barrier
    // after the if-else statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  v = non_uniform;
  if (true) {
  } else {
    v = 5;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:13:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:12:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:6:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  v = non_uniform;
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest,
       IfElse_AssignNonUniformBeforeIf_OnlyTrueBranchAssignsUniform_FalseBranchReturns) {
    // Assign a non-uniform value to a variable before and if-else statement, assign a uniform value
    // in the true branch of the if-else, leave the variable untouched in the false branch and just
    // return, and then use that variable for a conditional barrier after the if-else statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  v = non_uniform;
  if (true) {
    v = 5;
  } else {
    return;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest,
       IfElse_AssignNonUniformBeforeIf_OnlyFalseBranchAssignsUniform_TrueBranchReturns) {
    // Assign a non-uniform value to a variable before and if-else statement, assign a uniform value
    // in the false branch of the if-else, leave the variable untouched in the true branch and just
    // return, and then use that variable for a conditional barrier after the if-else statement.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  v = non_uniform;
  if (true) {
    return;
  } else {
    v = 5;
  }

  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformCondition_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingNonUniformConditionLHS_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition that uses short-circuiting.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42 || true) {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_ShortCircuitingNonUniformConditionRHS_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition that uses short-circuiting.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (false && non_uniform == 42) {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformFunctionCall_Reconverge) {
    // If statements reconverge at exit, so test that we can call workgroupBarrier() after an if
    // statement with a non-uniform condition.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar() {
  if (non_uniform == 42) {
    return;
  } else {
    return;
  }
}

fn foo() {
  if (non_uniform == 42) {
    bar();
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformReturn_NoReconverge) {
    // If statements should not reconverge after non-uniform returns.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
    return;
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (non_uniform == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 42) {
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, IfElse_NonUniformDiscard_NoReconverge) {
    // If statements should not reconverge after non-uniform discards.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  if (non_uniform == 42) {
    discard;
  } else {
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (non_uniform == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 42) {
      ^^^^^^^^^^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Switch statement tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, Switch_NonUniformCondition_BarrierInCase) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (non_uniform) {
    case 42: {
      workgroupBarrier();
      break;
    }
    default: {
      break;
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  switch (non_uniform) {
  ^^^^^^

test:5:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  switch (non_uniform) {
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformCondition_BarrierInDefault) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (non_uniform) {
    default: {
      workgroupBarrier();
      break;
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  switch (non_uniform) {
  ^^^^^^

test:5:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  switch (non_uniform) {
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformBreak) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  switch (condition) {
    case 42: {
      if (non_uniform == 42) {
        break;
      }
      workgroupBarrier();
    }
    default: {
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:8:7 note: control flow depends on non-uniform value
      if (non_uniform == 42) {
      ^^

test:8:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      if (non_uniform == 42) {
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformBreakInDifferentCase) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  switch (condition) {
    case 0: {
      if (non_uniform == 42) {
        break;
      }
    }
    case 42: {
      workgroupBarrier();
    }
    default: {
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformBreakInDifferentCase_Fallthrough) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  switch (condition) {
    case 0: {
      if (non_uniform == 42) {
        break;
      }
      fallthrough;
    }
    case 42: {
      workgroupBarrier();
    }
    default: {
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:8:7 note: control flow depends on non-uniform value
      if (non_uniform == 42) {
      ^^

test:8:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      if (non_uniform == 42) {
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesNonUniformInDifferentCase_WithBreak) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = 0;
  switch (condition) {
    case 0: {
      x = non_uniform;
      break;
    }
    case 42: {
      if (x == 0) {
        workgroupBarrier();
      }
    }
    default: {
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesNonUniformInDifferentCase_WithFallthrough) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = 0;
  switch (condition) {
    case 0: {
      x = non_uniform;
      fallthrough;
    }
    case 42: {
      if (x == 0) {
        workgroupBarrier();
      }
    }
    default: {
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:9 warning: 'workgroupBarrier' must only be called from uniform control flow
        workgroupBarrier();
        ^^^^^^^^^^^^^^^^

test:13:7 note: control flow depends on non-uniform value
      if (x == 0) {
      ^^

test:9:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      x = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesUniformInDifferentCase_WithBreak) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = non_uniform;
  switch (condition) {
    case 0: {
      x = 5;
      break;
    }
    case 42: {
      if (x == 0) {
        workgroupBarrier();
      }
    }
    default: {
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:9 warning: 'workgroupBarrier' must only be called from uniform control flow
        workgroupBarrier();
        ^^^^^^^^^^^^^^^^

test:13:7 note: control flow depends on non-uniform value
      if (x == 0) {
      ^^

test:6:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var x = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesUniformInDifferentCase_WithFallthrough) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = non_uniform;
  switch (condition) {
    case 0: {
      x = 5;
      fallthrough;
    }
    case 42: {
      if (x == 0) {
        workgroupBarrier();
      }
    }
    default: {
    }
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesNonUniformInCase_BarrierAfter) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = 0;
  switch (condition) {
    case 0: {
      x = non_uniform;
    }
    case 42: {
      x = 5;
    }
    default: {
      x = 6;
    }
  }
  if (x == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:19:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:18:3 note: control flow depends on non-uniform value
  if (x == 0) {
  ^^

test:9:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      x = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesUniformInAllCases_BarrierAfter) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = non_uniform;
  switch (condition) {
    case 0: {
      x = 4;
    }
    case 42: {
      x = 5;
    }
    default: {
      x = 6;
    }
  }
  if (x == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesUniformInSomeCases_BarrierAfter) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = non_uniform;
  switch (condition) {
    case 0: {
      x = 4;
    }
    case 42: {
    }
    default: {
      x = 6;
    }
  }
  if (x == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:18:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:17:3 note: control flow depends on non-uniform value
  if (x == 0) {
  ^^

test:6:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var x = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesUniformInCasesThatDontReturn_BarrierAfter) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = non_uniform;
  switch (condition) {
    case 0: {
      x = 4;
    }
    case 42: {
      return;
    }
    default: {
      x = 6;
    }
  }
  if (x == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_VarBecomesUniformAfterConditionalBreak_BarrierAfter) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = non_uniform;
  switch (condition) {
    case 0: {
      x = 4;
    }
    case 42: {
    }
    default: {
      if (false) {
        break;
      }
      x = 6;
    }
  }
  if (x == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:21:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:20:3 note: control flow depends on non-uniform value
  if (x == 0) {
  ^^

test:6:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var x = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_NestedInLoop_VarBecomesNonUniformWithBreak_BarrierInLoop) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = 0;
  loop {
    if (x == 0) {
      workgroupBarrier();
      break;
    }

    switch (condition) {
      case 0: {
        x = non_uniform;
        break;
      }
      default: {
        x = 6;
      }
    }
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:8:5 note: control flow depends on non-uniform value
    if (x == 0) {
    ^^

test:15:13 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
        x = non_uniform;
            ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Switch_NestedInLoop_VarBecomesNonUniformWithBreak_BarrierAfterLoop) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(0) var<uniform> condition : i32;

fn foo() {
  var x = 0;
  loop {
    if (false) {
      break;
    }
    switch (condition) {
      case 0: {
        x = non_uniform;
        break;
      }
      default: {
        x = 6;
      }
    }
    x = 5;
  }
  if (x == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformCondition_Reconverge) {
    // Switch statements reconverge at exit, so test that we can call workgroupBarrier() after a
    // switch statement that contains a non-uniform conditional break.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (non_uniform) {
    default: {
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformBreak_Reconverge) {
    // Switch statements reconverge at exit, so test that we can call workgroupBarrier() after a
    // switch statement that contains a non-uniform conditional break.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  switch (42) {
    default: {
      if (non_uniform == 0) {
        break;
      }
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformFunctionCall_Reconverge) {
    // Switch statements reconverge at exit, so test that we can call workgroupBarrier() after a
    // switch statement that contains a call to a function that causes non-uniform control flow.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    return;
  } else {
    return;
  }
}

fn foo() {
  switch (42) {
    default: {
      bar();
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, Switch_NonUniformFunctionDiscard_NoReconvergence) {
    // Switch statements should not reconverge after non-uniform discards.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> n : i32;

fn bar() {
  if (n == 42) {
    discard;
  }
}

fn foo() {
  switch (42) {
    default: {
      bar();
      break;
    }
  }
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:17:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:13:7 note: calling 'bar' may cause subsequent control flow to be non-uniform
      bar();
      ^^^

test:5:3 note: control flow depends on non-uniform value
  if (n == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'n' may result in a non-uniform value
  if (n == 42) {
      ^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Pointer tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  *&v = non_uniform;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:6:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  *&v = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughCapturedPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv = &v;
  *pv = non_uniform;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:7:9 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  *pv = non_uniform;
        ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  *&v = 42;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughCapturedPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv = &v;
  *pv = 42;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughCapturedPointer_InNonUniformControlFlow) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv = &v;
  if (non_uniform == 0) {
    *pv = 42;
  }
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (non_uniform == 0) {
  ^^

test:7:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 0) {
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadNonUniformThroughPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  if (*&v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:6:3 note: control flow depends on non-uniform value
  if (*&v == 0) {
  ^^

test:5:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadNonUniformThroughCapturedPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv = &v;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (*pv == 0) {
  ^^

test:5:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadNonUniformThroughPointerParameter) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  if (*p == 0) {
    workgroupBarrier();
  }
}

fn foo() {
  var v = non_uniform;
  bar(&v);
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:7 warning: parameter 'p' of 'bar' must be uniform
  bar(&v);
      ^

test:6:5 note: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadUniformThroughPointer) {
    std::string src = R"(
fn foo() {
  var v = 42;
  if (*&v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, LoadUniformThroughCapturedPointer) {
    std::string src = R"(
fn foo() {
  var v = 42;
  let pv = &v;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, LoadUniformThroughPointerParameter) {
    std::string src = R"(
fn bar(p : ptr<function, i32>) {
  if (*p == 0) {
    workgroupBarrier();
  }
}

fn foo() {
  var v = 42;
  bar(&v);
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, StoreNonUniformAfterCapturingPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv = &v;
  v = non_uniform;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (*pv == 0) {
  ^^

test:7:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  v = non_uniform;
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StoreUniformAfterCapturingPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv = &v;
  v = 42;
  if (*pv == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughLongChainOfPointers) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv1 = &*&v;
  let pv2 = &*&*pv1;
  *&*&*pv2 = non_uniform;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:9:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:8:14 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  *&*&*pv2 = non_uniform;
             ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, LoadNonUniformThroughLongChainOfPointers) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = non_uniform;
  let pv1 = &*&v;
  let pv2 = &*&*pv1;
  if (*&*&*pv2 == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (*&*&*pv2 == 0) {
  ^^

test:5:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThenNonUniformThroughDifferentPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv1 = &v;
  let pv2 = &v;
  *pv1 = 42;
  *pv2 = non_uniform;
  if (*pv1 == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:10:3 note: control flow depends on non-uniform value
  if (*pv1 == 0) {
  ^^

test:9:10 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  *pv2 = non_uniform;
         ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThenUniformThroughDifferentPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  var v = 0;
  let pv1 = &v;
  let pv2 = &v;
  *pv1 = non_uniform;
  *pv2 = 42;
  if (*pv1 == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, UnmodifiedPointerParameterNonUniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
}

fn foo() {
  var v = non_uniform;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:11:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:10:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:8:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, UnmodifiedPointerParameterUniform) {
    std::string src = R"(
fn bar(p : ptr<function, i32>) {
}

fn foo() {
  var v = 42;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCall) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  *p = non_uniform;
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:10:7 note: pointer contents may become non-uniform after calling 'bar'
  bar(&v);
      ^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCall) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  *p = 42;
}

fn foo() {
  var v = non_uniform;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCallViaArg) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : i32) {
  *p = a;
}

fn foo() {
  var v = 0;
  bar(&v, non_uniform);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:10:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  bar(&v, non_uniform);
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCallViaPointerArg) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn foo() {
  var v = 0;
  var a = non_uniform;
  bar(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:13:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:12:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:10:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var a = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCallViaArg) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : i32) {
  *p = a;
}

fn foo() {
  var v = non_uniform;
  bar(&v, 42);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCallViaPointerArg) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn foo() {
  var v = non_uniform;
  var a = 42;
  bar(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, AssignNonUniformThroughPointerInFunctionCallChain) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn f3(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn f2(p : ptr<function, i32>, a : ptr<function, i32>) {
  f3(p, a);
}

fn f1(p : ptr<function, i32>, a : ptr<function, i32>) {
  f2(p, a);
}

fn foo() {
  var v = 0;
  var a = non_uniform;
  f1(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:21:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:20:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:18:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var a = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, AssignUniformThroughPointerInFunctionCallChain) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn f3(p : ptr<function, i32>, a : ptr<function, i32>) {
  *p = *a;
}

fn f2(p : ptr<function, i32>, a : ptr<function, i32>) {
  f3(p, a);
}

fn f1(p : ptr<function, i32>, a : ptr<function, i32>) {
  f2(p, a);
}

fn foo() {
  var v = non_uniform;
  var a = 42;
  f1(&v, &a);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, MakePointerParamUniformInReturnExpression) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn zoo(p : ptr<function, i32>) -> i32 {
  *p = 5;
  return 6;
}

fn bar(p : ptr<function, i32>) -> i32 {
  *p = non_uniform;
  return zoo(p);
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, MakePointerParamNonUniformInReturnExpression) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn zoo(p : ptr<function, i32>) -> i32 {
  *p = non_uniform;
  return 6;
}

fn bar(p : ptr<function, i32>) -> i32 {
  *p = 5;
  return zoo(p);
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:18:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:17:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:16:7 note: pointer contents may become non-uniform after calling 'bar'
  bar(&v);
      ^
)");
}

TEST_F(UniformityAnalysisTest, PointerParamAssignNonUniformInTrueAndUniformInFalse) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  if (true) {
    *p = non_uniform;
  } else {
    *p = 5;
  }
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:16:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:15:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:14:7 note: pointer contents may become non-uniform after calling 'bar'
  bar(&v);
      ^
)");
}

TEST_F(UniformityAnalysisTest, ConditionalAssignNonUniformToPointerParamAndReturn) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  if (true) {
    *p = non_uniform;
    return;
  }
  *p = 5;
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:16:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:15:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:14:7 note: pointer contents may become non-uniform after calling 'bar'
  bar(&v);
      ^
)");
}

TEST_F(UniformityAnalysisTest, ConditionalAssignNonUniformToPointerParamAndBreakFromSwitch) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;
@group(0) @binding(1) var<uniform> condition : i32;

fn bar(p : ptr<function, i32>) {
  switch (condition) {
    case 0 {
      if (true) {
        *p = non_uniform;
        break;
      }
      *p = 5;
    }
    default {
      *p = 6;
    }
  }
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:24:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:23:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:22:7 note: pointer contents may become non-uniform after calling 'bar'
  bar(&v);
      ^
)");
}

TEST_F(UniformityAnalysisTest, ConditionalAssignNonUniformToPointerParamAndBreakFromLoop) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  loop {
    if (true) {
      *p = non_uniform;
      break;
    }
    *p = 5;
  }
}

fn foo() {
  var v = 0;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:18:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:17:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:16:7 note: pointer contents may become non-uniform after calling 'bar'
  bar(&v);
      ^
)");
}

TEST_F(UniformityAnalysisTest, ConditionalAssignNonUniformToPointerParamAndContinue) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo(p : ptr<function, i32>) {
  loop {
    if (*p == 0) {
      workgroupBarrier();
      break;
    }

    if (true) {
      *p = non_uniform;
      continue;
    }
    *p = 5;
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:7:7 warning: 'workgroupBarrier' must only be called from uniform control flow
      workgroupBarrier();
      ^^^^^^^^^^^^^^^^

test:6:5 note: control flow depends on non-uniform value
    if (*p == 0) {
    ^^

test:12:12 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
      *p = non_uniform;
           ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, PointerParamMaybeBecomesUniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  if (true) {
    *p = 5;
    return;
  }
}

fn foo() {
  var v = non_uniform;
  bar(&v);
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:14:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:12:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, NonUniformPointerParameterBecomesUniform_AfterUse) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *b = *a;
  *a = 0;
}

fn foo() {
  var a = non_uniform;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:3 note: control flow depends on non-uniform value
  if (b == 0) {
  ^^

test:10:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var a = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, NonUniformPointerParameterBecomesUniform_BeforeUse) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *a = 0;
  *b = *a;
}

fn foo() {
  var a = non_uniform;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, UniformPointerParameterBecomesNonUniform_BeforeUse) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *a = non_uniform;
  *b = *a;
}

fn foo() {
  var a = 0;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:3 note: control flow depends on non-uniform value
  if (b == 0) {
  ^^

test:12:11 note: pointer contents may become non-uniform after calling 'bar'
  bar(&a, &b);
          ^
)");
}

TEST_F(UniformityAnalysisTest, UniformPointerParameterBecomesNonUniform_AfterUse) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *b = *a;
  *a = non_uniform;
}

fn foo() {
  var a = 0;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, NonUniformPointerParameterUpdatedInPlace) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(p : ptr<function, i32>) {
  (*p)++;
}

fn foo() {
  var v = non_uniform;
  bar(&v);
  if (v == 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:3 note: control flow depends on non-uniform value
  if (v == 1) {
  ^^

test:9:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var v = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, MultiplePointerParametersBecomeNonUniform) {
    // The analysis traverses the tree for each pointer parameter, and we need to make sure that we
    // reset the "visited" state of nodes in between these traversals to properly capture each of
    // their uniformity states.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>) {
  *a = non_uniform;
  *b = non_uniform;
}

fn foo() {
  var a = 0;
  var b = 0;
  bar(&a, &b);
  if (b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:3 note: control flow depends on non-uniform value
  if (b == 0) {
  ^^

test:12:11 note: pointer contents may become non-uniform after calling 'bar'
  bar(&a, &b);
          ^
)");
}

TEST_F(UniformityAnalysisTest, MultiplePointerParametersWithEdgesToEachOther) {
    // The analysis traverses the tree for each pointer parameter, and we need to make sure that we
    // reset the "visited" state of nodes in between these traversals to properly capture each of
    // their uniformity states.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn bar(a : ptr<function, i32>, b : ptr<function, i32>, c : ptr<function, i32>) {
  *a = *a;
  *b = *b;
  *c = *a + *b;
}

fn foo() {
  var a = non_uniform;
  var b = 0;
  var c = 0;
  bar(&a, &b, &c);
  if (c == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:16:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:15:3 note: control flow depends on non-uniform value
  if (c == 0) {
  ^^

test:11:11 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  var a = non_uniform;
          ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, MaximumNumberOfPointerParameters) {
    // Create a function with the maximum number of parameters, all pointers, to stress the
    // quadratic nature of the analysis.
    ProgramBuilder b;
    auto& ty = b.ty;

    // fn foo(p0 : ptr<function, i32>, p1 : ptr<function, i32>, ...) {
    //   let rhs = *p0 + *p1 + ... + *p244;
    //   *p1 = rhs;
    //   *p2 = rhs;
    //   ...
    //   *p254 = rhs;
    // }
    ast::VariableList params;
    ast::StatementList foo_body;
    const ast::Expression* rhs_init = b.Deref("p0");
    for (int i = 1; i < 255; i++) {
        rhs_init = b.Add(rhs_init, b.Deref("p" + std::to_string(i)));
    }
    foo_body.push_back(b.Decl(b.Let("rhs", nullptr, rhs_init)));
    for (int i = 0; i < 255; i++) {
        params.push_back(
            b.Param("p" + std::to_string(i), ty.pointer(ty.i32(), ast::StorageClass::kFunction)));
        if (i > 0) {
            foo_body.push_back(b.Assign(b.Deref("p" + std::to_string(i)), "rhs"));
        }
    }
    b.Func("foo", std::move(params), ty.void_(), foo_body);

    // var<private> non_uniform_global : i32;
    // fn main() {
    //   var v0 : i32;
    //   var v1 : i32;
    //   ...
    //   var v254 : i32;
    //   v0 = non_uniform_global;
    //   foo(&v0, &v1, ...,  &v254);
    //   if (v254 == 0) {
    //     workgroupBarrier();
    //   }
    // }
    b.Global("non_uniform_global", ty.i32(), ast::StorageClass::kPrivate);
    ast::StatementList main_body;
    ast::ExpressionList args;
    for (int i = 0; i < 255; i++) {
        auto name = "v" + std::to_string(i);
        main_body.push_back(b.Decl(b.Var(name, ty.i32())));
        args.push_back(b.AddressOf(name));
    }
    main_body.push_back(b.Assign("v0", "non_uniform_global"));
    main_body.push_back(b.CallStmt(b.create<ast::CallExpression>(b.Expr("foo"), args)));
    main_body.push_back(
        b.If(b.Equal("v254", 0_i), b.Block(b.CallStmt(b.Call("workgroupBarrier")))));
    b.Func("main", {}, ty.void_(), main_body);

    // TODO(jrprice): Expect false when uniformity issues become errors.
    EXPECT_TRUE(RunTest(std::move(b))) << error_;
    EXPECT_EQ(error_,
              R"(warning: 'workgroupBarrier' must only be called from uniform control flow
note: control flow depends on non-uniform value
note: reading from module-scope private variable 'non_uniform_global' may result in a non-uniform value)");
}

////////////////////////////////////////////////////////////////////////////////
/// Tests to cover access to aggregate types.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, VectorElement_Uniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> v : vec4<i32>;

fn foo() {
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, VectorElement_NonUniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> v : array<i32>;

fn foo() {
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (v[2] == 0) {
  ^^

test:5:7 note: reading from read_write storage buffer 'v' may result in a non-uniform value
  if (v[2] == 0) {
      ^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_BecomesNonUniform_BeforeCondition) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[2] = rw;
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (v[2] == 0) {
  ^^

test:6:10 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  v[2] = rw;
         ^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_BecomesNonUniform_AfterCondition) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  if (v[2] == 0) {
    v[2] = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, VectorElement_DifferentElementBecomesNonUniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = rw;
  if (v[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (v[2] == 0) {
  ^^

test:6:10 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  v[1] = rw;
         ^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_ElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, that element is
    // still considered to be non-uniform.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = rw;
  v[1] = 42;
  if (v[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (v[1] == 0) {
  ^^

test:6:10 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  v[1] = rw;
         ^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_DifferentElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, the whole vector
    // is still considered to be non-uniform.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = rw;
  v[2] = 42;
  if (v[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (v[1] == 0) {
  ^^

test:6:10 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  v[1] = rw;
         ^^
)");
}

TEST_F(UniformityAnalysisTest, VectorElement_NonUniform_AnyBuiltin) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

fn foo() {
  var v : vec4<i32>;
  v[1] = non_uniform_global;
  if (any(v == vec4(42))) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (any(v == vec4(42))) {
  ^^

test:6:10 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  v[1] = non_uniform_global;
         ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_Uniform) {
    std::string src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read> s : S;

fn foo() {
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, StructMember_NonUniform) {
    std::string src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> s : S;

fn foo() {
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:9:3 note: control flow depends on non-uniform value
  if (s.b == 0) {
  ^^

test:9:7 note: reading from read_write storage buffer 's' may result in a non-uniform value
  if (s.b == 0) {
      ^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_BecomesNonUniform_BeforeCondition) {
    std::string src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.b = rw;
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:3 note: control flow depends on non-uniform value
  if (s.b == 0) {
  ^^

test:10:9 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  s.b = rw;
        ^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_BecomesNonUniform_AfterCondition) {
    std::string src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  if (s.b == 0) {
    s.b = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, StructMember_DifferentMemberBecomesNonUniform) {
    std::string src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.a = rw;
  if (s.b == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:12:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:11:3 note: control flow depends on non-uniform value
  if (s.b == 0) {
  ^^

test:10:9 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  s.a = rw;
        ^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_MemberBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to a member, that member is
    // still considered to be non-uniform.
    std::string src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.a = rw;
  s.a = 0;
  if (s.a == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:13:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:12:3 note: control flow depends on non-uniform value
  if (s.a == 0) {
  ^^

test:10:9 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  s.a = rw;
        ^^
)");
}

TEST_F(UniformityAnalysisTest, StructMember_DifferentMemberBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to a member, the whole struct
    // is still considered to be non-uniform.
    std::string src = R"(
struct S {
  a : i32,
  b : i32,
}
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var s : S;
  s.a = rw;
  s.b = 0;
  if (s.a == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:13:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:12:3 note: control flow depends on non-uniform value
  if (s.a == 0) {
  ^^

test:10:9 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  s.a = rw;
        ^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_Uniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read> arr : array<i32>;

fn foo() {
  if (arr[7] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ArrayElement_NonUniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> arr : array<i32>;

fn foo() {
  if (arr[7] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (arr[7] == 0) {
  ^^

test:5:7 note: reading from read_write storage buffer 'arr' may result in a non-uniform value
  if (arr[7] == 0) {
      ^^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_BecomesNonUniform_BeforeCondition) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[2] = rw;
  if (arr[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (arr[2] == 0) {
  ^^

test:6:12 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  arr[2] = rw;
           ^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_BecomesNonUniform_AfterCondition) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  if (arr[2] == 0) {
    arr[2] = rw;
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ArrayElement_DifferentElementBecomesNonUniform) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[1] = rw;
  if (arr[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (arr[2] == 0) {
  ^^

test:6:12 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  arr[1] = rw;
           ^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_DifferentElementBecomesNonUniformThroughPointer) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  let pa = &arr[1];
  *pa = rw;
  if (arr[2] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (arr[2] == 0) {
  ^^

test:7:9 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  *pa = rw;
        ^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_ElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, that element is
    // still considered to be non-uniform.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[1] = rw;
  arr[1] = 42;
  if (arr[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (arr[1] == 0) {
  ^^

test:6:12 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  arr[1] = rw;
           ^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_DifferentElementBecomesUniform) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element, the whole array
    // is still considered to be non-uniform.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  arr[1] = rw;
  arr[2] = 42;
  if (arr[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:9:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:8:3 note: control flow depends on non-uniform value
  if (arr[1] == 0) {
  ^^

test:6:12 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  arr[1] = rw;
           ^^
)");
}

TEST_F(UniformityAnalysisTest, ArrayElement_ElementBecomesUniformThroughPointer) {
    // For aggregate types, we conservatively consider them to be forever non-uniform once they
    // become non-uniform. Test that after assigning a uniform value to an element through a
    // pointer, the whole array is still considered to be non-uniform.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var arr : array<i32, 4>;
  let pa = &arr[2];
  arr[1] = rw;
  *pa = 42;
  if (arr[1] == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:9:3 note: control flow depends on non-uniform value
  if (arr[1] == 0) {
  ^^

test:7:12 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  arr[1] = rw;
           ^^
)");
}

////////////////////////////////////////////////////////////////////////////////
/// Miscellaneous statement and expression tests.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, FunctionRequiresUniformFlowAndCausesNonUniformFlow) {
    // Test that a function that requires uniform flow and then causes non-uniform flow can be
    // called without error.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

fn foo() {
  _ = dpdx(0.5);

  if (non_uniform_global == 0) {
    discard;
  }
}

@fragment
fn main() {
  foo();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, TypeConstructor) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

fn foo() {
  if (i32(non_uniform_global) == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (i32(non_uniform_global) == 0) {
  ^^

test:5:11 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (i32(non_uniform_global) == 0) {
          ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Conversion) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

fn foo() {
  if (f32(non_uniform_global) == 0.0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (f32(non_uniform_global) == 0.0) {
  ^^

test:5:11 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (f32(non_uniform_global) == 0.0) {
          ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Bitcast) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

fn foo() {
  if (bitcast<f32>(non_uniform_global) == 0.0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (bitcast<f32>(non_uniform_global) == 0.0) {
  ^^

test:5:20 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (bitcast<f32>(non_uniform_global) == 0.0) {
                   ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, CompoundAssignment_NonUniformRHS) {
    // Use compound assignment with a non-uniform RHS on a variable.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = 0;
  v += rw;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:6:8 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  v += rw;
       ^^
)");
}

TEST_F(UniformityAnalysisTest, CompoundAssignment_UniformRHS_StillNonUniform) {
    // Use compound assignment with a uniform RHS on a variable that is already non-uniform.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  var v = rw;
  v += 1;
  if (v == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:8:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (v == 0) {
  ^^

test:5:11 note: reading from read_write storage buffer 'rw' may result in a non-uniform value
  var v = rw;
          ^^
)");
}

TEST_F(UniformityAnalysisTest, PhonyAssignment_LhsCausesNonUniformControlFlow) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> nonuniform_var : i32;

fn bar() -> i32 {
  if (nonuniform_var == 42) {
    return 1;
  } else {
    return 2;
  }
}

fn foo() {
  _ = bar();
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:14:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:13:7 note: calling 'bar' may cause subsequent control flow to be non-uniform
  _ = bar();
      ^^^

test:5:3 note: control flow depends on non-uniform value
  if (nonuniform_var == 42) {
  ^^

test:5:7 note: reading from read_write storage buffer 'nonuniform_var' may result in a non-uniform value
  if (nonuniform_var == 42) {
      ^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ShortCircuiting_NoReconvergeLHS) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn non_uniform_discard_func() -> bool {
  if (non_uniform_global == 42) {
    discard;
  }
  return false;
}

fn main() {
  let b = non_uniform_discard_func() && false;
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:14:11 note: calling 'non_uniform_discard_func' may cause subsequent control flow to be non-uniform
  let b = non_uniform_discard_func() && false;
          ^^^^^^^^^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (non_uniform_global == 42) {
  ^^

test:7:7 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (non_uniform_global == 42) {
      ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ShortCircuiting_NoReconvergeRHS) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn non_uniform_discard_func() -> bool {
  if (non_uniform_global == 42) {
    discard;
  }
  return false;
}

fn main() {
  let b = false && non_uniform_discard_func();
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:14:20 note: calling 'non_uniform_discard_func' may cause subsequent control flow to be non-uniform
  let b = false && non_uniform_discard_func();
                   ^^^^^^^^^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (non_uniform_global == 42) {
  ^^

test:7:7 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (non_uniform_global == 42) {
      ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ShortCircuiting_NoReconvergeBoth) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn non_uniform_discard_func() -> bool {
  if (non_uniform_global == 42) {
    discard;
  }
  return false;
}

fn main() {
  let b = non_uniform_discard_func() && non_uniform_discard_func();
  workgroupBarrier();
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:3 warning: 'workgroupBarrier' must only be called from uniform control flow
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:14:41 note: calling 'non_uniform_discard_func' may cause subsequent control flow to be non-uniform
  let b = non_uniform_discard_func() && non_uniform_discard_func();
                                        ^^^^^^^^^^^^^^^^^^^^^^^^

test:7:3 note: control flow depends on non-uniform value
  if (non_uniform_global == 42) {
  ^^

test:7:7 note: reading from read_write storage buffer 'non_uniform_global' may result in a non-uniform value
  if (non_uniform_global == 42) {
      ^^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, ShortCircuiting_ReconvergeLHS) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn uniform_discard_func() -> bool {
  if (true) {
    discard;
  }
  return false;
}

fn main() {
  let b = uniform_discard_func() && false;
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ShortCircuiting_ReconvergeRHS) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn uniform_discard_func() -> bool {
  if (true) {
    discard;
  }
  return false;
}

fn main() {
  let b = false && uniform_discard_func();
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ShortCircuiting_ReconvergeBoth) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform_global : i32;

var<private> p : i32;

fn uniform_discard_func() -> bool {
  if (true) {
    discard;
  }
  return false;
}

fn main() {
  let b = uniform_discard_func() && uniform_discard_func();
  workgroupBarrier();
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, DeadCode_AfterReturn) {
    // Dead code after a return statement shouldn't cause uniformity errors.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  return;
  if (non_uniform == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, DeadCode_AfterDiscard) {
    // Dead code after a discard statement shouldn't cause uniformity errors.
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  discard;
  if (non_uniform == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, ArrayLength) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> arr : array<f32>;

fn foo() {
  for (var i = 0u; i < arrayLength(&arr); i++) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, WorkgroupAtomics) {
    std::string src = R"(
var<workgroup> a : atomic<i32>;

fn foo() {
  if (atomicAdd(&a, 1) == 1) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (atomicAdd(&a, 1) == 1) {
  ^^

test:5:18 note: reading from workgroup storage variable 'a' may result in a non-uniform value
  if (atomicAdd(&a, 1) == 1) {
                 ^
)");
}

TEST_F(UniformityAnalysisTest, StorageAtomics) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<i32>;

fn foo() {
  if (atomicAdd(&a, 1) == 1) {
    storageBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:6:5 warning: 'storageBarrier' must only be called from uniform control flow
    storageBarrier();
    ^^^^^^^^^^^^^^

test:5:3 note: control flow depends on non-uniform value
  if (atomicAdd(&a, 1) == 1) {
  ^^

test:5:18 note: reading from read_write storage buffer 'a' may result in a non-uniform value
  if (atomicAdd(&a, 1) == 1) {
                 ^
)");
}

TEST_F(UniformityAnalysisTest, DisableAnalysisWithExtension) {
    std::string src = R"(
enable chromium_disable_uniformity_analysis;

@group(0) @binding(0) var<storage, read_write> rw : i32;

fn foo() {
  if (rw == 0) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, true);
}

TEST_F(UniformityAnalysisTest, StressGraphTraversalDepth) {
    // Create a function with a very long sequence of variable declarations and assignments to
    // test traversals of very deep graphs. This requires a non-recursive traversal algorithm.
    ProgramBuilder b;
    auto& ty = b.ty;

    // var<private> v0 : i32 = 0i;
    // fn foo() {
    //   let v1 = v0;
    //   let v2 = v1;
    //   ...
    //   let v{N} = v{N-1};
    //   if (v{N} == 0) {
    //     workgroupBarrier();
    //   }
    // }
    b.Global("v0", ty.i32(), ast::StorageClass::kPrivate, b.Expr(0_i));
    ast::StatementList foo_body;
    std::string v_last = "v0";
    for (int i = 1; i < 100000; i++) {
        auto v = "v" + std::to_string(i);
        foo_body.push_back(b.Decl(b.Var(v, nullptr, b.Expr(v_last))));
        v_last = v;
    }
    foo_body.push_back(b.If(b.Equal(v_last, 0_i), b.Block(b.CallStmt(b.Call("workgroupBarrier")))));
    b.Func("foo", {}, ty.void_(), foo_body);

    // TODO(jrprice): Expect false when uniformity issues become errors.
    EXPECT_TRUE(RunTest(std::move(b))) << error_;
    EXPECT_EQ(error_,
              R"(warning: 'workgroupBarrier' must only be called from uniform control flow
note: control flow depends on non-uniform value
note: reading from module-scope private variable 'v0' may result in a non-uniform value)");
}

////////////////////////////////////////////////////////////////////////////////
/// Tests for the quality of the error messages produced by the analysis.
////////////////////////////////////////////////////////////////////////////////

TEST_F(UniformityAnalysisTest, Error_CallUserThatCallsBuiltinDirectly) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn foo() {
  workgroupBarrier();
}

fn main() {
  if (non_uniform == 42) {
    foo();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:10:5 warning: 'foo' must only be called from uniform control flow
    foo();
    ^^^

test:5:3 note: 'foo' requires uniformity because it calls workgroupBarrier
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:9:3 note: control flow depends on non-uniform value
  if (non_uniform == 42) {
  ^^

test:9:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 42) {
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Error_CallUserThatCallsBuiltinIndirectly) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn zoo() {
  workgroupBarrier();
}

fn bar() {
  zoo();
}

fn foo() {
  bar();
}

fn main() {
  if (non_uniform == 42) {
    foo();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:18:5 warning: 'foo' must only be called from uniform control flow
    foo();
    ^^^

test:5:3 note: 'foo' requires uniformity because it indirectly calls workgroupBarrier
  workgroupBarrier();
  ^^^^^^^^^^^^^^^^

test:17:3 note: control flow depends on non-uniform value
  if (non_uniform == 42) {
  ^^

test:17:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  if (non_uniform == 42) {
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Error_ParametersRequireUniformityInChain) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn zoo(a : i32) {
  if (a == 42) {
    workgroupBarrier();
  }
}

fn bar(b : i32) {
  zoo(b);
}

fn foo(c : i32) {
  bar(c);
}

fn main() {
  foo(non_uniform);
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:19:7 warning: parameter 'c' of 'foo' must be uniform
  foo(non_uniform);
      ^^^^^^^^^^^

test:15:7 note: parameter 'b' of 'bar' must be uniform
  bar(c);
      ^

test:11:7 note: parameter 'a' of 'zoo' must be uniform
  zoo(b);
      ^

test:6:5 note: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:19:7 note: reading from read_write storage buffer 'non_uniform' may result in a non-uniform value
  foo(non_uniform);
      ^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Error_ReturnValueMayBeNonUniformChain) {
    std::string src = R"(
@group(0) @binding(0) var<storage, read_write> non_uniform : i32;

fn zoo() -> i32 {
  return non_uniform;
}

fn bar() -> i32 {
  return zoo();
}

fn foo() -> i32 {
  return bar();
}

fn main() {
  if (foo() == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:18:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:17:3 note: control flow depends on non-uniform value
  if (foo() == 42) {
  ^^

test:17:7 note: return value of 'foo' may be non-uniform
  if (foo() == 42) {
      ^^^
)");
}

TEST_F(UniformityAnalysisTest, Error_SubsequentControlFlowMayBeNonUniform) {
    // Make sure we correctly identify the function call as the source of non-uniform control flow
    // and not the if statement with the uniform condition.
    std::string src = R"(
@group(0) @binding(0) var<uniform> uniform_value : i32;
@group(0) @binding(1) var<storage, read_write> non_uniform_value : i32;

fn foo() -> i32 {
  if (non_uniform_value == 0) {
    return 5;
  }
  return 6;
}

fn main() {
  foo();
  if (uniform_value == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:3 note: calling 'foo' may cause subsequent control flow to be non-uniform
  foo();
  ^^^

test:6:3 note: control flow depends on non-uniform value
  if (non_uniform_value == 0) {
  ^^

test:6:7 note: reading from read_write storage buffer 'non_uniform_value' may result in a non-uniform value
  if (non_uniform_value == 0) {
      ^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Error_ParameterRequiredToBeUniformForSubsequentControlFlow) {
    // Make sure we correctly identify the function call as the source of non-uniform control flow
    // and not the if statement with the uniform condition.
    std::string src = R"(
@group(0) @binding(0) var<uniform> uniform_value : i32;
@group(0) @binding(1) var<storage, read_write> non_uniform_value : i32;

fn foo(x : i32) -> i32 {
  if (x == 0) {
    return 5;
  }
  return 6;
}

fn main() {
  foo(non_uniform_value);
  if (uniform_value == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:7 note: non-uniform function call argument causes subsequent control flow to be non-uniform
  foo(non_uniform_value);
      ^^^^^^^^^^^^^^^^^

test:6:3 note: control flow depends on non-uniform value
  if (x == 0) {
  ^^

test:6:7 note: reading from 'x' may result in a non-uniform value
  if (x == 0) {
      ^

test:13:7 note: reading from read_write storage buffer 'non_uniform_value' may result in a non-uniform value
  foo(non_uniform_value);
      ^^^^^^^^^^^^^^^^^
)");
}

TEST_F(UniformityAnalysisTest, Error_ShortCircuitingExprCausesNonUniformControlFlow) {
    // Make sure we correctly identify the short-circuit as the source of non-uniform control flow
    // and not the if statement with the uniform condition.
    std::string src = R"(
@group(0) @binding(0) var<uniform> uniform_value : i32;
@group(0) @binding(1) var<storage, read_write> non_uniform_value : i32;

fn non_uniform_discard_func() -> bool {
  if (non_uniform_value == 42) {
    discard;
  }
  return false;
}

fn main() {
  let b = non_uniform_discard_func() && true;
  if (uniform_value == 42) {
    workgroupBarrier();
  }
}
)";

    RunTest(src, false);
    EXPECT_EQ(error_,
              R"(test:15:5 warning: 'workgroupBarrier' must only be called from uniform control flow
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test:13:11 note: calling 'non_uniform_discard_func' may cause subsequent control flow to be non-uniform
  let b = non_uniform_discard_func() && true;
          ^^^^^^^^^^^^^^^^^^^^^^^^

test:6:3 note: control flow depends on non-uniform value
  if (non_uniform_value == 42) {
  ^^

test:6:7 note: reading from read_write storage buffer 'non_uniform_value' may result in a non-uniform value
  if (non_uniform_value == 42) {
      ^^^^^^^^^^^^^^^^^
)");
}

}  // namespace
}  // namespace tint::resolver
