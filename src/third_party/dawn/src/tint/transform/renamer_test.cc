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

#include "src/tint/transform/renamer.h"

#include <memory>

#include "gmock/gmock.h"
#include "src/tint/transform/test_helper.h"

namespace tint::transform {
namespace {

constexpr const char kUnicodeIdentifier[] =  // "𝖎𝖉𝖊𝖓𝖙𝖎𝖋𝖎𝖊𝖗123"
    "\xf0\x9d\x96\x8e\xf0\x9d\x96\x89\xf0\x9d\x96\x8a\xf0\x9d\x96\x93"
    "\xf0\x9d\x96\x99\xf0\x9d\x96\x8e\xf0\x9d\x96\x8b\xf0\x9d\x96\x8e"
    "\xf0\x9d\x96\x8a\xf0\x9d\x96\x97\x31\x32\x33";

using ::testing::ContainerEq;

using RenamerTest = TransformTest;

TEST_F(RenamerTest, EmptyModule) {
    auto* src = "";
    auto* expect = "";

    auto got = Run<Renamer>(src);

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<Renamer::Data>();

    ASSERT_EQ(data->remappings.size(), 0u);
}

TEST_F(RenamerTest, BasicModuleVertexIndex) {
    auto* src = R"(
fn test(vert_idx : u32) -> u32 {
  return vert_idx;
}

@stage(vertex)
fn entry(@builtin(vertex_index) vert_idx : u32
        ) -> @builtin(position) vec4<f32>  {
  _ = test(vert_idx);
  return vec4<f32>();
}
)";

    auto* expect = R"(
fn tint_symbol(tint_symbol_1 : u32) -> u32 {
  return tint_symbol_1;
}

@stage(vertex)
fn tint_symbol_2(@builtin(vertex_index) tint_symbol_1 : u32) -> @builtin(position) vec4<f32> {
  _ = tint_symbol(tint_symbol_1);
  return vec4<f32>();
}
)";

    auto got = Run<Renamer>(src);

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<Renamer::Data>();

    ASSERT_NE(data, nullptr);
    Renamer::Data::Remappings expected_remappings = {
        {"vert_idx", "tint_symbol_1"},
        {"test", "tint_symbol"},
        {"entry", "tint_symbol_2"},
    };
    EXPECT_THAT(data->remappings, ContainerEq(expected_remappings));
}

TEST_F(RenamerTest, PreserveSwizzles) {
    auto* src = R"(
@stage(vertex)
fn entry() -> @builtin(position) vec4<f32> {
  var v : vec4<f32>;
  var rgba : f32;
  var xyzw : f32;
  return v.zyxw + v.rgab;
}
)";

    auto* expect = R"(
@stage(vertex)
fn tint_symbol() -> @builtin(position) vec4<f32> {
  var tint_symbol_1 : vec4<f32>;
  var tint_symbol_2 : f32;
  var tint_symbol_3 : f32;
  return (tint_symbol_1.zyxw + tint_symbol_1.rgab);
}
)";

    auto got = Run<Renamer>(src);

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<Renamer::Data>();

    ASSERT_NE(data, nullptr);
    Renamer::Data::Remappings expected_remappings = {
        {"entry", "tint_symbol"},
        {"v", "tint_symbol_1"},
        {"rgba", "tint_symbol_2"},
        {"xyzw", "tint_symbol_3"},
    };
    EXPECT_THAT(data->remappings, ContainerEq(expected_remappings));
}

TEST_F(RenamerTest, PreserveBuiltins) {
    auto* src = R"(
@stage(vertex)
fn entry() -> @builtin(position) vec4<f32> {
  var blah : vec4<f32>;
  return abs(blah);
}
)";

    auto* expect = R"(
@stage(vertex)
fn tint_symbol() -> @builtin(position) vec4<f32> {
  var tint_symbol_1 : vec4<f32>;
  return abs(tint_symbol_1);
}
)";

    auto got = Run<Renamer>(src);

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<Renamer::Data>();

    ASSERT_NE(data, nullptr);
    Renamer::Data::Remappings expected_remappings = {
        {"entry", "tint_symbol"},
        {"blah", "tint_symbol_1"},
    };
    EXPECT_THAT(data->remappings, ContainerEq(expected_remappings));
}

TEST_F(RenamerTest, PreserveBuiltinTypes) {
    auto* src = R"(
@stage(compute) @workgroup_size(1)
fn entry() {
  var a = modf(1.0).whole;
  var b = modf(1.0).fract;
  var c = frexp(1.0).sig;
  var d = frexp(1.0).exp;
}
)";

    auto* expect = R"(
@stage(compute) @workgroup_size(1)
fn tint_symbol() {
  var tint_symbol_1 = modf(1.0).whole;
  var tint_symbol_2 = modf(1.0).fract;
  var tint_symbol_3 = frexp(1.0).sig;
  var tint_symbol_4 = frexp(1.0).exp;
}
)";

    auto got = Run<Renamer>(src);

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<Renamer::Data>();

    ASSERT_NE(data, nullptr);
    Renamer::Data::Remappings expected_remappings = {
        {"entry", "tint_symbol"}, {"a", "tint_symbol_1"}, {"b", "tint_symbol_2"},
        {"c", "tint_symbol_3"},   {"d", "tint_symbol_4"},
    };
    EXPECT_THAT(data->remappings, ContainerEq(expected_remappings));
}

