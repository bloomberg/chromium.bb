// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#include "ui/gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace {

#if defined(TOOLKIT_VIEWS)
const bool kUsesSkiaNatively = true;
#else
const bool kUsesSkiaNatively = false;
#endif

class ImageTest : public testing::Test {
};

namespace gt = gfx::test;

TEST_F(ImageTest, EmptyImage) {
  // Test the default constructor.
  gfx::Image image;
  EXPECT_EQ(0U, image.RepresentationCount());
  EXPECT_TRUE(image.IsEmpty());

  // Test the copy constructor.
  gfx::Image imageCopy(image);
  EXPECT_TRUE(imageCopy.IsEmpty());

  // Test calling SwapRepresentations() with an empty image.
  gfx::Image image2(gt::CreateBitmap(25, 25));
  EXPECT_FALSE(image2.IsEmpty());

  image.SwapRepresentations(&image2);
  EXPECT_FALSE(image.IsEmpty());
  EXPECT_TRUE(image2.IsEmpty());
}

TEST_F(ImageTest, SkiaToSkia) {
  gfx::Image image(gt::CreateBitmap(25, 25));
  const SkBitmap* bitmap = image.ToSkBitmap();
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(1U, image.RepresentationCount());

  // Make sure double conversion doesn't happen.
  bitmap = image.ToSkBitmap();
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(1U, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
}

TEST_F(ImageTest, SkiaRefToSkia) {
  gfx::Image image(gt::CreateBitmap(25, 25));
  const SkBitmap* bitmap = image.ToSkBitmap();
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(1U, image.RepresentationCount());

  EXPECT_EQ(bitmap, image.ToSkBitmap());
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
}

TEST_F(ImageTest, SkiaToSkiaRef) {
  gfx::Image image(gt::CreateBitmap(25, 25));

  const SkBitmap* bitmap = image.ToSkBitmap();
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(1U, image.RepresentationCount());

  const SkBitmap* bitmap1 = image.ToSkBitmap();
  EXPECT_FALSE(bitmap1->isNull());
  EXPECT_EQ(1U, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
}

TEST_F(ImageTest, SkiaToPlatform) {
  gfx::Image image(gt::CreateBitmap(25, 25));
  const size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gt::GetPlatformRepresentationType()));

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  const SkBitmap* bitmap = image.ToSkBitmap();
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
  EXPECT_TRUE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
}

TEST_F(ImageTest, PlatformToSkia) {
  gfx::Image image(gt::CreatePlatformImage());
  const size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  EXPECT_TRUE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gfx::Image::kImageRepSkia));

  const SkBitmap* bitmap = image.ToSkBitmap();
  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(kRepCount, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepSkia));
}

TEST_F(ImageTest, PlatformToPlatform) {
  gfx::Image image(gt::CreatePlatformImage());
  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(1U, image.RepresentationCount());

  // Make sure double conversion doesn't happen.
  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
  EXPECT_EQ(1U, image.RepresentationCount());

  EXPECT_TRUE(image.HasRepresentation(gt::GetPlatformRepresentationType()));
  if (!kUsesSkiaNatively)
    EXPECT_FALSE(image.HasRepresentation(gfx::Image::kImageRepSkia));
}

TEST_F(ImageTest, PlatformToSkiaToCopy) {
  const SkBitmap* bitmap;

  {
    gfx::Image image(gt::CreatePlatformImage());
    bitmap = image.CopySkBitmap();
  }

  EXPECT_TRUE(bitmap);
  EXPECT_FALSE(bitmap->isNull());

  delete bitmap;
}

#if defined(TOOLKIT_GTK)
TEST_F(ImageTest, SkiaToGdkCopy) {
  GdkPixbuf* pixbuf;

  {
    gfx::Image image(gt::CreateBitmap(25, 25));
    pixbuf = image.CopyGdkPixbuf();
  }

  EXPECT_TRUE(pixbuf);
  g_object_unref(pixbuf);
}

TEST_F(ImageTest, SkiaToCairoCreatesGdk) {
  gfx::Image image(gt::CreateBitmap(25, 25));
  EXPECT_FALSE(image.HasRepresentation(gfx::Image::kImageRepGdk));
  EXPECT_TRUE(image.ToCairo());
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepGdk));
}
#endif

#if defined(OS_MACOSX)
TEST_F(ImageTest, SkiaToCocoaCopy) {
  NSImage* ns_image;

  {
    gfx::Image image(gt::CreateBitmap(25, 25));
    ns_image = image.CopyNSImage();
  }

  EXPECT_TRUE(ns_image);
  base::mac::NSObjectRelease(ns_image);
}
#endif

