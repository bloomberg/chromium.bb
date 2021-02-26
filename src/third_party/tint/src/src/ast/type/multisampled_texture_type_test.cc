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

#include "src/ast/type/multisampled_texture_type.h"

#include "gtest/gtest.h"
#include "src/ast/type/f32_type.h"

namespace tint {
namespace ast {
namespace type {
namespace {

using MultisampledTextureTypeTest = testing::Test;

TEST_F(MultisampledTextureTypeTest, Is) {
  F32Type f32;
  MultisampledTextureType s(TextureDimension::kCube, &f32);
  EXPECT_FALSE(s.IsAccessControl());
  EXPECT_FALSE(s.IsAlias());
  EXPECT_FALSE(s.IsArray());
  EXPECT_FALSE(s.IsBool());
  EXPECT_FALSE(s.IsF32());
  EXPECT_FALSE(s.IsI32());
  EXPECT_FALSE(s.IsMatrix());
  EXPECT_FALSE(s.IsPointer());
  EXPECT_FALSE(s.IsSampler());
  EXPECT_FALSE(s.IsStruct());
  EXPECT_TRUE(s.IsTexture());
  EXPECT_FALSE(s.IsU32());
  EXPECT_FALSE(s.IsVector());
}

TEST_F(MultisampledTextureTypeTest, IsTextureType) {
  F32Type f32;
  MultisampledTextureType s(TextureDimension::kCube, &f32);
  EXPECT_FALSE(s.IsDepth());
  EXPECT_TRUE(s.IsMultisampled());
  EXPECT_FALSE(s.IsSampled());
  EXPECT_FALSE(s.IsStorage());
}

TEST_F(MultisampledTextureTypeTest, Dim) {
  F32Type f32;
  MultisampledTextureType s(TextureDimension::k3d, &f32);
  EXPECT_EQ(s.dim(), TextureDimension::k3d);
}

TEST_F(MultisampledTextureTypeTest, Type) {
  F32Type f32;
  MultisampledTextureType s(TextureDimension::k3d, &f32);
  EXPECT_EQ(s.type(), &f32);
}

TEST_F(MultisampledTextureTypeTest, TypeName) {
  F32Type f32;
  MultisampledTextureType s(TextureDimension::k3d, &f32);
  EXPECT_EQ(s.type_name(), "__multisampled_texture_3d__f32");
}

TEST_F(MultisampledTextureTypeTest, MinBufferBindingSize) {
  F32Type f32;
  MultisampledTextureType s(TextureDimension::k3d, &f32);
  EXPECT_EQ(0u, s.MinBufferBindingSize(MemoryLayout::kUniformBuffer));
}

}  // namespace
}  // namespace type
}  // namespace ast
}  // namespace tint