TEST_F(RenamerTest, PreserveUnicode) {
    auto src = R"(
@stage(fragment)
fn frag_main() {
  var )" + std::string(kUnicodeIdentifier) +
               R"( : i32;
}
)";

    auto expect = src;

    DataMap inputs;
    inputs.Add<Renamer::Config>(Renamer::Target::kMslKeywords,
                                /* preserve_unicode */ true);
    auto got = Run<Renamer>(src, inputs);

    EXPECT_EQ(expect, str(got));
}

TEST_F(RenamerTest, AttemptSymbolCollision) {
    auto* src = R"(
@stage(vertex)
fn entry() -> @builtin(position) vec4<f32> {
  var tint_symbol : vec4<f32>;
  var tint_symbol_2 : vec4<f32>;
  var tint_symbol_4 : vec4<f32>;
  return tint_symbol + tint_symbol_2 + tint_symbol_4;
}
)";

    auto* expect = R"(
@stage(vertex)
fn tint_symbol() -> @builtin(position) vec4<f32> {
  var tint_symbol_1 : vec4<f32>;
  var tint_symbol_2 : vec4<f32>;
  var tint_symbol_3 : vec4<f32>;
  return ((tint_symbol_1 + tint_symbol_2) + tint_symbol_3);
}
)";

    auto got = Run<Renamer>(src);

    EXPECT_EQ(expect, str(got));

    auto* data = got.data.Get<Renamer::Data>();

    ASSERT_NE(data, nullptr);
    Renamer::Data::Remappings expected_remappings = {
        {"entry", "tint_symbol"},
        {"tint_symbol", "tint_symbol_1"},
        {"tint_symbol_2", "tint_symbol_2"},
        {"tint_symbol_4", "tint_symbol_3"},
    };
    EXPECT_THAT(data->remappings, ContainerEq(expected_remappings));
}

using RenamerTestGlsl = TransformTestWithParam<std::string>;
using RenamerTestHlsl = TransformTestWithParam<std::string>;
using RenamerTestMsl = TransformTestWithParam<std::string>;

TEST_P(RenamerTestGlsl, Keywords) {
    auto keyword = GetParam();

    auto src = R"(
@stage(fragment)
fn frag_main() {
  var )" + keyword +
               R"( : i32;
}
)";

    auto* expect = R"(
@stage(fragment)
fn frag_main() {
  var tint_symbol : i32;
}
)";

    DataMap inputs;
    inputs.Add<Renamer::Config>(Renamer::Target::kGlslKeywords,
                                /* preserve_unicode */ false);
    auto got = Run<Renamer>(src, inputs);

    EXPECT_EQ(expect, str(got));
}

TEST_P(RenamerTestHlsl, Keywords) {
    auto keyword = GetParam();

    auto src = R"(
@stage(fragment)
fn frag_main() {
  var )" + keyword +
               R"( : i32;
}
)";

    auto* expect = R"(
@stage(fragment)
fn frag_main() {
  var tint_symbol : i32;
}
)";

    DataMap inputs;
    inputs.Add<Renamer::Config>(Renamer::Target::kHlslKeywords,
                                /* preserve_unicode */ false);
    auto got = Run<Renamer>(src, inputs);

    EXPECT_EQ(expect, str(got));
}

TEST_P(RenamerTestMsl, Keywords) {
    auto keyword = GetParam();

    auto src = R"(
@stage(fragment)
fn frag_main() {
  var )" + keyword +
               R"( : i32;
}
)";

    auto* expect = R"(
@stage(fragment)
fn frag_main() {
  var tint_symbol : i32;
}
)";

    DataMap inputs;
    inputs.Add<Renamer::Config>(Renamer::Target::kMslKeywords,
                                /* preserve_unicode */ false);
    auto got = Run<Renamer>(src, inputs);

    EXPECT_EQ(expect, str(got));
}

