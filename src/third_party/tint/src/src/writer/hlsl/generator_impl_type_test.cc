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

#include "src/ast/module.h"
#include "src/ast/struct.h"
#include "src/ast/struct_block_decoration.h"
#include "src/ast/struct_decoration.h"
#include "src/ast/struct_member.h"
#include "src/ast/struct_member_decoration.h"
#include "src/ast/struct_member_offset_decoration.h"
#include "src/ast/type/array_type.h"
#include "src/ast/type/bool_type.h"
#include "src/ast/type/depth_texture_type.h"
#include "src/ast/type/f32_type.h"
#include "src/ast/type/i32_type.h"
#include "src/ast/type/matrix_type.h"
#include "src/ast/type/multisampled_texture_type.h"
#include "src/ast/type/pointer_type.h"
#include "src/ast/type/sampled_texture_type.h"
#include "src/ast/type/sampler_type.h"
#include "src/ast/type/storage_texture_type.h"
#include "src/ast/type/struct_type.h"
#include "src/ast/type/u32_type.h"
#include "src/ast/type/vector_type.h"
#include "src/ast/type/void_type.h"
#include "src/writer/hlsl/test_helper.h"

namespace tint {
namespace writer {
namespace hlsl {
namespace {

using HlslGeneratorImplTest_Type = TestHelper;

TEST_F(HlslGeneratorImplTest_Type, EmitType_Alias) {
  ast::type::F32Type f32;
  ast::type::AliasType alias("alias", &f32);

  ASSERT_TRUE(gen().EmitType(out(), &alias, "")) << gen().error();
  EXPECT_EQ(result(), "alias");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Alias_NameCollision) {
  ast::type::F32Type f32;
  ast::type::AliasType alias("bool", &f32);

  ASSERT_TRUE(gen().EmitType(out(), &alias, "")) << gen().error();
  EXPECT_EQ(result(), "bool_tint_0");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Array) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b, 4);

  ASSERT_TRUE(gen().EmitType(out(), &a, "ary")) << gen().error();
  EXPECT_EQ(result(), "bool ary[4]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_ArrayOfArray) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b, 4);
  ast::type::ArrayType c(&a, 5);

  ASSERT_TRUE(gen().EmitType(out(), &c, "ary")) << gen().error();
  EXPECT_EQ(result(), "bool ary[5][4]");
}

// TODO(dsinclair): Is this possible? What order should it output in?
TEST_F(HlslGeneratorImplTest_Type,
       DISABLED_EmitType_ArrayOfArrayOfRuntimeArray) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b, 4);
  ast::type::ArrayType c(&a, 5);
  ast::type::ArrayType d(&c);

  ASSERT_TRUE(gen().EmitType(out(), &c, "ary")) << gen().error();
  EXPECT_EQ(result(), "bool ary[5][4][1]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_ArrayOfArrayOfArray) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b, 4);
  ast::type::ArrayType c(&a, 5);
  ast::type::ArrayType d(&c, 6);

  ASSERT_TRUE(gen().EmitType(out(), &d, "ary")) << gen().error();
  EXPECT_EQ(result(), "bool ary[6][5][4]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Array_NameCollision) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b, 4);

  ASSERT_TRUE(gen().EmitType(out(), &a, "bool")) << gen().error();
  EXPECT_EQ(result(), "bool bool_tint_0[4]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Array_WithoutName) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b, 4);

  ASSERT_TRUE(gen().EmitType(out(), &a, "")) << gen().error();
  EXPECT_EQ(result(), "bool[4]");
}

TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_RuntimeArray) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b);

