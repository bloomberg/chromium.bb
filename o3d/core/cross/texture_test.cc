/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file implements unit tests for class Texture.

#include "tests/common/win/testing_common.h"
#include "core/cross/texture.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"

namespace o3d {

namespace {

bool CompareTexture(Texture2D* texture, int level, const uint8* expected) {
  Texture2D::LockHelper helper(texture, level, Texture::kReadOnly);
  const uint8* data = helper.GetDataAs<const uint8>();
  unsigned mip_width = image::ComputeMipDimension(level, texture->width());
  unsigned mip_height = image::ComputeMipDimension(level, texture->height());

  int bytes_per_row = image::ComputePitch(texture->format(), mip_width);
  for (unsigned yy = 0; yy < mip_height; ++yy) {
    if (memcmp(data, expected, bytes_per_row) != 0) {
      return false;
    }
    expected += bytes_per_row;
    data += helper.pitch();
  }
  return true;
}

}  // anonymous namespace.

class Texture2DTest : public testing::Test {
 protected:
  Texture2DTest()
      : object_manager_(g_service_locator) {
  }

  virtual void SetUp() {
    pack_ = object_manager_->CreatePack();
  }

  virtual void TearDown() {
    pack_->Destroy();
  }

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
};

TEST_F(Texture2DTest, Basic) {
  Texture2D* texture = pack()->CreateTexture2D(8, 8, Texture::ARGB8, 1, false);
  ASSERT_TRUE(texture != NULL);
  EXPECT_TRUE(texture->IsA(Texture2D::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(Texture::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(ParamObject::GetApparentClass()));
  EXPECT_EQ(texture->format(), Texture::ARGB8);
  EXPECT_EQ(texture->levels(), 1);
  EXPECT_FALSE(texture->render_surfaces_enabled());
  EXPECT_EQ(0, Texture::kMaxDimension >> Texture::kMaxLevels);
  EXPECT_EQ(1, Texture::kMaxDimension >> (Texture::kMaxLevels - 1));
}

TEST_F(Texture2DTest, SetRect) {
  const int kWidth = 8;
  const int kHeight = 8;
  const int kLevels = 2;
  const int kDestMip = 1;
  const unsigned kDestX = 1u;
  const unsigned kDestY = 1u;
  Texture2D* texture = pack()->CreateTexture2D(
      kWidth, kHeight, Texture::ARGB8, kLevels, false);
  ASSERT_TRUE(texture != NULL);
  static const uint8 kExpected1[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_TRUE(CompareTexture(texture, 1, kExpected1));
  const int kSrcWidth = 2;
  const int kSrcHeight = 2;
  static const uint8 kSourcePixels[] = {
    0x01, 0x01, 0x01, 0x02, 0x03, 0x03, 0x03, 0x04,
    0x05, 0x05, 0x05, 0x06, 0x07, 0x07, 0x07, 0x08,
  };
  const int kSourcePitch = sizeof(kSourcePixels[0]) * kSrcWidth * 4;
  // normal copy
  texture->SetRect(kDestMip, kDestX, kDestY,
                   kSrcWidth, kSrcHeight, kSourcePixels, kSourcePitch);
  static const uint8 kExpected2[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02,
    0x03, 0x03, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x05, 0x05, 0x05, 0x06,
    0x07, 0x07, 0x07, 0x08, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_TRUE(CompareTexture(texture, 1, kExpected2));
  // flipped copy
  texture->SetRect(
      kDestMip, kDestX, kDestY,
      kSrcWidth, kSrcHeight,
      kSourcePixels + kSourcePitch,
      -kSourcePitch);
  static const uint8 kExpected3[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x05, 0x05, 0x05, 0x06,
    0x07, 0x07, 0x07, 0x08, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x02,
    0x03, 0x03, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  EXPECT_TRUE(CompareTexture(texture, 1, kExpected3));
}

class TextureCUBETest : public testing::Test {
 protected:
  TextureCUBETest()
      : object_manager_(g_service_locator) {
  }

  virtual void SetUp() {
    pack_ = object_manager_->CreatePack();
  }

  virtual void TearDown() {
    pack_->Destroy();
  }

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
};

TEST_F(TextureCUBETest, Basic) {
  TextureCUBE* texture =
      pack()->CreateTextureCUBE(8, Texture::ARGB8, 1, false);
  ASSERT_TRUE(texture != NULL);
  EXPECT_TRUE(texture->IsA(TextureCUBE::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(Texture::GetApparentClass()));
  EXPECT_TRUE(texture->IsA(ParamObject::GetApparentClass()));
  EXPECT_EQ(texture->format(), Texture::ARGB8);
  EXPECT_EQ(texture->levels(), 1);
  EXPECT_FALSE(texture->render_surfaces_enabled());
}

}  // namespace o3d

