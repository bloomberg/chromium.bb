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

#include "src/tint/ast/sampled_texture.h"

#include "src/tint/ast/f32.h"
#include "src/tint/ast/test_helper.h"

namespace tint::ast {
namespace {

using AstSampledTextureTest = TestHelper;

TEST_F(AstSampledTextureTest, IsTexture) {
    auto* f32 = create<F32>();
    Texture* ty = create<SampledTexture>(TextureDimension::kCube, f32);
    EXPECT_FALSE(ty->Is<DepthTexture>());
    EXPECT_TRUE(ty->Is<SampledTexture>());
    EXPECT_FALSE(ty->Is<StorageTexture>());
}

TEST_F(AstSampledTextureTest, Dim) {
    auto* f32 = create<F32>();
    auto* s = create<SampledTexture>(TextureDimension::k3d, f32);
    EXPECT_EQ(s->dim, TextureDimension::k3d);
}

TEST_F(AstSampledTextureTest, Type) {
    auto* f32 = create<F32>();
    auto* s = create<SampledTexture>(TextureDimension::k3d, f32);
    EXPECT_EQ(s->type, f32);
}

TEST_F(AstSampledTextureTest, FriendlyName) {
    auto* f32 = create<F32>();
    auto* s = create<SampledTexture>(TextureDimension::k3d, f32);
    EXPECT_EQ(s->FriendlyName(Symbols()), "texture_3d<f32>");
}

}  // namespace
}  // namespace tint::ast
