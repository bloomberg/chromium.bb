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

#include "src/ast/struct.h"
#include "src/ast/struct_block_decoration.h"
#include "src/ast/struct_decoration.h"
#include "src/ast/struct_member.h"
#include "src/ast/struct_member_decoration.h"
#include "src/ast/struct_member_offset_decoration.h"
#include "src/program.h"
#include "src/type/access_control_type.h"
#include "src/type/array_type.h"
#include "src/type/bool_type.h"
#include "src/type/depth_texture_type.h"
#include "src/type/f32_type.h"
#include "src/type/i32_type.h"
#include "src/type/matrix_type.h"
#include "src/type/multisampled_texture_type.h"
#include "src/type/pointer_type.h"
#include "src/type/sampled_texture_type.h"
#include "src/type/sampler_type.h"
#include "src/type/storage_texture_type.h"
#include "src/type/struct_type.h"
#include "src/type/u32_type.h"
#include "src/type/vector_type.h"
#include "src/type/void_type.h"
#include "src/writer/hlsl/test_helper.h"

namespace tint {
namespace writer {
namespace hlsl {
namespace {

using HlslGeneratorImplTest_Type = TestHelper;

TEST_F(HlslGeneratorImplTest_Type, EmitType_Alias) {
  auto* alias = ty.alias("alias", ty.f32());

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, alias, "")) << gen.error();
  EXPECT_EQ(result(), "alias");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Alias_NameCollision) {
  auto* alias = ty.alias("bool", ty.f32());

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, alias, "")) << gen.error();
  EXPECT_EQ(result(), "bool_tint_0");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Array) {
  auto* arr = ty.array<bool, 4>();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "ary")) << gen.error();
  EXPECT_EQ(result(), "bool ary[4]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_ArrayOfArray) {
  auto* arr = ty.array(ty.array<bool, 4>(), 5);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "ary")) << gen.error();
  EXPECT_EQ(result(), "bool ary[5][4]");
}

// TODO(dsinclair): Is this possible? What order should it output in?
TEST_F(HlslGeneratorImplTest_Type,
       DISABLED_EmitType_ArrayOfArrayOfRuntimeArray) {
  auto* arr = ty.array(ty.array(ty.array<bool, 4>(), 5), 0);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "ary")) << gen.error();
  EXPECT_EQ(result(), "bool ary[5][4][1]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_ArrayOfArrayOfArray) {
  auto* arr = ty.array(ty.array(ty.array<bool, 4>(), 5), 6);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "ary")) << gen.error();
  EXPECT_EQ(result(), "bool ary[6][5][4]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Array_NameCollision) {
  auto* arr = ty.array<bool, 4>();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "bool")) << gen.error();
  EXPECT_EQ(result(), "bool bool_tint_0[4]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Array_WithoutName) {
  auto* arr = ty.array<bool, 4>();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "")) << gen.error();
  EXPECT_EQ(result(), "bool[4]");
}

TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_RuntimeArray) {
  auto* arr = ty.array<bool>();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "ary")) << gen.error();
  EXPECT_EQ(result(), "bool ary[]");
}

TEST_F(HlslGeneratorImplTest_Type,
       DISABLED_EmitType_RuntimeArray_NameCollision) {
  auto* arr = ty.array<bool>();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, arr, "double")) << gen.error();
  EXPECT_EQ(result(), "bool double_tint_0[]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Bool) {
  auto* bool_ = ty.bool_();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, bool_, "")) << gen.error();
  EXPECT_EQ(result(), "bool");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_F32) {
  auto* f32 = ty.f32();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, f32, "")) << gen.error();
  EXPECT_EQ(result(), "float");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_I32) {
  auto* i32 = ty.i32();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, i32, "")) << gen.error();
  EXPECT_EQ(result(), "int");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Matrix) {
  auto* mat2x3 = ty.mat2x3<f32>();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, mat2x3, "")) << gen.error();
  EXPECT_EQ(result(), "float3x2");
}