INSTANTIATE_TEST_SUITE_P(RenamerTestGlsl,
                         RenamerTestGlsl,
                         testing::Values("active",
                                         //    "asm",       // WGSL keyword
                                         "atomic_uint",
                                         "attribute",
                                         //    "bool",      // WGSL keyword
                                         //    "break",     // WGSL keyword
                                         "buffer",
                                         "bvec2",
                                         "bvec3",
                                         "bvec4",
                                         //    "case",      // WGSL keyword
                                         "cast",
                                         "centroid",
                                         "class",
                                         "coherent",
                                         "common",
                                         //    "const",     // WGSL keyword
                                         //    "continue",  // WGSL keyword
                                         //    "default",   // WGSL keyword
                                         //    "discard",   // WGSL keyword
                                         "dmat2",
                                         "dmat2x2",
                                         "dmat2x3",
                                         "dmat2x4",
                                         "dmat3",
                                         "dmat3x2",
                                         "dmat3x3",
                                         "dmat3x4",
                                         "dmat4",
                                         "dmat4x2",
                                         "dmat4x3",
                                         "dmat4x4",
                                         //    "do",         // WGSL keyword
                                         "double",
                                         "dvec2",
                                         "dvec3",
                                         "dvec4",
                                         //    "else"        // WGSL keyword
                                         //    "enum",       // WGSL keyword
                                         "extern",
                                         "external",
                                         //    "false",      // WGSL keyword
                                         "filter",
                                         "fixed",
                                         "flat",
                                         "float",
                                         //    "for",        // WGSL keyword
                                         "fvec2",
                                         "fvec3",
                                         "fvec4",
                                         "gl_BaseInstance",
                                         "gl_BaseVertex",
                                         "gl_ClipDistance",
                                         "gl_DepthRangeParameters",
                                         "gl_DrawID",
                                         "gl_FragCoord",
                                         "gl_FragDepth",
                                         "gl_FrontFacing",
                                         "gl_GlobalInvocationID",
                                         "gl_InstanceID",
                                         "gl_LocalInvocationID",
                                         "gl_LocalInvocationIndex",
                                         "gl_NumSamples",
                                         "gl_NumWorkGroups",
                                         "gl_PerVertex",
                                         "gl_PointCoord",
                                         "gl_PointSize",
                                         "gl_Position",
                                         "gl_PrimitiveID",
                                         "gl_SampleID",
                                         "gl_SampleMask",
                                         "gl_SampleMaskIn",
                                         "gl_SamplePosition",
                                         "gl_VertexID",
                                         "gl_WorkGroupID",
                                         "gl_WorkGroupSize",
                                         "goto",
                                         "half",
                                         "highp",
                                         "hvec2",
                                         "hvec3",
                                         "hvec4",
                                         //    "if",         // WGSL keyword
                                         "iimage1D",
                                         "iimage1DArray",
                                         "iimage2D",
                                         "iimage2DArray",
                                         "iimage2DMS",
                                         "iimage2DMSArray",
                                         "iimage2DRect",
                                         "iimage3D",
                                         "iimageBuffer",
                                         "iimageCube",
                                         "iimageCubeArray",
                                         "image1D",
                                         "image1DArray",
                                         "image2D",
                                         "image2DArray",
                                         "image2DMS",
                                         "image2DMSArray",
                                         "image2DRect",
                                         "image3D",
                                         "imageBuffer",
                                         "imageCube",
                                         "imageCubeArray",
                                         "in",
                                         "inline",
                                         "inout",
                                         "input",
                                         "int",
                                         "interface",
                                         "invariant",
                                         "isampler1D",
                                         "isampler1DArray",
                                         "isampler2D",
                                         "isampler2DArray",
                                         "isampler2DMS",
                                         "isampler2DMSArray",
                                         "isampler2DRect",
                                         "isampler3D",
                                         "isamplerBuffer",
                                         "isamplerCube",
                                         "isamplerCubeArray",
                                         "ivec2",
                                         "ivec3",
                                         "ivec4",
                                         "layout",
                                         "long",
                                         "lowp",
                                         //    "mat2x2",      // WGSL keyword
                                         //    "mat2x3",      // WGSL keyword
                                         //    "mat2x4",      // WGSL keyword
                                         //    "mat2",
                                         "mat3",
                                         //    "mat3x2",      // WGSL keyword
                                         //    "mat3x3",      // WGSL keyword
                                         //    "mat3x4",      // WGSL keyword
                                         "mat4",
                                         //    "mat4x2",      // WGSL keyword
                                         //    "mat4x3",      // WGSL keyword
                                         //    "mat4x4",      // WGSL keyword
                                         "mediump",
                                         "namespace",
                                         "noinline",
                                         "noperspective",
                                         "out",
                                         "output",
                                         "partition",
                                         "patch",
                                         "precise",
                                         "precision",
                                         "public",
                                         "readonly",
                                         "resource",
                                         "restrict",
                                         //    "return",     // WGSL keyword
                                         "sample",
                                         "sampler1D",
                                         "sampler1DArray",
                                         "sampler1DArrayShadow",
                                         "sampler1DShadow",
                                         "sampler2D",
                                         "sampler2DArray",
                                         "sampler2DArrayShadow",
                                         "sampler2DMS",
                                         "sampler2DMSArray",
                                         "sampler2DRect",
                                         "sampler2DRectShadow",
                                         "sampler2DShadow",
                                         "sampler3D",
                                         "sampler3DRect",
                                         "samplerBuffer",
                                         "samplerCube",
                                         "samplerCubeArray",
                                         "samplerCubeArrayShadow",
                                         "samplerCubeShadow",
                                         "shared",
                                         "short",
                                         "sizeof",
                                         "smooth",
                                         "static",
                                         //    "struct",     // WGSL keyword
                                         "subroutine",
                                         "superp",
                                         //    "switch",     // WGSL keyword
                                         "template",
                                         "this",
                                         //    "true",       // WGSL keyword
                                         //    "typedef",    // WGSL keyword
                                         "uimage1D",
                                         "uimage1DArray",
                                         "uimage2D",
                                         "uimage2DArray",
                                         "uimage2DMS",
                                         "uimage2DMSArray",
                                         "uimage2DRect",
                                         "uimage3D",
                                         "uimageBuffer",
                                         "uimageCube",
                                         "uimageCubeArray",
                                         "uint",
                                         //    "uniform",    // WGSL keyword
                                         "union",
                                         "unsigned",
                                         "usampler1D",
                                         "usampler1DArray",
                                         "usampler2D",
                                         "usampler2DArray",
                                         "usampler2DMS",
                                         "usampler2DMSArray",
                                         "usampler2DRect",
                                         "usampler3D",
                                         "usamplerBuffer",
                                         "usamplerCube",
                                         "usamplerCubeArray",
                                         //    "using",      // WGSL keyword
                                         "uvec2",
                                         "uvec3",
                                         "uvec4",
                                         "varying",
                                         //    "vec2",       // WGSL keyword
                                         //    "vec3",       // WGSL keyword
                                         //    "vec4",       // WGSL keyword
                                         //    "void",       // WGSL keyword
                                         "volatile",
                                         //    "while",      // WGSL keyword
                                         "writeonly",
                                         kUnicodeIdentifier));