  ASSERT_TRUE(gen().EmitType(out(), &a, "ary")) << gen().error();
  EXPECT_EQ(result(), "bool ary[]");
}

TEST_F(HlslGeneratorImplTest_Type,
       DISABLED_EmitType_RuntimeArray_NameCollision) {
  ast::type::BoolType b;
  ast::type::ArrayType a(&b);

  ASSERT_TRUE(gen().EmitType(out(), &a, "double")) << gen().error();
  EXPECT_EQ(result(), "bool double_tint_0[]");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Bool) {
  ast::type::BoolType b;

  ASSERT_TRUE(gen().EmitType(out(), &b, "")) << gen().error();
  EXPECT_EQ(result(), "bool");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_F32) {
  ast::type::F32Type f32;

  ASSERT_TRUE(gen().EmitType(out(), &f32, "")) << gen().error();
  EXPECT_EQ(result(), "float");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_I32) {
  ast::type::I32Type i32;

  ASSERT_TRUE(gen().EmitType(out(), &i32, "")) << gen().error();
  EXPECT_EQ(result(), "int");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Matrix) {
  ast::type::F32Type f32;
  ast::type::MatrixType m(&f32, 3, 2);

  ASSERT_TRUE(gen().EmitType(out(), &m, "")) << gen().error();
  EXPECT_EQ(result(), "matrix<float, 3, 2>");
}

// TODO(dsinclair): How to annotate as workgroup?
TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_Pointer) {
  ast::type::F32Type f32;
  ast::type::PointerType p(&f32, ast::StorageClass::kWorkgroup);

  ASSERT_TRUE(gen().EmitType(out(), &p, "")) << gen().error();
  EXPECT_EQ(result(), "float*");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_StructDecl) {
  ast::type::I32Type i32;
  ast::type::F32Type f32;

  ast::StructMemberList members;
  members.push_back(std::make_unique<ast::StructMember>(
      "a", &i32, ast::StructMemberDecorationList{}));

  ast::StructMemberDecorationList b_deco;
  b_deco.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(4, Source{}));
  members.push_back(
      std::make_unique<ast::StructMember>("b", &f32, std::move(b_deco)));

  auto str = std::make_unique<ast::Struct>();
  str->set_members(std::move(members));

  ast::type::StructType s("S", std::move(str));

  ASSERT_TRUE(gen().EmitStructType(out(), &s, "S")) << gen().error();
  EXPECT_EQ(result(), R"(struct S {
  int a;
  float b;
};
)");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Struct) {
  ast::type::I32Type i32;
  ast::type::F32Type f32;

  ast::StructMemberList members;
  members.push_back(std::make_unique<ast::StructMember>(
      "a", &i32, ast::StructMemberDecorationList{}));

  ast::StructMemberDecorationList b_deco;
  b_deco.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(4, Source{}));
  members.push_back(
      std::make_unique<ast::StructMember>("b", &f32, std::move(b_deco)));

  auto str = std::make_unique<ast::Struct>();
  str->set_members(std::move(members));

  ast::type::StructType s("S", std::move(str));

  ASSERT_TRUE(gen().EmitType(out(), &s, "")) << gen().error();
  EXPECT_EQ(result(), "S");
}

TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_Struct_InjectPadding) {
  ast::type::I32Type i32;
  ast::type::F32Type f32;

  ast::StructMemberDecorationList decos;
  decos.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(4, Source{}));

  ast::StructMemberList members;
  members.push_back(
      std::make_unique<ast::StructMember>("a", &i32, std::move(decos)));

  decos.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(32, Source{}));
  members.push_back(
      std::make_unique<ast::StructMember>("b", &f32, std::move(decos)));

  decos.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(128, Source{}));
  members.push_back(
      std::make_unique<ast::StructMember>("c", &f32, std::move(decos)));

  auto str = std::make_unique<ast::Struct>();
  str->set_members(std::move(members));

  ast::type::StructType s("S", std::move(str));

  ASSERT_TRUE(gen().EmitType(out(), &s, "")) << gen().error();
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
  ast::type::I32Type i32;
  ast::type::F32Type f32;

  ast::StructMemberList members;
  members.push_back(std::make_unique<ast::StructMember>(
      "double", &i32, ast::StructMemberDecorationList{}));

  ast::StructMemberDecorationList b_deco;
  members.push_back(
      std::make_unique<ast::StructMember>("float", &f32, std::move(b_deco)));

  auto str = std::make_unique<ast::Struct>();
  str->set_members(std::move(members));

  ast::type::StructType s("S", std::move(str));

  ASSERT_TRUE(gen().EmitStructType(out(), &s, "S")) << gen().error();
  EXPECT_EQ(result(), R"(struct S {
  int double_tint_0;
  float float_tint_0;
};
)");
}

