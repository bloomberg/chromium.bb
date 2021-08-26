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

#include "src/ast/builtin_decoration.h"
#include "src/ast/location_decoration.h"
#include "src/ast/return_statement.h"
#include "src/ast/stage_decoration.h"
#include "src/ast/struct_block_decoration.h"
#include "src/resolver/resolver.h"
#include "src/resolver/resolver_test_helper.h"

#include "gmock/gmock.h"

namespace tint {
namespace resolver {
namespace {

// Helpers and typedefs
template <typename T>
using DataType = builder::DataType<T>;
template <typename T>
using vec2 = builder::vec2<T>;
template <typename T>
using vec3 = builder::vec3<T>;
template <typename T>
using vec4 = builder::vec4<T>;
template <typename T>
using mat2x2 = builder::mat2x2<T>;
template <typename T>
using mat3x3 = builder::mat3x3<T>;
template <typename T>
using mat4x4 = builder::mat4x4<T>;
template <typename T>
using alias = builder::alias<T>;
using f32 = builder::f32;
using i32 = builder::i32;
using u32 = builder::u32;

class ResolverEntryPointValidationTest : public TestHelper,
                                         public testing::Test {};

TEST_F(ResolverEntryPointValidationTest, ReturnTypeAttribute_Location) {
  // [[stage(fragment)]]
  // fn main() -> [[location(0)]] f32 { return 1.0; }
  Func(Source{{12, 34}}, "main", {}, ty.f32(), {Return(1.0f)},
       {Stage(ast::PipelineStage::kFragment)}, {Location(0)});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverEntryPointValidationTest, ReturnTypeAttribute_Builtin) {
  // [[stage(vertex)]]
  // fn main() -> [[builtin(position)]] vec4<f32> { return vec4<f32>(); }
  Func(Source{{12, 34}}, "main", {}, ty.vec4<f32>(),
       {Return(Construct(ty.vec4<f32>()))},
       {Stage(ast::PipelineStage::kVertex)},
       {Builtin(ast::Builtin::kPosition)});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverEntryPointValidationTest, ReturnTypeAttribute_Missing) {
  // [[stage(vertex)]]
  // fn main() -> f32 {
  //   return 1.0;
  // }
  Func(Source{{12, 34}}, "main", {}, ty.vec4<f32>(),
       {Return(Construct(ty.vec4<f32>()))},
       {Stage(ast::PipelineStage::kVertex)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: missing entry point IO attribute on return type");
}

TEST_F(ResolverEntryPointValidationTest, ReturnTypeAttribute_Multiple) {
  // [[stage(vertex)]]
  // fn main() -> [[location(0)]] [[builtin(position)]] vec4<f32> {
  //   return vec4<f32>();
  // }
  Func(Source{{12, 34}}, "main", {}, ty.vec4<f32>(),
       {Return(Construct(ty.vec4<f32>()))},
       {Stage(ast::PipelineStage::kVertex)},
       {Location(Source{{13, 43}}, 0),
        Builtin(Source{{14, 52}}, ast::Builtin::kPosition)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(14:52 error: multiple entry point IO attributes
13:43 note: previously consumed location(0))");
}

TEST_F(ResolverEntryPointValidationTest, ReturnType_Struct_Valid) {
  // struct Output {
  //   [[location(0)]] a : f32;
  //   [[builtin(frag_depth)]] b : f32;
  // };
  // [[stage(fragment)]]
  // fn main() -> Output {
  //   return Output();
  // }
  auto* output = Structure(
      "Output", {Member("a", ty.f32(), {Location(0)}),
                 Member("b", ty.f32(), {Builtin(ast::Builtin::kFragDepth)})});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverEntryPointValidationTest,
       ReturnType_Struct_MemberMultipleAttributes) {
  // struct Output {
  //   [[location(0)]] [[builtin(frag_depth)]] a : f32;
  // };
  // [[stage(fragment)]]
  // fn main() -> Output {
  //   return Output();
  // }
  auto* output = Structure(
      "Output",
      {Member("a", ty.f32(),
              {Location(Source{{13, 43}}, 0),
               Builtin(Source{{14, 52}}, ast::Builtin::kFragDepth)})});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(14:52 error: multiple entry point IO attributes
13:43 note: previously consumed location(0)
12:34 note: while analysing entry point 'main')");
}

TEST_F(ResolverEntryPointValidationTest,
       ReturnType_Struct_MemberMissingAttribute) {
  // struct Output {
  //   [[location(0)]] a : f32;
  //   b : f32;
  // };
  // [[stage(fragment)]]
  // fn main() -> Output {
  //   return Output();
  // }
  auto* output = Structure(
      "Output", {Member(Source{{13, 43}}, "a", ty.f32(), {Location(0)}),
                 Member(Source{{14, 52}}, "b", ty.f32(), {})});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            R"(14:52 error: missing entry point IO attribute
12:34 note: while analysing entry point 'main')");
}


TEST_F(ResolverEntryPointValidationTest, ReturnType_Struct_DuplicateBuiltins) {
  // struct Output {
  //   [[builtin(frag_depth)]] a : f32;
  //   [[builtin(frag_depth)]] b : f32;
  // };
  // [[stage(fragment)]]
  // fn main() -> Output {
  //   return Output();
  // }
  auto* output = Structure(
      "Output", {Member("a", ty.f32(), {Builtin(ast::Builtin::kFragDepth)}),
                 Member("b", ty.f32(), {Builtin(ast::Builtin::kFragDepth)})});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(12:34 error: builtin(frag_depth) attribute appears multiple times as pipeline output
12:34 note: while analysing entry point 'main')");
}

TEST_F(ResolverEntryPointValidationTest, ParameterAttribute_Location) {
  // [[stage(fragment)]]
  // fn main([[location(0)]] param : f32) {}
  auto* param = Param("param", ty.f32(), {Location(0)});
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverEntryPointValidationTest, ParameterAttribute_Missing) {
  // [[stage(fragment)]]
  // fn main(param : f32) {}
  auto* param = Param(Source{{13, 43}}, "param", ty.vec4<f32>());
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "13:43 error: missing entry point IO attribute on parameter");
}

TEST_F(ResolverEntryPointValidationTest, ParameterAttribute_Multiple) {
  // [[stage(fragment)]]
  // fn main([[location(0)]] [[builtin(sample_index)]] param : u32) {}
  auto* param = Param("param", ty.u32(),
                      {Location(Source{{13, 43}}, 0),
                       Builtin(Source{{14, 52}}, ast::Builtin::kSampleIndex)});
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(14:52 error: multiple entry point IO attributes
13:43 note: previously consumed location(0))");
}

TEST_F(ResolverEntryPointValidationTest, Parameter_Struct_Valid) {
  // struct Input {
  //   [[location(0)]] a : f32;
  //   [[builtin(sample_index)]] b : u32;
  // };
  // [[stage(fragment)]]
  // fn main(param : Input) {}
  auto* input = Structure(
      "Input", {Member("a", ty.f32(), {Location(0)}),
                Member("b", ty.u32(), {Builtin(ast::Builtin::kSampleIndex)})});
  auto* param = Param("param", ty.Of(input));
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(ResolverEntryPointValidationTest,
       Parameter_Struct_MemberMultipleAttributes) {
  // struct Input {
  //   [[location(0)]] [[builtin(sample_index)]] a : u32;
  // };
  // [[stage(fragment)]]
  // fn main(param : Input) {}
  auto* input = Structure(
      "Input",
      {Member("a", ty.u32(),
              {Location(Source{{13, 43}}, 0),
               Builtin(Source{{14, 52}}, ast::Builtin::kSampleIndex)})});
  auto* param = Param("param", ty.Of(input));
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(14:52 error: multiple entry point IO attributes
13:43 note: previously consumed location(0)
12:34 note: while analysing entry point 'main')");
}

TEST_F(ResolverEntryPointValidationTest,
       Parameter_Struct_MemberMissingAttribute) {
  // struct Input {
  //   [[location(0)]] a : f32;
  //   b : f32;
  // };
  // [[stage(fragment)]]
  // fn main(param : Input) {}
  auto* input = Structure(
      "Input", {Member(Source{{13, 43}}, "a", ty.f32(), {Location(0)}),
                Member(Source{{14, 52}}, "b", ty.f32(), {})});
  auto* param = Param("param", ty.Of(input));
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(), R"(14:52 error: missing entry point IO attribute
12:34 note: while analysing entry point 'main')");
}

TEST_F(ResolverEntryPointValidationTest, Parameter_DuplicateBuiltins) {
  // [[stage(fragment)]]
  // fn main([[builtin(sample_index)]] param_a : u32,
  //         [[builtin(sample_index)]] param_b : u32) {}
  auto* param_a =
      Param("param_a", ty.u32(), {Builtin(ast::Builtin::kSampleIndex)});
  auto* param_b =
      Param("param_b", ty.u32(), {Builtin(ast::Builtin::kSampleIndex)});
  Func(Source{{12, 34}}, "main", {param_a, param_b}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      "12:34 error: builtin(sample_index) attribute appears multiple times as "
      "pipeline input");
}

TEST_F(ResolverEntryPointValidationTest, Parameter_Struct_DuplicateBuiltins) {
  // struct InputA {
  //   [[builtin(sample_index)]] a : u32;
  // };
  // struct InputB {
  //   [[builtin(sample_index)]] a : u32;
  // };
  // [[stage(fragment)]]
  // fn main(param_a : InputA, param_b : InputB) {}
  auto* input_a = Structure(
      "InputA", {Member("a", ty.u32(), {Builtin(ast::Builtin::kSampleIndex)})});
  auto* input_b = Structure(
      "InputB", {Member("a", ty.u32(), {Builtin(ast::Builtin::kSampleIndex)})});
  auto* param_a = Param("param_a", ty.Of(input_a));
  auto* param_b = Param("param_b", ty.Of(input_b));
  Func(Source{{12, 34}}, "main", {param_a, param_b}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(
      r()->error(),
      R"(12:34 error: builtin(sample_index) attribute appears multiple times as pipeline input
12:34 note: while analysing entry point 'main')");
}

TEST_F(ResolverEntryPointValidationTest, VertexShaderMustReturnPosition) {
  // [[stage(vertex)]]
  // fn main() {}
  Func(Source{{12, 34}}, "main", {}, ty.void_(), {},
       {Stage(ast::PipelineStage::kVertex)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: a vertex shader must include the 'position' builtin "
            "in its return type");
}

namespace TypeValidationTests {
struct Params {
  builder::ast_type_func_ptr create_ast_type;
  bool is_valid;
};

template <typename T>
constexpr Params ParamsFor(bool is_valid) {
  return Params{DataType<T>::AST, is_valid};
}

using TypeValidationTest = resolver::ResolverTestWithParam<Params>;

static constexpr Params cases[] = {
    ParamsFor<f32>(true),            //
    ParamsFor<i32>(true),            //
    ParamsFor<u32>(true),            //
    ParamsFor<bool>(false),          //
    ParamsFor<vec2<f32>>(true),      //
    ParamsFor<vec3<f32>>(true),      //
    ParamsFor<vec4<f32>>(true),      //
    ParamsFor<mat2x2<f32>>(false),   //
    ParamsFor<mat3x3<f32>>(false),   //
    ParamsFor<mat4x4<f32>>(false),   //
    ParamsFor<alias<f32>>(true),     //
    ParamsFor<alias<i32>>(true),     //
    ParamsFor<alias<u32>>(true),     //
    ParamsFor<alias<bool>>(false),   //
};

TEST_P(TypeValidationTest, BareInputs) {
  // [[stage(fragment)]]
  // fn main([[location(0)]] a : *) {}
  auto params = GetParam();
  auto* a = Param("a", params.create_ast_type(*this), {Location(0)});
  Func(Source{{12, 34}}, "main", {a}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  if (params.is_valid) {
    EXPECT_TRUE(r()->Resolve()) << r()->error();
  } else {
    EXPECT_FALSE(r()->Resolve());
  }
}

TEST_P(TypeValidationTest, StructInputs) {
  // struct Input {
  //   [[location(0)]] a : *;
  // };
  // [[stage(fragment)]]
  // fn main(a : Input) {}
  auto params = GetParam();
  auto* input = Structure(
      "Input", {Member("a", params.create_ast_type(*this), {Location(0)})});
  auto* a = Param("a", ty.Of(input), {});
  Func(Source{{12, 34}}, "main", {a}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  if (params.is_valid) {
    EXPECT_TRUE(r()->Resolve()) << r()->error();
  } else {
    EXPECT_FALSE(r()->Resolve());
  }
}

TEST_P(TypeValidationTest, BareOutputs) {
  // [[stage(fragment)]]
  // fn main() -> [[location(0)]] * {
  //   return *();
  // }
  auto params = GetParam();
  Func(Source{{12, 34}}, "main", {}, params.create_ast_type(*this),
       {Return(Construct(params.create_ast_type(*this)))},
       {Stage(ast::PipelineStage::kFragment)}, {Location(0)});

  if (params.is_valid) {
    EXPECT_TRUE(r()->Resolve()) << r()->error();
  } else {
    EXPECT_FALSE(r()->Resolve());
  }
}

TEST_P(TypeValidationTest, StructOutputs) {
  // struct Output {
  //   [[location(0)]] a : *;
  // };
  // [[stage(fragment)]]
  // fn main() -> Output {
  //   return Output();
  // }
  auto params = GetParam();
  auto* output = Structure(
      "Output", {Member("a", params.create_ast_type(*this), {Location(0)})});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))},
       {Stage(ast::PipelineStage::kFragment)});

  if (params.is_valid) {
    EXPECT_TRUE(r()->Resolve()) << r()->error();
  } else {
    EXPECT_FALSE(r()->Resolve());
  }
}
INSTANTIATE_TEST_SUITE_P(ResolverEntryPointValidationTest,
                         TypeValidationTest,
                         testing::ValuesIn(cases));

}  // namespace TypeValidationTests

namespace LocationDecorationTests {
namespace {
using LocationDecorationTests = ResolverTest;

TEST_F(LocationDecorationTests, Pass) {
  // [[stage(fragment)]]
  // fn frag_main([[location(0)]] a: i32) {}

  auto* p = Param(Source{{12, 34}}, "a", ty.i32(), {Location(0)});
  Func("frag_main", {p}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(LocationDecorationTests, BadType_Input_bool) {
  // [[stage(fragment)]]
  // fn frag_main([[location(0)]] a: bool) {}

  auto* p =
      Param(Source{{12, 34}}, "a", ty.bool_(), {Location(Source{{34, 56}}, 0)});
  Func("frag_main", {p}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: cannot apply 'location' attribute to declaration of "
            "type 'bool'\n"
            "34:56 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, BadType_Output_Array) {
  // [[stage(fragment)]]
  // fn frag_main()->[[location(0)]] array<f32, 2> { return array<f32, 2>(); }

  Func(Source{{12, 34}}, "frag_main", {}, ty.array<f32, 2>(),
       {Return(Construct(ty.array<f32, 2>()))},
       {Stage(ast::PipelineStage::kFragment)}, {Location(Source{{34, 56}}, 0)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: cannot apply 'location' attribute to declaration of "
            "type 'array<f32, 2>'\n"
            "34:56 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, BadType_Input_Struct) {
  // struct Input {
  //   a : f32;
  // };
  // [[stage(fragment)]]
  // fn main([[location(0)]] param : Input) {}
  auto* input = Structure("Input", {Member("a", ty.f32())});
  auto* param = Param(Source{{12, 34}}, "param", ty.Of(input),
                      {Location(Source{{13, 43}}, 0)});
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: cannot apply 'location' attribute to declaration of "
            "type 'Input'\n"
            "13:43 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, BadType_Input_Struct_NestedStruct) {
  // struct Inner {
  //   [[location(0)]] b : f32;
  // };
  // struct Input {
  //   [[location(0)]] a : Inner;
  // };
  // [[stage(fragment)]]
  // fn main(param : Input) {}
  auto* inner = Structure(
      "Inner", {Member(Source{{13, 43}}, "a", ty.f32(), {Location(0)})});
  auto* input = Structure("Input", {Member(Source{{14, 52}}, "a", ty.Of(inner),
                                           {Location(Source{{12, 34}}, 0)})});
  auto* param = Param("param", ty.Of(input));
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "14:52 error: cannot apply 'location' attribute to declaration of "
            "type 'Inner'\n"
            "12:34 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, BadType_Input_Struct_RuntimeArray) {
  // [[block]]
  // struct Input {
  //   [[location(0)]] a : array<f32>;
  // };
  // [[stage(fragment)]]
  // fn main(param : Input) {}
  auto* input = Structure(
      "Input",
      {Member(Source{{13, 43}}, "a", ty.array<float>(), {Location(0)})},
      {create<ast::StructBlockDecoration>()});
  auto* param = Param("param", ty.Of(input));
  Func(Source{{12, 34}}, "main", {param}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "13:43 error: cannot apply 'location' attribute to declaration of "
            "type 'array<f32>'\n"
            "note: 'location' attribute must only be applied to declarations "
            "of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, BadMemberType_Input) {
  // [[block]]
  // struct S { [[location(0)]] m: array<i32>; };
  // [[stage(fragment)]]
  // fn frag_main( a: S) {}

  auto* m = Member(Source{{34, 56}}, "m", ty.array<i32>(),
                   ast::DecorationList{Location(Source{{12, 34}}, 0u)});
  auto* s = Structure("S", {m}, ast::DecorationList{StructBlock()});
  auto* p = Param("a", ty.Of(s));

  Func("frag_main", {p}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "34:56 error: cannot apply 'location' attribute to declaration of "
            "type 'array<i32>'\n"
            "12:34 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, BadMemberType_Output) {
  // struct S { [[location(0)]] m: atomic<i32>; };
  // [[stage(fragment)]]
  // fn frag_main() -> S {}
  auto* m = Member(Source{{34, 56}}, "m", ty.atomic<i32>(),
                   ast::DecorationList{Location(Source{{12, 34}}, 0u)});
  auto* s = Structure("S", {m});

  Func("frag_main", {}, ty.Of(s), {Return(Construct(ty.Of(s)))},
       {Stage(ast::PipelineStage::kFragment)}, {});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "34:56 error: cannot apply 'location' attribute to declaration of "
            "type 'atomic<i32>'\n"
            "12:34 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, BadMemberType_Unused) {
  // struct S { [[location(0)]] m: mat3x2<f32>; };

  auto* m = Member(Source{{34, 56}}, "m", ty.mat3x2<f32>(),
                   ast::DecorationList{Location(Source{{12, 34}}, 0u)});
  Structure("S", {m});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "34:56 error: cannot apply 'location' attribute to declaration of "
            "type 'mat3x2<f32>'\n"
            "12:34 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, ReturnType_Struct_Valid) {
  // struct Output {
  //   [[location(0)]] a : f32;
  //   [[builtin(frag_depth)]] b : f32;
  // };
  // [[stage(fragment)]]
  // fn main() -> Output {
  //   return Output();
  // }
  auto* output = Structure(
      "Output", {Member("a", ty.f32(), {Location(0)}),
                 Member("b", ty.f32(), {Builtin(ast::Builtin::kFragDepth)})});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_TRUE(r()->Resolve()) << r()->error();
}

TEST_F(LocationDecorationTests, ReturnType_Struct) {
  // struct Output {
  //   a : f32;
  // };
  // [[stage(vertex)]]
  // fn main() -> [[location(0)]] Output {
  //   return Output();
  // }
  auto* output = Structure("Output", {Member("a", ty.f32())});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))}, {Stage(ast::PipelineStage::kVertex)},
       {Location(Source{{13, 43}}, 0)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: cannot apply 'location' attribute to declaration of "
            "type 'Output'\n"
            "13:43 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, ReturnType_Struct_NestedStruct) {
  // struct Inner {
  //   [[location(0)]] b : f32;
  // };
  // struct Output {
  //   [[location(0)]] a : Inner;
  // };
  auto* inner = Structure(
      "Inner", {Member(Source{{13, 43}}, "a", ty.f32(), {Location(0)})});
  Structure("Output", {Member(Source{{14, 52}}, "a", ty.Of(inner),
                              {Location(Source{{12, 34}}, 0)})});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "14:52 error: cannot apply 'location' attribute to declaration of "
            "type 'Inner'\n"
            "12:34 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, ReturnType_Struct_RuntimeArray) {
  // [[block]]
  // struct Output {
  //   [[location(0)]] a : array<f32>;
  // };
  // [[stage(fragment)]]
  // fn main() -> Output {
  //   return Output();
  // }
  auto* output = Structure("Output",
                           {Member(Source{{13, 43}}, "a", ty.array<float>(),
                                   {Location(Source{{12, 34}}, 0)})},
                           {create<ast::StructBlockDecoration>()});
  Func(Source{{12, 34}}, "main", {}, ty.Of(output),
       {Return(Construct(ty.Of(output)))},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "13:43 error: cannot apply 'location' attribute to declaration of "
            "type 'array<f32>'\n"
            "12:34 note: 'location' attribute must only be applied to "
            "declarations of numeric scalar or numeric vector type");
}

TEST_F(LocationDecorationTests, ComputeShaderLocation_Input) {
  Func("main", {}, ty.i32(), {Return(Expr(1))},
       {Stage(ast::PipelineStage::kCompute),
        create<ast::WorkgroupDecoration>(Source{{12, 34}}, Expr(1))},
       ast::DecorationList{Location(Source{{12, 34}}, 1)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: decoration is not valid for compute shader output");
}

TEST_F(LocationDecorationTests, ComputeShaderLocation_Output) {
  auto* input = Param("input", ty.i32(),
                      ast::DecorationList{Location(Source{{12, 34}}, 0u)});
  Func("main", {input}, ty.void_(), {},
       {Stage(ast::PipelineStage::kCompute),
        create<ast::WorkgroupDecoration>(Source{{12, 34}}, Expr(1))});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: decoration is not valid for compute shader inputs");
}

TEST_F(LocationDecorationTests, ComputeShaderLocationStructMember_Output) {
  auto* m = Member("m", ty.i32(),
                   ast::DecorationList{Location(Source{{12, 34}}, 0u)});
  auto* s = Structure("S", {m});
  Func(Source{{56, 78}}, "main", {}, ty.Of(s),
       ast::StatementList{Return(Expr(Construct(ty.Of(s))))},
       {Stage(ast::PipelineStage::kCompute),
        create<ast::WorkgroupDecoration>(Source{{12, 34}}, Expr(1))});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: decoration is not valid for compute shader output\n"
            "56:78 note: while analysing entry point 'main'");
}

TEST_F(LocationDecorationTests, ComputeShaderLocationStructMember_Input) {
  auto* m = Member("m", ty.i32(),
                   ast::DecorationList{Location(Source{{12, 34}}, 0u)});
  auto* s = Structure("S", {m});
  auto* input = Param("input", ty.Of(s));
  Func(Source{{56, 78}}, "main", {input}, ty.void_(), {},
       {Stage(ast::PipelineStage::kCompute),
        create<ast::WorkgroupDecoration>(Source{{12, 34}}, Expr(1))});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: decoration is not valid for compute shader inputs\n"
            "56:78 note: while analysing entry point 'main'");
}

TEST_F(LocationDecorationTests, Duplicate_input) {
  // [[stage(fragment)]]
  // fn main([[location(1)]] param_a : f32,
  //         [[location(1)]] param_b : f32) {}
  auto* param_a = Param("param_a", ty.u32(), {Location(1)});
  auto* param_b = Param("param_b", ty.u32(), {Location(Source{{12, 34}}, 1)});
  Func(Source{{12, 34}}, "main", {param_a, param_b}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "12:34 error: location(1) attribute appears multiple times");
}

TEST_F(LocationDecorationTests, Duplicate_struct) {
  // struct InputA {
  //   [[location(1)]] a : f32;
  // };
  // struct InputB {
  //   [[location(1)]] a : f32;
  // };
  // [[stage(fragment)]]
  // fn main(param_a : InputA, param_b : InputB) {}
  auto* input_a = Structure("InputA", {Member("a", ty.f32(), {Location(1)})});
  auto* input_b = Structure(
      "InputB", {Member("a", ty.f32(), {Location(Source{{34, 56}}, 1)})});
  auto* param_a = Param("param_a", ty.Of(input_a));
  auto* param_b = Param("param_b", ty.Of(input_b));
  Func(Source{{12, 34}}, "main", {param_a, param_b}, ty.void_(), {},
       {Stage(ast::PipelineStage::kFragment)});

  EXPECT_FALSE(r()->Resolve());
  EXPECT_EQ(r()->error(),
            "34:56 error: location(1) attribute appears multiple times\n"
            "12:34 note: while analysing entry point 'main'");
}

}  // namespace
}  // namespace LocationDecorationTests

}  // namespace
}  // namespace resolver
}  // namespace tint