INSTANTIATE_TEST_SUITE_P(RenamerTestHlsl,
                         RenamerTestHlsl,
                         testing::Values("AddressU",
                                         "AddressV",
                                         "AddressW",
                                         "AllMemoryBarrier",
                                         "AllMemoryBarrierWithGroupSync",
                                         "AppendStructuredBuffer",
                                         "BINORMAL",
                                         "BLENDINDICES",
                                         "BLENDWEIGHT",
                                         "BlendState",
                                         "BorderColor",
                                         "Buffer",
                                         "ByteAddressBuffer",
                                         "COLOR",
                                         "CheckAccessFullyMapped",
                                         "ComparisonFunc",
                                         "CompileShader",
                                         "ComputeShader",
                                         "ConsumeStructuredBuffer",
                                         "D3DCOLORtoUBYTE4",
                                         "DEPTH",
                                         "DepthStencilState",
                                         "DepthStencilView",
                                         "DeviceMemoryBarrier",
                                         "DeviceMemroyBarrierWithGroupSync",
                                         "DomainShader",
                                         "EvaluateAttributeAtCentroid",
                                         "EvaluateAttributeAtSample",
                                         "EvaluateAttributeSnapped",
                                         "FOG",
                                         "Filter",
                                         "GeometryShader",
                                         "GetRenderTargetSampleCount",
                                         "GetRenderTargetSamplePosition",
                                         "GroupMemoryBarrier",
                                         "GroupMemroyBarrierWithGroupSync",
                                         "Hullshader",
                                         "InputPatch",
                                         "InterlockedAdd",
                                         "InterlockedAnd",
                                         "InterlockedCompareExchange",
                                         "InterlockedCompareStore",
                                         "InterlockedExchange",
                                         "InterlockedMax",
                                         "InterlockedMin",
                                         "InterlockedOr",
                                         "InterlockedXor",
                                         "LineStream",
                                         "MaxAnisotropy",
                                         "MaxLOD",
                                         "MinLOD",
                                         "MipLODBias",
                                         "NORMAL",
                                         "NULL",
                                         "Normal",
                                         "OutputPatch",
                                         "POSITION",
                                         "POSITIONT",
                                         "PSIZE",
                                         "PixelShader",
                                         "PointStream",
                                         "Process2DQuadTessFactorsAvg",
                                         "Process2DQuadTessFactorsMax",
                                         "Process2DQuadTessFactorsMin",
                                         "ProcessIsolineTessFactors",
                                         "ProcessQuadTessFactorsAvg",
                                         "ProcessQuadTessFactorsMax",
                                         "ProcessQuadTessFactorsMin",
                                         "ProcessTriTessFactorsAvg",
                                         "ProcessTriTessFactorsMax",
                                         "ProcessTriTessFactorsMin",
                                         "RWBuffer",
                                         "RWByteAddressBuffer",
                                         "RWStructuredBuffer",
                                         "RWTexture1D",
                                         "RWTexture1DArray",
                                         "RWTexture2D",
                                         "RWTexture2DArray",
                                         "RWTexture3D",
                                         "RasterizerState",
                                         "RenderTargetView",
                                         "SV_ClipDistance",
                                         "SV_Coverage",
                                         "SV_CullDistance",
                                         "SV_Depth",
                                         "SV_DepthGreaterEqual",
                                         "SV_DepthLessEqual",
                                         "SV_DispatchThreadID",
                                         "SV_DomainLocation",
                                         "SV_GSInstanceID",
                                         "SV_GroupID",
                                         "SV_GroupIndex",
                                         "SV_GroupThreadID",
                                         "SV_InnerCoverage",
                                         "SV_InsideTessFactor",
                                         "SV_InstanceID",
                                         "SV_IsFrontFace",
                                         "SV_OutputControlPointID",
                                         "SV_Position",
                                         "SV_PrimitiveID",
                                         "SV_RenderTargetArrayIndex",
                                         "SV_SampleIndex",
                                         "SV_StencilRef",
                                         "SV_Target",
                                         "SV_TessFactor",
                                         "SV_VertexArrayIndex",
                                         "SV_VertexID",
                                         "Sampler",
                                         "Sampler1D",
                                         "Sampler2D",
                                         "Sampler3D",
                                         "SamplerCUBE",
                                         "SamplerComparisonState",
                                         "SamplerState",
                                         "StructuredBuffer",
                                         "TANGENT",
                                         "TESSFACTOR",
                                         "TEXCOORD",
                                         "Texcoord",
                                         "Texture",
                                         "Texture1D",
                                         "Texture1DArray",
                                         "Texture2D",
                                         "Texture2DArray",
                                         "Texture2DMS",
                                         "Texture2DMSArray",
                                         "Texture3D",
                                         "TextureCube",
                                         "TextureCubeArray",
                                         "TriangleStream",
                                         "VFACE",
                                         "VPOS",
                                         "VertexShader",
                                         "abort",
                                         // "abs",  // WGSL builtin
                                         // "acos",  // WGSL builtin
                                         // "all",  // WGSL builtin
                                         "allow_uav_condition",
                                         // "any",  // WGSL builtin
                                         "asdouble",
                                         "asfloat",
                                         // "asin",  // WGSL builtin
                                         "asint",
                                         // "asm",  // WGSL keyword
                                         "asm_fragment",
                                         "asuint",
                                         // "atan",  // WGSL builtin
                                         // "atan2",  // WGSL builtin
                                         "auto",
                                         // "bool",  // WGSL keyword
                                         "bool1",
                                         "bool1x1",
                                         "bool1x2",
                                         "bool1x3",
                                         "bool1x4",
                                         "bool2",
                                         "bool2x1",
                                         "bool2x2",
                                         "bool2x3",
                                         "bool2x4",
                                         "bool3",
                                         "bool3x1",
                                         "bool3x2",
                                         "bool3x3",
                                         "bool3x4",
                                         "bool4",
                                         "bool4x1",
                                         "bool4x2",
                                         "bool4x3",
                                         "bool4x4",
                                         "branch",
                                         // "break",  // WGSL keyword
                                         // "call",  // WGSL builtin
                                         // "case",  // WGSL keyword
                                         "catch",
                                         "cbuffer",
                                         // "ceil",  // WGSL builtin
                                         "centroid",
                                         "char",
                                         // "clamp",  // WGSL builtin
                                         "class",
                                         "clip",
                                         "column_major",
                                         "compile",
                                         "compile_fragment",
                                         // "const",  // WGSL keyword
                                         "const_cast",
                                         // "continue",  // WGSL keyword
                                         // "cos",  // WGSL builtin
                                         // "cosh",  // WGSL builtin
                                         "countbits",
                                         // "cross",  // WGSL builtin
                                         "ddx",
                                         "ddx_coarse",
                                         "ddx_fine",
                                         "ddy",
                                         "ddy_coarse",
                                         "ddy_fine",
                                         // "default",  // WGSL keyword
                                         "degrees",
                                         "delete",
                                         // "determinant",  // WGSL builtin
                                         // "discard",  // WGSL keyword
                                         // "distance",  // WGSL builtin
                                         // "do",  // WGSL keyword
                                         // "dot",  // WGSL builtin
                                         "double",
                                         "double1",
                                         "double1x1",
                                         "double1x2",
                                         "double1x3",
                                         "double1x4",
                                         "double2",
                                         "double2x1",
                                         "double2x2",
                                         "double2x3",
                                         "double2x4",
                                         "double3",
                                         "double3x1",
                                         "double3x2",
                                         "double3x3",
                                         "double3x4",
                                         "double4",
                                         "double4x1",
                                         "double4x2",
                                         "double4x3",
                                         "double4x4",
                                         "dst",
                                         "dword",
                                         "dword1",
                                         "dword1x1",
                                         "dword1x2",
                                         "dword1x3",
                                         "dword1x4",
                                         "dword2",
                                         "dword2x1",
                                         "dword2x2",
                                         "dword2x3",
                                         "dword2x4",
                                         "dword3",
                                         "dword3x1",
                                         "dword3x2",
                                         "dword3x3",
                                         "dword3x4",
                                         "dword4",
                                         "dword4x1",
                                         "dword4x2",
                                         "dword4x3",
                                         "dword4x4",
                                         "dynamic_cast",
                                         // "else",  // WGSL keyword
                                         // "enum",  // WGSL keyword
                                         "errorf",
                                         // "exp",  // WGSL builtin
                                         // "exp2",  // WGSL builtin
                                         "explicit",
                                         "export",
                                         "extern",
                                         "f16to32",
                                         "f32tof16",
                                         // "faceforward",  // WGSL builtin
                                         // "false",  // WGSL keyword
                                         "fastopt",
                                         "firstbithigh",
                                         "firstbitlow",
                                         "flatten",
                                         "float",
                                         "float1",
                                         "float1x1",
                                         "float1x2",
                                         "float1x3",
                                         "float1x4",
                                         "float2",
                                         "float2x1",
                                         "float2x2",
                                         "float2x3",
                                         "float2x4",
                                         "float3",
                                         "float3x1",
                                         "float3x2",
                                         "float3x3",
                                         "float3x4",
                                         "float4",
                                         "float4x1",
                                         "float4x2",
                                         "float4x3",
                                         "float4x4",
                                         // "floor",  // WGSL builtin
                                         // "fma",  // WGSL builtin
                                         "fmod",
                                         // "for",  // WGSL keyword
                                         "forcecase",
                                         "frac",
                                         // "frexp",  // WGSL builtin
                                         "friend",
                                         // "fwidth",  // WGSL builtin
                                         "fxgroup",
                                         "goto",
                                         "groupshared",
                                         "half",
                                         "half1",
                                         "half1x1",
                                         "half1x2",
                                         "half1x3",
                                         "half1x4",
                                         "half2",
                                         "half2x1",
                                         "half2x2",
                                         "half2x3",
                                         "half2x4",
                                         "half3",
                                         "half3x1",
                                         "half3x2",
                                         "half3x3",
                                         "half3x4",
                                         "half4",
                                         "half4x1",
                                         "half4x2",
                                         "half4x3",
                                         "half4x4",
                                         // "if",  // WGSL keyword
                                         // "in",  // WGSL keyword
                                         "inline",
                                         "inout",
                                         "int",
                                         "int1",
                                         "int1x1",
                                         "int1x2",
                                         "int1x3",
                                         "int1x4",
                                         "int2",
                                         "int2x1",
                                         "int2x2",
                                         "int2x3",
                                         "int2x4",
                                         "int3",
                                         "int3x1",
                                         "int3x2",
                                         "int3x3",
                                         "int3x4",
                                         "int4",
                                         "int4x1",
                                         "int4x2",
                                         "int4x3",
                                         "int4x4",
                                         "interface",
                                         "isfinite",
                                         "isinf",
                                         "isnan",
                                         // "ldexp",  // WGSL builtin
                                         // "length",  // WGSL builtin
                                         "lerp",
                                         "line",
                                         "lineadj",
                                         "linear",
                                         "lit",
                                         // "log",  // WGSL builtin
                                         "log10",
                                         // "log2",  // WGSL builtin
                                         "long",
                                         // "loop",  // WGSL keyword
                                         "mad",
                                         "matrix",
                                         // "max",  // WGSL builtin
                                         // "min",  // WGSL builtin
                                         "min10float",
                                         "min10float1",
                                         "min10float1x1",
                                         "min10float1x2",
                                         "min10float1x3",
                                         "min10float1x4",
                                         "min10float2",
                                         "min10float2x1",
                                         "min10float2x2",
                                         "min10float2x3",
                                         "min10float2x4",
                                         "min10float3",
                                         "min10float3x1",
                                         "min10float3x2",
                                         "min10float3x3",
                                         "min10float3x4",
                                         "min10float4",
                                         "min10float4x1",
                                         "min10float4x2",
                                         "min10float4x3",
                                         "min10float4x4",
                                         "min12int",
                                         "min12int1",
                                         "min12int1x1",
                                         "min12int1x2",
                                         "min12int1x3",
                                         "min12int1x4",
                                         "min12int2",
                                         "min12int2x1",
                                         "min12int2x2",
                                         "min12int2x3",
                                         "min12int2x4",
                                         "min12int3",
                                         "min12int3x1",
                                         "min12int3x2",
                                         "min12int3x3",
                                         "min12int3x4",
                                         "min12int4",
                                         "min12int4x1",
                                         "min12int4x2",
                                         "min12int4x3",
                                         "min12int4x4",
                                         "min16float",
                                         "min16float1",
                                         "min16float1x1",
                                         "min16float1x2",
                                         "min16float1x3",
                                         "min16float1x4",
                                         "min16float2",
                                         "min16float2x1",
                                         "min16float2x2",
                                         "min16float2x3",
                                         "min16float2x4",
                                         "min16float3",
                                         "min16float3x1",
                                         "min16float3x2",
                                         "min16float3x3",
                                         "min16float3x4",
                                         "min16float4",
                                         "min16float4x1",
                                         "min16float4x2",
                                         "min16float4x3",
                                         "min16float4x4",
                                         "min16int",
                                         "min16int1",
                                         "min16int1x1",
                                         "min16int1x2",
                                         "min16int1x3",
                                         "min16int1x4",
                                         "min16int2",
                                         "min16int2x1",
                                         "min16int2x2",
                                         "min16int2x3",
                                         "min16int2x4",
                                         "min16int3",
                                         "min16int3x1",
                                         "min16int3x2",
                                         "min16int3x3",
                                         "min16int3x4",
                                         "min16int4",
                                         "min16int4x1",
                                         "min16int4x2",
                                         "min16int4x3",
                                         "min16int4x4",
                                         "min16uint",
                                         "min16uint1",
                                         "min16uint1x1",
                                         "min16uint1x2",
                                         "min16uint1x3",
                                         "min16uint1x4",
                                         "min16uint2",
                                         "min16uint2x1",
                                         "min16uint2x2",
                                         "min16uint2x3",
                                         "min16uint2x4",
                                         "min16uint3",
                                         "min16uint3x1",
                                         "min16uint3x2",
                                         "min16uint3x3",
                                         "min16uint3x4",
                                         "min16uint4",
                                         "min16uint4x1",
                                         "min16uint4x2",
                                         "min16uint4x3",
                                         "min16uint4x4",
                                         // "modf",  // WGSL builtin
                                         "msad4",
                                         "mul",
                                         "mutable",
                                         "namespace",
                                         "new",
                                         "nointerpolation",
                                         "noise",
                                         "noperspective",
                                         // "normalize",  // WGSL builtin
                                         "numthreads",
                                         "operator",
                                         // "out",  // WGSL keyword
                                         "packoffset",
                                         "pass",
                                         "pixelfragment",
                                         "pixelshader",
                                         "point",
                                         // "pow",  // WGSL builtin
                                         "precise",
                                         "printf",
                                         // "private",  // WGSL keyword
                                         "protected",
                                         "public",
                                         "radians",
                                         "rcp",
                                         // "reflect",  // WGSL builtin
                                         "refract",
                                         "register",
                                         "reinterpret_cast",
                                         // "return",  // WGSL keyword
                                         // "reversebits",  // WGSL builtin
                                         // "round",  // WGSL builtin
                                         "row_major",
                                         "rsqrt",
                                         "sample",
                                         "sampler1D",
                                         "sampler2D",
                                         "sampler3D",
                                         "samplerCUBE",
                                         "sampler_state",
                                         "saturate",
                                         "shared",
                                         "short",
                                         // "sign",  // WGSL builtin
                                         "signed",
                                         // "sin",  // WGSL builtin
                                         "sincos",
                                         // "sinh",  // WGSL builtin
                                         "sizeof",
                                         // "smoothstep",  // WGSL builtin
                                         "snorm",
                                         // "sqrt",  // WGSL builtin
                                         "stateblock",
                                         "stateblock_state",
                                         "static",
                                         "static_cast",
                                         // "step",  // WGSL builtin
                                         "string",
                                         // "struct",  // WGSL keyword
                                         // "switch",  // WGSL keyword
                                         // "tan",  // WGSL builtin
                                         // "tanh",  // WGSL builtin
                                         "tbuffer",
                                         "technique",
                                         "technique10",
                                         "technique11",
                                         "template",
                                         "tex1D",
                                         "tex1Dbias",
                                         "tex1Dgrad",
                                         "tex1Dlod",
                                         "tex1Dproj",
                                         "tex2D",
                                         "tex2Dbias",
                                         "tex2Dgrad",
                                         "tex2Dlod",
                                         "tex2Dproj",
                                         "tex3D",
                                         "tex3Dbias",
                                         "tex3Dgrad",
                                         "tex3Dlod",
                                         "tex3Dproj",
                                         "texCUBE",
                                         "texCUBEbias",
                                         "texCUBEgrad",
                                         "texCUBElod",
                                         "texCUBEproj",
                                         "texture",
                                         "texture1D",
                                         "texture1DArray",
                                         "texture2D",
                                         "texture2DArray",
                                         "texture2DMS",
                                         "texture2DMSArray",
                                         "texture3D",
                                         "textureCube",
                                         "textureCubeArray",
                                         "this",
                                         "throw",
                                         "transpose",
                                         "triangle",
                                         "triangleadj",
                                         // "true",  // WGSL keyword
                                         // "trunc",  // WGSL builtin
                                         "try",
                                         // "typedef",  // WGSL keyword
                                         "typename",
                                         "uint",
                                         "uint1",
                                         "uint1x1",
                                         "uint1x2",
                                         "uint1x3",
                                         "uint1x4",
                                         "uint2",
                                         "uint2x1",
                                         "uint2x2",
                                         "uint2x3",
                                         "uint2x4",
                                         "uint3",
                                         "uint3x1",
                                         "uint3x2",
                                         "uint3x3",
                                         "uint3x4",
                                         "uint4",
                                         "uint4x1",
                                         "uint4x2",
                                         "uint4x3",
                                         "uint4x4",
                                         // "uniform",  // WGSL keyword
                                         "union",
                                         "unorm",
                                         "unroll",
                                         "unsigned",
                                         // "using",  // WGSL reserved keyword
                                         "vector",
                                         "vertexfragment",
                                         "vertexshader",
                                         "virtual",
                                         // "void",  // WGSL keyword
                                         "volatile",
                                         // "while"  // WGSL reserved keyword
                                         kUnicodeIdentifier));