// TODO(dsinclair): How to annotate as workgroup?
TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_Pointer) {
  type::Pointer p(ty.f32(), ast::StorageClass::kWorkgroup);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, &p, "")) << gen.error();
  EXPECT_EQ(result(), "float*");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_StructDecl) {
  auto* str = create<ast::Struct>(
      ast::StructMemberList{Member("a", ty.i32()),
                            Member("b", ty.f32(), {MemberOffset(4)})},
      ast::StructDecorationList{});

  auto* s = ty.struct_("S", str);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStructType(out, s, "S")) << gen.error();
  EXPECT_EQ(result(), R"(struct S {
  int a;
  float b;
};
)");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Struct) {
  auto* str = create<ast::Struct>(
      ast::StructMemberList{Member("a", ty.i32()),
                            Member("b", ty.f32(), {MemberOffset(4)})},
      ast::StructDecorationList{});

  auto* s = ty.struct_("S", str);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, s, "")) << gen.error();
  EXPECT_EQ(result(), "S");
}

TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_Struct_InjectPadding) {
  auto* str = create<ast::Struct>(
      ast::StructMemberList{Member("a", ty.i32(), {MemberOffset(4)}),
                            Member("b", ty.f32(), {MemberOffset(32)}),
                            Member("c", ty.f32(), {MemberOffset(128)})},
      ast::StructDecorationList{});

  auto* s = ty.struct_("S", str);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, s, "")) << gen.error();
  EXPECT_EQ(result(), R"(struct {
  int8_t pad_0[4];
  int a;
  int8_t pad_1[24];
  float b;
  int8_t pad_2[92];
  float c;
})");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Struct_NameCollision) {
  auto* str =
      create<ast::Struct>(ast::StructMemberList{Member("double", ty.i32()),
                                                Member("float", ty.f32())},
                          ast::StructDecorationList{});

  auto* s = ty.struct_("S", str);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStructType(out, s, "S")) << gen.error();
  EXPECT_EQ(result(), R"(struct S {
  int double_tint_0;
  float float_tint_0;
};
)");
}

// TODO(dsinclair): How to translate [[block]]
TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_Struct_WithDecoration) {
  ast::StructDecorationList decos;
  decos.push_back(create<ast::StructBlockDecoration>());

  auto* str = create<ast::Struct>(
      ast::StructMemberList{Member("a", ty.i32()),
                            Member("b", ty.f32(), {MemberOffset(4)})},
      decos);

  auto* s = ty.struct_("S", str);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitStructType(out, s, "B")) << gen.error();
  EXPECT_EQ(result(), R"(struct B {
  int a;
  float b;
})");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_U32) {
  auto* u32 = ty.u32();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, u32, "")) << gen.error();
  EXPECT_EQ(result(), "uint");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Vector) {
  auto* vec3 = ty.vec3<f32>();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, vec3, "")) << gen.error();
  EXPECT_EQ(result(), "float3");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Void) {
  auto* void_ = ty.void_();

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, void_, "")) << gen.error();
  EXPECT_EQ(result(), "void");
}

TEST_F(HlslGeneratorImplTest_Type, EmitSampler) {
  type::Sampler sampler(type::SamplerKind::kSampler);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, &sampler, "")) << gen.error();
  EXPECT_EQ(result(), "SamplerState");
}

TEST_F(HlslGeneratorImplTest_Type, EmitSamplerComparison) {
  type::Sampler sampler(type::SamplerKind::kComparisonSampler);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, &sampler, "")) << gen.error();
  EXPECT_EQ(result(), "SamplerComparisonState");
}

struct HlslDepthTextureData {
  type::TextureDimension dim;
  std::string result;
};
inline std::ostream& operator<<(std::ostream& out, HlslDepthTextureData data) {
  out << data.dim;
  return out;
}
using HlslDepthtexturesTest = TestParamHelper<HlslDepthTextureData>;
TEST_P(HlslDepthtexturesTest, Emit) {
  auto params = GetParam();

  type::DepthTexture s(params.dim);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, &s, "")) << gen.error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest_Type,
    HlslDepthtexturesTest,
    testing::Values(
        HlslDepthTextureData{type::TextureDimension::k2d, "Texture2D"},
        HlslDepthTextureData{type::TextureDimension::k2dArray,
                             "Texture2DArray"},
        HlslDepthTextureData{type::TextureDimension::kCube, "TextureCube"},
        HlslDepthTextureData{type::TextureDimension::kCubeArray,
                             "TextureCubeArray"}));