// TODO(dsinclair): How to translate [[block]]
TEST_F(HlslGeneratorImplTest_Type, DISABLED_EmitType_Struct_WithDecoration) {
  ast::type::I32Type i32;
  ast::type::F32Type f32;

  ast::StructMemberList members;
  members.push_back(std::make_unique<ast::StructMember>(
      "a", &i32, ast::StructMemberDecorationList{}));

  ast::StructMemberDecorationList b_deco;
  b_deco.push_back(
      std::make_unique<ast::StructMemberOffsetDecoration>(4, Source{}));
  members.push_back(
      std::make_unique<ast::StructMember>("b", &f32, std::move(b_deco)));

  ast::StructDecorationList decos;
  decos.push_back(std::make_unique<ast::StructBlockDecoration>(Source{}));

  auto str =
      std::make_unique<ast::Struct>(std::move(decos), std::move(members));

  ast::type::StructType s("S", std::move(str));

  ASSERT_TRUE(gen().EmitStructType(out(), &s, "B")) << gen().error();
  EXPECT_EQ(result(), R"(struct B {
  int a;
  float b;
})");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_U32) {
  ast::type::U32Type u32;

  ASSERT_TRUE(gen().EmitType(out(), &u32, "")) << gen().error();
  EXPECT_EQ(result(), "uint");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Vector) {
  ast::type::F32Type f32;
  ast::type::VectorType v(&f32, 3);

  ASSERT_TRUE(gen().EmitType(out(), &v, "")) << gen().error();
  EXPECT_EQ(result(), "vector<float, 3>");
}

TEST_F(HlslGeneratorImplTest_Type, EmitType_Void) {
  ast::type::VoidType v;

  ASSERT_TRUE(gen().EmitType(out(), &v, "")) << gen().error();
  EXPECT_EQ(result(), "void");
}

TEST_F(HlslGeneratorImplTest_Type, EmitSampler) {
  ast::type::SamplerType sampler(ast::type::SamplerKind::kSampler);

  ASSERT_TRUE(gen().EmitType(out(), &sampler, "")) << gen().error();
  EXPECT_EQ(result(), "SamplerState");
}

TEST_F(HlslGeneratorImplTest_Type, EmitSamplerComparison) {
  ast::type::SamplerType sampler(ast::type::SamplerKind::kComparisonSampler);

  ASSERT_TRUE(gen().EmitType(out(), &sampler, "")) << gen().error();
  EXPECT_EQ(result(), "SamplerComparisonState");
}

struct HlslDepthTextureData {
  ast::type::TextureDimension dim;
  std::string result;
};
inline std::ostream& operator<<(std::ostream& out, HlslDepthTextureData data) {
  out << data.dim;
  return out;
}
using HlslDepthtexturesTest =
    TestHelperBase<testing::TestWithParam<HlslDepthTextureData>>;
TEST_P(HlslDepthtexturesTest, Emit) {
  auto params = GetParam();

  ast::type::DepthTextureType s(params.dim);

  ASSERT_TRUE(gen().EmitType(out(), &s, "")) << gen().error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest_Type,
    HlslDepthtexturesTest,
    testing::Values(
        HlslDepthTextureData{ast::type::TextureDimension::k2d, "Texture2D"},
        HlslDepthTextureData{ast::type::TextureDimension::k2dArray,
                             "Texture2DArray"},
        HlslDepthTextureData{ast::type::TextureDimension::kCube, "TextureCube"},
        HlslDepthTextureData{ast::type::TextureDimension::kCubeArray,
                             "TextureCubeArray"}));

