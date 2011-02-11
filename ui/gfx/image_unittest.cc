// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image.h"

#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image_unittest.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#include "gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace {

using namespace ui::gfx::test;

#if defined(TOOLKIT_VIEWS)
const bool kUsesSkiaNatively = true;
#else
const bool kUsesSkiaNatively = false;
#endif

class ImageTest : public testing::Test {
 public:
  size_t GetRepCount(const ui::gfx::Image& image) {
    return image.representations_.size();
  }
};

TEST_F(ImageTest, SkiaToSkia) {
  ui::gfx::Image image(CreateBitmap());
  const SkBitmap* bitmap = static_cast<const SkBitmap*>(image);
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(1U, GetRepCount(image));

  // Make sure double conversion doesn't happen.
  bitmap = static_cast<const SkBitmap*>(image);
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(1U, GetRepCount(image));
}

TEST_F(ImageTest, SkiaToSkiaRef) {
  ui::gfx::Image image(CreateBitmap());

  const SkBitmap& bitmap = static_cast<const SkBitmap&>(image);
  EXPECT_FALSE(bitmap.isNull());
  EXPECT_EQ(1U, GetRepCount(image));

  const SkBitmap* bitmap1 = static_cast<const SkBitmap*>(image);
  EXPECT_FALSE(bitmap1->isNull());
  EXPECT_EQ(1U, GetRepCount(image));
}

TEST_F(ImageTest, SkiaToPlatform) {
  ui::gfx::Image image(CreateBitmap());
  const size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  EXPECT_TRUE(static_cast<PlatformImage>(image));
  EXPECT_EQ(kRepCount, GetRepCount(image));

  const SkBitmap& bitmap = static_cast<const SkBitmap&>(image);
  EXPECT_FALSE(bitmap.isNull());
  EXPECT_EQ(kRepCount, GetRepCount(image));
}

TEST_F(ImageTest, PlatformToSkia) {
  ui::gfx::Image image(CreatePlatformImage());
  const size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  const SkBitmap* bitmap = static_cast<const SkBitmap*>(image);
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(kRepCount, GetRepCount(image));

  EXPECT_TRUE(static_cast<PlatformImage>(image));
  EXPECT_EQ(kRepCount, GetRepCount(image));
}

TEST_F(ImageTest, PlatformToPlatform) {
  ui::gfx::Image image(CreatePlatformImage());
  EXPECT_TRUE(static_cast<PlatformImage>(image));
  EXPECT_EQ(1U, GetRepCount(image));

  // Make sure double conversion doesn't happen.
  EXPECT_TRUE(static_cast<PlatformImage>(image));
  EXPECT_EQ(1U, GetRepCount(image));
}

TEST_F(ImageTest, CheckSkiaColor) {
  ui::gfx::Image image(CreatePlatformImage());
  const SkBitmap& bitmap(image);

  SkAutoLockPixels auto_lock(bitmap);
  uint32_t* pixel = bitmap.getAddr32(10, 10);
  EXPECT_EQ(SK_ColorRED, *pixel);
}

// Integration tests with UI toolkit frameworks require linking against the
// Views library and cannot be here (gfx_unittests doesn't include it). They
// instead live in /chrome/browser/ui/tests/ui_gfx_image_unittest.cc.

}  // namespace
