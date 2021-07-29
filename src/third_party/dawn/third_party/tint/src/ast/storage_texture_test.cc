// Copyright 2020 The Tint Authors->
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

#include "src/ast/storage_texture.h"

#include "src/ast/test_helper.h"

namespace tint {
namespace ast {
namespace {

using AstStorageTextureTest = TestHelper;

TEST_F(AstStorageTextureTest, IsTexture) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRgba32Float, *this);
  Texture* ty =
      create<StorageTexture>(TextureDimension::k2dArray,
                             ImageFormat::kRgba32Float, subtype, Access::kRead);
  EXPECT_FALSE(ty->Is<DepthTexture>());
  EXPECT_FALSE(ty->Is<SampledTexture>());
  EXPECT_TRUE(ty->Is<StorageTexture>());
}

TEST_F(AstStorageTextureTest, Dim) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRgba32Float, *this);
  auto* s =
      create<StorageTexture>(TextureDimension::k2dArray,
                             ImageFormat::kRgba32Float, subtype, Access::kRead);
  EXPECT_EQ(s->dim(), TextureDimension::k2dArray);
}

TEST_F(AstStorageTextureTest, Format) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRgba32Float, *this);
  auto* s =
      create<StorageTexture>(TextureDimension::k2dArray,
                             ImageFormat::kRgba32Float, subtype, Access::kRead);
  EXPECT_EQ(s->image_format(), ImageFormat::kRgba32Float);
}

TEST_F(AstStorageTextureTest, TypeName) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRgba32Float, *this);
  auto* s =
      create<StorageTexture>(TextureDimension::k2dArray,
                             ImageFormat::kRgba32Float, subtype, Access::kRead);
  EXPECT_EQ(s->type_name(), "__storage_texture_2d_array_rgba32float_read");
}

TEST_F(AstStorageTextureTest, FriendlyName) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRgba32Float, *this);
  auto* s =
      create<StorageTexture>(TextureDimension::k2dArray,
                             ImageFormat::kRgba32Float, subtype, Access::kRead);
  EXPECT_EQ(s->FriendlyName(Symbols()),
            "texture_storage_2d_array<rgba32float, read>");
}

TEST_F(AstStorageTextureTest, F32) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRgba32Float, *this);
  Type* s =
      create<StorageTexture>(TextureDimension::k2dArray,
                             ImageFormat::kRgba32Float, subtype, Access::kRead);

  ASSERT_TRUE(s->Is<Texture>());
  ASSERT_TRUE(s->Is<StorageTexture>());
  EXPECT_TRUE(s->As<StorageTexture>()->type()->Is<F32>());
}

TEST_F(AstStorageTextureTest, U32) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRg32Uint, *this);
  Type* s =
      create<StorageTexture>(TextureDimension::k2dArray, ImageFormat::kRg32Uint,
                             subtype, Access::kRead);

  ASSERT_TRUE(s->Is<Texture>());
  ASSERT_TRUE(s->Is<StorageTexture>());
  EXPECT_TRUE(s->As<StorageTexture>()->type()->Is<U32>());
}

TEST_F(AstStorageTextureTest, I32) {
  auto* subtype = StorageTexture::SubtypeFor(ImageFormat::kRgba32Sint, *this);
  Type* s =
      create<StorageTexture>(TextureDimension::k2dArray,
                             ImageFormat::kRgba32Sint, subtype, Access::kRead);

  ASSERT_TRUE(s->Is<Texture>());
  ASSERT_TRUE(s->Is<StorageTexture>());
  EXPECT_TRUE(s->As<StorageTexture>()->type()->Is<I32>());
}

}  // namespace
}  // namespace ast
}  // namespace tint
