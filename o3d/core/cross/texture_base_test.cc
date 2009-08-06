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
#include "core/cross/texture_base.h"
#include "core/cross/object_manager.h"

namespace o3d {

namespace {

Texture::RGBASwizzleIndices swizzle;

class MockTexture : public Texture {
 public:
  typedef SmartPointer<MockTexture> Ref;

  MockTexture(ServiceLocator* service_locator,
              Texture::Format format,
              int levels,
              bool alpha_is_one,
              bool resize_to_pot,
              bool enable_render_surfaces)
      : Texture(service_locator, format, levels, alpha_is_one, resize_to_pot,
                enable_render_surfaces) {
  }

  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices() {
    return swizzle;
  }

  virtual void* GetTextureHandle() const {
    return NULL;
  }

  virtual void SetFromBitmap(const Bitmap& bitmap) {
  }

  virtual void GenerateMips(int source_level, int num_levels) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTexture);
};

}  // anonymous namespace

class TextureTest : public testing::Test {
 protected:

  TextureTest()
      : object_manager_(g_service_locator) {
  }

 private:
  ServiceDependency<ObjectManager> object_manager_;
};

TEST_F(TextureTest, Basic) {
  MockTexture::Ref texture(new MockTexture(
      g_service_locator,
      Texture::XRGB8,
      1,
      false,
      false,
      false));
  ASSERT_TRUE(texture != NULL);
  EXPECT_EQ(texture->format(), Texture::XRGB8);
  EXPECT_EQ(texture->levels(), 1);
  EXPECT_FALSE(texture->alpha_is_one());
  EXPECT_FALSE(texture->render_surfaces_enabled());
}

}  // namespace o3d

