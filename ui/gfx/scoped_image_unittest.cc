// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "gfx/scoped_image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_LINUX)
#include "gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace {

class ScopedImageTest : public testing::Test {
 public:
  SkBitmap* CreateBitmap() {
    SkBitmap* bitmap = new SkBitmap();
    bitmap->setConfig(SkBitmap::kARGB_8888_Config, 25, 25);
    bitmap->allocPixels();
    bitmap->eraseRGB(255, 0, 0);
    return bitmap;
  }

  gfx::NativeImage CreateNativeImage() {
    scoped_ptr<SkBitmap> bitmap(CreateBitmap());
#if defined(OS_MACOSX)
    NSImage* image = gfx::SkBitmapToNSImage(*(bitmap.get()));
    base::mac::NSObjectRetain(image);
    return image;
#elif defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
    return gfx::GdkPixbufFromSkBitmap(bitmap.get());
#else
    return bitmap.release();
#endif
  }
};

TEST_F(ScopedImageTest, Initialize) {
  gfx::ScopedImage<SkBitmap> image(CreateBitmap());
  EXPECT_TRUE(image.Get());
}

TEST_F(ScopedImageTest, Free) {
  gfx::ScopedImage<SkBitmap> image(CreateBitmap());
  EXPECT_TRUE(image.Get());
  image.Free();
  EXPECT_FALSE(image.Get());
}

TEST_F(ScopedImageTest, Release) {
  gfx::ScopedImage<SkBitmap> image(CreateBitmap());
  EXPECT_TRUE(image.Get());
  scoped_ptr<SkBitmap> bitmap(image.Release());
  EXPECT_FALSE(image.Get());
}

TEST_F(ScopedImageTest, Set) {
  gfx::ScopedImage<SkBitmap> image(CreateBitmap());
  EXPECT_TRUE(image.Get());
  SkBitmap* image2 = CreateBitmap();
  image.Set(image2);
  EXPECT_EQ(image2, image.Get());
}

TEST_F(ScopedImageTest, NativeInitialize) {
  gfx::ScopedNativeImage image(CreateNativeImage());
  EXPECT_TRUE(image.Get());
}

TEST_F(ScopedImageTest, NativeFree) {
  gfx::ScopedNativeImage image(CreateNativeImage());
  EXPECT_TRUE(image.Get());
  image.Free();
  EXPECT_FALSE(image.Get());
}

TEST_F(ScopedImageTest, NativeRelease) {
  gfx::ScopedNativeImage image(CreateNativeImage());
  EXPECT_TRUE(image.Get());
  gfx::ScopedNativeImage image2(image.Release());
  EXPECT_FALSE(image.Get());
  EXPECT_TRUE(image2.Get());
}

TEST_F(ScopedImageTest, NativeSet) {
  gfx::ScopedNativeImage image(CreateNativeImage());
  EXPECT_TRUE(image.Get());
  gfx::NativeImage image2 = CreateNativeImage();
  image.Set(image2);
  EXPECT_EQ(image2, image.Get());
}

}  // namespace