struct HlslTextureData {
  type::TextureDimension dim;
  std::string result;
};
inline std::ostream& operator<<(std::ostream& out, HlslTextureData data) {
  out << data.dim;
  return out;
}
using HlslSampledtexturesTest = TestParamHelper<HlslTextureData>;
TEST_P(HlslSampledtexturesTest, Emit) {
  auto params = GetParam();

  type::SampledTexture s(params.dim, ty.f32());

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, &s, "")) << gen.error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest_Type,
    HlslSampledtexturesTest,
    testing::Values(
        HlslTextureData{type::TextureDimension::k1d, "Texture1D"},
        HlslTextureData{type::TextureDimension::k2d, "Texture2D"},
        HlslTextureData{type::TextureDimension::k2dArray, "Texture2DArray"},
        HlslTextureData{type::TextureDimension::k3d, "Texture3D"},
        HlslTextureData{type::TextureDimension::kCube, "TextureCube"},
        HlslTextureData{type::TextureDimension::kCubeArray,
                        "TextureCubeArray"}));

TEST_F(HlslGeneratorImplTest_Type, EmitMultisampledTexture) {
  type::MultisampledTexture s(type::TextureDimension::k2d, ty.f32());

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, &s, "")) << gen.error();
  EXPECT_EQ(result(), "Texture2DMS<float4>");
}

struct HlslStorageTextureData {
  type::TextureDimension dim;
  type::ImageFormat imgfmt;
  bool ro;
  std::string result;
};
inline std::ostream& operator<<(std::ostream& out,
                                HlslStorageTextureData data) {
  out << data.dim << (data.ro ? "ReadOnly" : "WriteOnly");
  return out;
}
using HlslStoragetexturesTest = TestParamHelper<HlslStorageTextureData>;
TEST_P(HlslStoragetexturesTest, Emit) {
  auto params = GetParam();

  auto* subtype = type::StorageTexture::SubtypeFor(params.imgfmt, Types());
  auto* s = create<type::StorageTexture>(params.dim, params.imgfmt, subtype);
  auto* ac =
      create<type::AccessControl>(params.ro ? ast::AccessControl::kReadOnly
                                            : ast::AccessControl::kWriteOnly,
                                  s);

  GeneratorImpl& gen = Build();

  ASSERT_TRUE(gen.EmitType(out, ac, "")) << gen.error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest_Type,
    HlslStoragetexturesTest,
    testing::Values(HlslStorageTextureData{type::TextureDimension::k1d,
                                           type::ImageFormat::kRgba8Unorm, true,
                                           "RWTexture1D<float4>"},
                    HlslStorageTextureData{type::TextureDimension::k2d,
                                           type::ImageFormat::kRgba16Float,
                                           true, "RWTexture2D<float4>"},
                    HlslStorageTextureData{type::TextureDimension::k2dArray,
                                           type::ImageFormat::kR32Float, true,
                                           "RWTexture2DArray<float4>"},
                    HlslStorageTextureData{type::TextureDimension::k3d,
                                           type::ImageFormat::kRg32Float, true,
                                           "RWTexture3D<float4>"},
                    HlslStorageTextureData{type::TextureDimension::k1d,
                                           type::ImageFormat::kRgba32Float,
                                           false, "RWTexture1D<float4>"},
                    HlslStorageTextureData{type::TextureDimension::k2d,
                                           type::ImageFormat::kRgba16Uint,
                                           false, "RWTexture2D<uint4>"},
                    HlslStorageTextureData{type::TextureDimension::k2dArray,
                                           type::ImageFormat::kR32Uint, false,
                                           "RWTexture2DArray<uint4>"},
                    HlslStorageTextureData{type::TextureDimension::k3d,
                                           type::ImageFormat::kRg32Uint, false,
                                           "RWTexture3D<uint4>"},
                    HlslStorageTextureData{type::TextureDimension::k1d,
                                           type::ImageFormat::kRgba32Uint, true,
                                           "RWTexture1D<uint4>"},
                    HlslStorageTextureData{type::TextureDimension::k2d,
                                           type::ImageFormat::kRgba16Sint, true,
                                           "RWTexture2D<int4>"},
                    HlslStorageTextureData{type::TextureDimension::k2dArray,
                                           type::ImageFormat::kR32Sint, true,
                                           "RWTexture2DArray<int4>"},
                    HlslStorageTextureData{type::TextureDimension::k3d,
                                           type::ImageFormat::kRg32Sint, true,
                                           "RWTexture3D<int4>"},
                    HlslStorageTextureData{type::TextureDimension::k1d,
                                           type::ImageFormat::kRgba32Sint,
                                           false, "RWTexture1D<int4>"}));

}  // namespace
}  // namespace hlsl
}  // namespace writer
}  // namespace tint