INSTANTIATE_TEST_SUITE_P(
    RenamerTestMsl,
    RenamerTestMsl,
    testing::Values(
        // c++14 spec
        "alignas",
        "alignof",
        "and",
        "and_eq",
        // "asm",  // Also reserved in WGSL
        "auto",
        "bitand",
        "bitor",
        // "bool",   // Also used in WGSL
        // "break",  // Also used in WGSL
        // "case",   // Also used in WGSL
        "catch",
        "char",
        "char16_t",
        "char32_t",
        "class",
        "compl",
        // "const",     // Also used in WGSL
        "const_cast",
        "constexpr",
        // "continue",  // Also used in WGSL
        "decltype",
        // "default",   // Also used in WGSL
        "delete",
        // "do",  // Also used in WGSL
        "double",
        "dynamic_cast",
        // "else",  // Also used in WGSL
        // "enum",  // Also used in WGSL
        "explicit",
        "extern",
        // "false",  // Also used in WGSL
        "final",
        "float",
        // "for",  // Also used in WGSL
        "friend",
        "goto",
        // "if",  // Also used in WGSL
        "inline",
        "int",
        "long",
        "mutable",
        "namespace",
        "new",
        "noexcept",
        "not",
        "not_eq",
        "nullptr",
        "operator",
        "or",
        "or_eq",
        // "override", // Also used in WGSL
        // "private",  // Also used in WGSL
        "protected",
        "public",
        "register",
        "reinterpret_cast",
        // "return",  // Also used in WGSL
        "short",
        "signed",
        "sizeof",
        "static",
        "static_assert",
        "static_cast",
        // "struct",  // Also used in WGSL
        // "switch",  // Also used in WGSL
        "template",
        "this",
        "thread_local",
        "throw",
        // "true",  // Also used in WGSL
        "try",
        // "typedef",  // Also used in WGSL
        "typeid",
        "typename",
        "union",
        "unsigned",
        // "using",  // WGSL reserved keyword
        "virtual",
        // "void",  // Also used in WGSL
        "volatile",
        "wchar_t",
        // "while",  // WGSL reserved keyword
        "xor",
        "xor_eq",

        // Metal Spec
        "access",
        // "array",  // Also used in WGSL
        "array_ref",
        "as_type",
        // "atomic",  // Also used in WGSL
        "atomic_bool",
        "atomic_int",
        "atomic_uint",
        "bool2",
        "bool3",
        "bool4",
        "buffer",
        "char2",
        "char3",
        "char4",
        "const_reference",
        "constant",
        "depth2d",
        "depth2d_array",
        "depth2d_ms",
        "depth2d_ms_array",
        "depthcube",
        "depthcube_array",
        "device",
        "discard_fragment",
        "float2",
        "float2x2",
        "float2x3",
        "float2x4",
        "float3",
        "float3x2",
        "float3x3",
        "float3x4",
        "float4",
        "float4x2",
        "float4x3",
        "float4x4",
        "fragment",
        "half",
        "half2",
        "half2x2",
        "half2x3",
        "half2x4",
        "half3",
        "half3x2",
        "half3x3",
        "half3x4",
        "half4",
        "half4x2",
        "half4x3",
        "half4x4",
        "imageblock",
        "int16_t",
        "int2",
        "int3",
        "int32_t",
        "int4",
        "int64_t",
        "int8_t",
        "kernel",
        "long2",
        "long3",
        "long4",
        "main",  // No functions called main
        "matrix",
        "metal",  // The namespace
        "packed_bool2",
        "packed_bool3",
        "packed_bool4",
        "packed_char2",
        "packed_char3",
        "packed_char4",
        "packed_float2",
        "packed_float3",
        "packed_float4",
        "packed_half2",
        "packed_half3",
        "packed_half4",
        "packed_int2",
        "packed_int3",
        "packed_int4",
        "packed_short2",
        "packed_short3",
        "packed_short4",
        "packed_uchar2",
        "packed_uchar3",
        "packed_uchar4",
        "packed_uint2",
        "packed_uint3",
        "packed_uint4",
        "packed_ushort2",
        "packed_ushort3",
        "packed_ushort4",
        "patch_control_point",
        "ptrdiff_t",
        "r16snorm",
        "r16unorm",
        "r8unorm",
        "reference",
        "rg11b10f",
        "rg16snorm",
        "rg16unorm",
        "rg8snorm",
        "rg8unorm",
        "rgb10a2",
        "rgb9e5",
        "rgba16snorm",
        "rgba16unorm",
        "rgba8snorm",
        "rgba8unorm",
        // "sampler",  // Also used in WGSL
        "short2",
        "short3",
        "short4",
        "size_t",
        "srgba8unorm",
        "texture",
        "texture1d",
        "texture1d_array",
        "texture2d",
        "texture2d_array",
        "texture2d_ms",
        "texture2d_ms_array",
        "texture3d",
        "texture_buffer",
        "texturecube",
        "texturecube_array",
        "thread",
        "threadgroup",
        "threadgroup_imageblock",
        "uchar",
        "uchar2",
        "uchar3",
        "uchar4",
        "uint",
        "uint16_t",
        "uint2",
        "uint3",
        "uint32_t",
        "uint4",
        "uint64_t",
        "uint8_t",
        "ulong2",
        "ulong3",
        "ulong4",
        // "uniform",  // Also used in WGSL
        "ushort",
        "ushort2",
        "ushort3",
        "ushort4",
        // "vec",  // WGSL reserved keyword
        "vertex",

        // https://developer.apple.com/metal/Metal-Shading-Language-Specification.pdf
        // Table 6.5. Constants for single-precision floating-point math
        // functions
        "MAXFLOAT",
        "HUGE_VALF",
        "INFINITY",
        "infinity",
        "NAN",
        "M_E_F",
        "M_LOG2E_F",
        "M_LOG10E_F",
        "M_LN2_F",
        "M_LN10_F",
        "M_PI_F",
        "M_PI_2_F",
        "M_PI_4_F",
        "M_1_PI_F",
        "M_2_PI_F",
        "M_2_SQRTPI_F",
        "M_SQRT2_F",
        "M_SQRT1_2_F",
        "MAXHALF",
        "HUGE_VALH",
        "M_E_H",
        "M_LOG2E_H",
        "M_LOG10E_H",
        "M_LN2_H",
        "M_LN10_H",
        "M_PI_H",
        "M_PI_2_H",
        "M_PI_4_H",
        "M_1_PI_H",
        "M_2_PI_H",
        "M_2_SQRTPI_H",
        "M_SQRT2_H",
        "M_SQRT1_2_H",
        // "while"  // WGSL reserved keyword
        kUnicodeIdentifier));

}  // namespace
}  // namespace tint::transform