TEST_F(ImageTest, CheckSkiaColor) {
  gfx::Image image(gt::CreatePlatformImage());
  const SkBitmap* bitmap = image.ToSkBitmap();

  SkAutoLockPixels auto_lock(*bitmap);
  uint32_t* pixel = bitmap->getAddr32(10, 10);
  EXPECT_EQ(SK_ColorRED, *pixel);
}

TEST_F(ImageTest, SwapRepresentations) {
  const size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  gfx::Image image1(gt::CreateBitmap(25, 25));
  const SkBitmap* bitmap1 = image1.ToSkBitmap();
  EXPECT_EQ(1U, image1.RepresentationCount());

  gfx::Image image2(gt::CreatePlatformImage());
  const SkBitmap* bitmap2 = image2.ToSkBitmap();
  gt::PlatformImage platform_image = gt::ToPlatformType(image2);
  EXPECT_EQ(kRepCount, image2.RepresentationCount());

  image1.SwapRepresentations(&image2);

  EXPECT_EQ(bitmap2, image1.ToSkBitmap());
  EXPECT_TRUE(gt::PlatformImagesEqual(platform_image,
                                      gt::ToPlatformType(image1)));
  EXPECT_EQ(bitmap1, image2.ToSkBitmap());
  EXPECT_EQ(kRepCount, image1.RepresentationCount());
  EXPECT_EQ(1U, image2.RepresentationCount());
}

TEST_F(ImageTest, Copy) {
  const size_t kRepCount = kUsesSkiaNatively ? 1U : 2U;

  gfx::Image image1(gt::CreateBitmap(25, 25));
  gfx::Image image2(image1);

  EXPECT_EQ(1U, image1.RepresentationCount());
  EXPECT_EQ(1U, image2.RepresentationCount());
  EXPECT_EQ(image1.ToSkBitmap(), image2.ToSkBitmap());

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image2)));
  EXPECT_EQ(kRepCount, image2.RepresentationCount());
  EXPECT_EQ(kRepCount, image1.RepresentationCount());
}

TEST_F(ImageTest, Assign) {
  gfx::Image image1(gt::CreatePlatformImage());
  gfx::Image image2 = image1;

  EXPECT_EQ(1U, image1.RepresentationCount());
  EXPECT_EQ(1U, image2.RepresentationCount());
  EXPECT_EQ(image1.ToSkBitmap(), image2.ToSkBitmap());
}

TEST_F(ImageTest, MultiResolutionImage) {
  const int width1x = 10;
  const int height1x = 12;
  const int width2x = 20;
  const int height2x = 24;

  gfx::ImageSkia image_skia;
  image_skia.AddBitmapForScale(gt::CreateBitmap(width1x, height1x), 1.0f);
  image_skia.AddBitmapForScale(gt::CreateBitmap(width2x, height2x), 2.0f);

  EXPECT_EQ(2u, image_skia.bitmaps().size());

  float scale_factor;
  const SkBitmap& bitmap1x = image_skia.GetBitmapForScale(1.0f, 1.0f,
                                                          &scale_factor);
  EXPECT_TRUE(!bitmap1x.isNull());
  EXPECT_EQ(1.0f, scale_factor);
  EXPECT_EQ(width1x, bitmap1x.width());
  EXPECT_EQ(height1x, bitmap1x.height());

  const SkBitmap& bitmap2x = image_skia.GetBitmapForScale(2.0f, 2.0f,
                                                          &scale_factor);
  EXPECT_TRUE(!bitmap2x.isNull());
  EXPECT_EQ(2.0f, scale_factor);
  EXPECT_EQ(width2x, bitmap2x.width());
  EXPECT_EQ(height2x, bitmap2x.height());

  // Check that the image has a single representation.
  gfx::Image image(image_skia);
  EXPECT_EQ(1u, image.RepresentationCount());
}

// Tests that gfx::Image does indeed take ownership of the SkBitmap it is
// passed.
TEST_F(ImageTest, OwnershipTest) {
  gfx::Image image;
  {
    SkBitmap bitmap = gt::CreateBitmap(10, 10);
    EXPECT_TRUE(!bitmap.isNull());
    image = gfx::Image(bitmap);
  }
  EXPECT_TRUE(!image.ToSkBitmap()->isNull());
}

// Integration tests with UI toolkit frameworks require linking against the
// Views library and cannot be here (gfx_unittests doesn't include it). They
// instead live in /chrome/browser/ui/tests/ui_gfx_image_unittest.cc.

}  // namespace
