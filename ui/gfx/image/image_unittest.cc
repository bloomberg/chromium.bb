// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#include "ui/gfx/gtk_util.h"
#elif defined(OS_IOS)
#include "base/mac/foundation_util.h"
#include "skia/ext/skia_utils_ios.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace {

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)
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

// Test constructing a gfx::Image from an empty PlatformImage.
TEST_F(ImageTest, EmptyImageFromEmptyPlatformImage) {
#if defined(OS_MACOSX) || defined(TOOLKIT_GTK)
  gfx::Image image1(NULL);
  EXPECT_TRUE(image1.IsEmpty());
  EXPECT_EQ(0U, image1.RepresentationCount());
#endif

  // SkBitmap and gfx::ImageSkia are available on all platforms.
  SkBitmap bitmap;
  EXPECT_TRUE(bitmap.empty());
  gfx::Image image2(bitmap);
  EXPECT_TRUE(image2.IsEmpty());
  EXPECT_EQ(0U, image2.RepresentationCount());

  gfx::ImageSkia image_skia;
  EXPECT_TRUE(image_skia.isNull());
  gfx::Image image3(image_skia);
  EXPECT_TRUE(image3.IsEmpty());
  EXPECT_EQ(0U, image3.RepresentationCount());
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

TEST_F(ImageTest, SkiaToPNGEncodeAndDecode) {
  gfx::Image image(gt::CreateBitmap(25, 25));
  const std::vector<unsigned char>* png = image.ToImagePNG();
  EXPECT_TRUE(png);
  EXPECT_FALSE(png->empty());
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepPNG));

  gfx::Image from_png(&png->front(), png->size());
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepPNG));
  EXPECT_TRUE(gt::IsEqual(from_png, image));
}

TEST_F(ImageTest, PlatformToPNGEncodeAndDecode) {
  gfx::Image image(gt::CreatePlatformImage());
  const std::vector<unsigned char>* png = image.ToImagePNG();
  EXPECT_TRUE(png);
  EXPECT_FALSE(png->empty());
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepPNG));

  gfx::Image from_png(&png->front(), png->size());
  EXPECT_TRUE(image.HasRepresentation(gfx::Image::kImageRepPNG));
  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(image)));
}

// The platform types use the platform provided encoding/decoding of PNGs. Make
// sure these work with the Skia Encode/Decode.
TEST_F(ImageTest, PNGEncodeFromSkiaDecodeToPlatform) {
  // Force the conversion sequence skia to png to platform_type.
  gfx::Image from_skia(gt::CreateBitmap(25, 25));
  const std::vector<unsigned char>* png = from_skia.ToImagePNG();
  gfx::Image from_png(&png->front(), png->size());
  gfx::Image from_platform(gt::CopyPlatformType(from_png));

  EXPECT_TRUE(gt::IsPlatformImageValid(gt::ToPlatformType(from_platform)));
  EXPECT_TRUE(gt::IsEqual(from_skia, from_platform));
}

TEST_F(ImageTest, PNGEncodeFromPlatformDecodeToSkia) {
  // Force the conversion sequence platform_type to png to skia.
  gfx::Image from_platform(gt::CreatePlatformImage());
  const std::vector<unsigned char>* png = from_platform.ToImagePNG();
  gfx::Image from_png(&png->front(), png->size());
  gfx::Image from_skia(*from_png.ToImageSkia());

  EXPECT_TRUE(gt::IsEqual(from_skia, from_platform));
}

TEST_F(ImageTest, PNGDecodeToSkiaFailure) {
  std::vector<unsigned char> png(100, 0);
  gfx::Image image(&png.front(), png.size());
  const SkBitmap* bitmap = image.ToSkBitmap();

  SkAutoLockPixels auto_lock(*bitmap);
  gt::CheckColor(bitmap->getColor(10, 10), true);
}

// TODO(rohitrao): This test needs an iOS implementation of
// GetPlatformImageColor().
#if !defined(OS_IOS)
TEST_F(ImageTest, PNGDecodeToPlatformFailure) {
  std::vector<unsigned char> png(100, 0);
  gfx::Image image(&png.front(), png.size());
  gt::CheckColor(gt::GetPlatformImageColor(gt::ToPlatformType(image)), true);
}
#endif

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

#if defined(OS_IOS)
TEST_F(ImageTest, SkiaToCocoaTouchCopy) {
  UIImage* ui_image;

  {
    gfx::Image image(gt::CreateBitmap(25, 25));
    ui_image = image.CopyUIImage();
  }

  EXPECT_TRUE(ui_image);
  base::mac::NSObjectRelease(ui_image);
}
#elif defined(OS_MACOSX)
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
  gt::CheckColor(bitmap->getColor(10, 10), false);
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
  const int kWidth1x = 10;
  const int kHeight1x = 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth1x, kHeight1x),
      ui::SCALE_FACTOR_100P));
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth2x, kHeight2x),
      ui::SCALE_FACTOR_200P));

  EXPECT_EQ(2u, image_skia.image_reps().size());

  const gfx::ImageSkiaRep& image_rep1x =
      image_skia.GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(!image_rep1x.is_null());
  EXPECT_EQ(ui::SCALE_FACTOR_100P, image_rep1x.scale_factor());
  EXPECT_EQ(kWidth1x, image_rep1x.GetWidth());
  EXPECT_EQ(kHeight1x, image_rep1x.GetHeight());
  EXPECT_EQ(kWidth1x, image_rep1x.pixel_width());
  EXPECT_EQ(kHeight1x, image_rep1x.pixel_height());

  const gfx::ImageSkiaRep& image_rep2x =
      image_skia.GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_TRUE(!image_rep2x.is_null());
  EXPECT_EQ(ui::SCALE_FACTOR_200P, image_rep2x.scale_factor());
  EXPECT_EQ(kWidth1x, image_rep2x.GetWidth());
  EXPECT_EQ(kHeight1x, image_rep2x.GetHeight());
  EXPECT_EQ(kWidth2x, image_rep2x.pixel_width());
  EXPECT_EQ(kHeight2x, image_rep2x.pixel_height());

  // Check that the image has a single representation.
  gfx::Image image(image_skia);
  EXPECT_EQ(1u, image.RepresentationCount());
}

TEST_F(ImageTest, RemoveFromMultiResolutionImage) {
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  gfx::ImageSkia image_skia;

  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth2x, kHeight2x), ui::SCALE_FACTOR_200P));
  EXPECT_EQ(1u, image_skia.image_reps().size());

  image_skia.RemoveRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_EQ(1u, image_skia.image_reps().size());

  image_skia.RemoveRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_EQ(0u, image_skia.image_reps().size());
}

// Tests that gfx::Image does indeed take ownership of the SkBitmap it is
// passed.
TEST_F(ImageTest, OwnershipTest) {
  gfx::Image image;
  {
    SkBitmap bitmap(gt::CreateBitmap(10, 10));
    EXPECT_TRUE(!bitmap.isNull());
    image = gfx::Image(gfx::ImageSkiaRep(bitmap, ui::SCALE_FACTOR_100P));
  }
  EXPECT_TRUE(!image.ToSkBitmap()->isNull());
}

// Integration tests with UI toolkit frameworks require linking against the
// Views library and cannot be here (ui_unittests doesn't include it). They
// instead live in /chrome/browser/ui/tests/ui_gfx_image_unittest.cc.

}  // namespace
