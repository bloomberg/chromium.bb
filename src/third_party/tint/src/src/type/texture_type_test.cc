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

#include "src/type/texture_type.h"

#include "src/type/test_helper.h"

namespace tint {
namespace type {
namespace {

using TextureTypeTest = TestHelper;

TEST_F(TextureTypeTest, IsTextureArray) {
  EXPECT_EQ(false, IsTextureArray(TextureDimension::kNone));
  EXPECT_EQ(false, IsTextureArray(TextureDimension::k1d));
  EXPECT_EQ(false, IsTextureArray(TextureDimension::k2d));
  EXPECT_EQ(true, IsTextureArray(TextureDimension::k2dArray));
  EXPECT_EQ(false, IsTextureArray(TextureDimension::k3d));
  EXPECT_EQ(false, IsTextureArray(TextureDimension::kCube));
  EXPECT_EQ(true, IsTextureArray(TextureDimension::kCubeArray));
}

TEST_F(TextureTypeTest, NumCoordinateAxes) {
  EXPECT_EQ(0, NumCoordinateAxes(TextureDimension::kNone));
  EXPECT_EQ(1, NumCoordinateAxes(TextureDimension::k1d));
  EXPECT_EQ(2, NumCoordinateAxes(TextureDimension::k2d));
  EXPECT_EQ(2, NumCoordinateAxes(TextureDimension::k2dArray));
  EXPECT_EQ(3, NumCoordinateAxes(TextureDimension::k3d));
  EXPECT_EQ(3, NumCoordinateAxes(TextureDimension::kCube));
  EXPECT_EQ(3, NumCoordinateAxes(TextureDimension::kCubeArray));
}

}  // namespace
}  // namespace type
}  // namespace tint