struct HlslTextureData {
  ast::type::TextureDimension dim;
  std::string result;
};
inline std::ostream& operator<<(std::ostream& out, HlslTextureData data) {
  out << data.dim;
  return out;
}
using HlslSampledtexturesTest =
    TestHelperBase<testing::TestWithParam<HlslTextureData>>;
TEST_P(HlslSampledtexturesTest, Emit) {
  auto params = GetParam();

  ast::type::F32Type f32;
  ast::type::SampledTextureType s(params.dim, &f32);

  ASSERT_TRUE(gen().EmitType(out(), &s, "")) << gen().error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest_Type,
    HlslSampledtexturesTest,
    testing::Values(
        HlslTextureData{ast::type::TextureDimension::k1d, "Texture1D"},
        HlslTextureData{ast::type::TextureDimension::k1dArray,
                        "Texture1DArray"},
        HlslTextureData{ast::type::TextureDimension::k2d, "Texture2D"},
        HlslTextureData{ast::type::TextureDimension::k2dArray,
                        "Texture2DArray"},
        HlslTextureData{ast::type::TextureDimension::k3d, "Texture3D"},
        HlslTextureData{ast::type::TextureDimension::kCube, "TextureCube"},
        HlslTextureData{ast::type::TextureDimension::kCubeArray,
                        "TextureCubeArray"}));

TEST_F(HlslGeneratorImplTest_Type, EmitMultisampledTexture) {
  ast::type::F32Type f32;
  ast::type::MultisampledTextureType s(ast::type::TextureDimension::k2d, &f32);

  ASSERT_TRUE(gen().EmitType(out(), &s, "")) << gen().error();
  EXPECT_EQ(result(), "Texture2D");
}

struct HlslStorageTextureData {
  ast::type::TextureDimension dim;
  bool ro;
  std::string result;
};
inline std::ostream& operator<<(std::ostream& out,
                                HlslStorageTextureData data) {
  out << data.dim << (data.ro ? "ReadOnly" : "WriteOnly");
  return out;
}
using HlslStoragetexturesTest =
    TestHelperBase<testing::TestWithParam<HlslStorageTextureData>>;
TEST_P(HlslStoragetexturesTest, Emit) {
  auto params = GetParam();

  ast::type::StorageTextureType s(params.dim,
                                  params.ro ? ast::AccessControl::kReadOnly
                                            : ast::AccessControl::kWriteOnly,
                                  ast::type::ImageFormat::kR16Float);

  ASSERT_TRUE(gen().EmitType(out(), &s, "")) << gen().error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest_Type,
    HlslStoragetexturesTest,
    testing::Values(
        HlslStorageTextureData{ast::type::TextureDimension::k1d, true,
                               "RWTexture1D"},
        HlslStorageTextureData{ast::type::TextureDimension::k1dArray, true,
                               "RWTexture1DArray"},
        HlslStorageTextureData{ast::type::TextureDimension::k2d, true,
                               "RWTexture2D"},
        HlslStorageTextureData{ast::type::TextureDimension::k2dArray, true,
                               "RWTexture2DArray"},
        HlslStorageTextureData{ast::type::TextureDimension::k3d, true,
                               "RWTexture3D"},
        HlslStorageTextureData{ast::type::TextureDimension::k1d, false,
                               "RWTexture1D"},
        HlslStorageTextureData{ast::type::TextureDimension::k1dArray, false,
                               "RWTexture1DArray"},
        HlslStorageTextureData{ast::type::TextureDimension::k2d, false,
                               "RWTexture2D"},
        HlslStorageTextureData{ast::type::TextureDimension::k2dArray, false,
                               "RWTexture2DArray"},
        HlslStorageTextureData{ast::type::TextureDimension::k3d, false,
                               "RWTexture3D"}));

}  // namespace
}  // namespace hlsl
}  // namespace writer
}  // namespace tint
