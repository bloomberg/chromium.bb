// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

class ImageMacTest : public testing::Test {
 public:
  ImageMacTest() {
  }

  ~ImageMacTest() {
  }

  virtual void SetUp() OVERRIDE {
    gfx::test::SetSupportedScaleFactorsTo1xAnd2x();
  }

  void BitmapImageRep(int width, int height,
                      NSBitmapImageRep** image_rep) {
    *image_rep = [[[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                      pixelsWide:width
                      pixelsHigh:height
                   bitsPerSample:8
                 samplesPerPixel:3
                        hasAlpha:NO
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                    bitmapFormat:0
                     bytesPerRow:0
                    bitsPerPixel:0]
                 autorelease];
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ImageMacTest);
};

namespace gt = gfx::test;

TEST_F(ImageMacTest, MultiResolutionNSImageToImageSkia) {
  const int kWidth1x = 10;
  const int kHeight1x = 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  NSBitmapImageRep* ns_image_rep1;
  BitmapImageRep(kWidth1x, kHeight1x, &ns_image_rep1);
  NSBitmapImageRep* ns_image_rep2;
  BitmapImageRep(kWidth2x, kHeight2x, &ns_image_rep2);
  scoped_nsobject<NSImage> ns_image(
      [[NSImage alloc] initWithSize:NSMakeSize(kWidth1x, kHeight1x)]);
  [ns_image addRepresentation:ns_image_rep1];
  [ns_image addRepresentation:ns_image_rep2];

  gfx::Image image(ns_image.release());

  EXPECT_EQ(1u, image.RepresentationCount());

  const gfx::ImageSkia* image_skia = image.ToImageSkia();
  EXPECT_EQ(2u, image_skia->image_reps().size());

  EXPECT_EQ(kWidth1x, image_skia->width());
  EXPECT_EQ(kHeight1x, image_skia->height());

  const gfx::ImageSkiaRep& image_rep1x =
      image_skia->GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(!image_rep1x.is_null());
  EXPECT_EQ(kWidth1x, image_rep1x.GetWidth());
  EXPECT_EQ(kHeight1x, image_rep1x.GetHeight());
  EXPECT_EQ(kWidth1x, image_rep1x.pixel_width());
  EXPECT_EQ(kHeight1x, image_rep1x.pixel_height());
  EXPECT_EQ(ui::SCALE_FACTOR_100P, image_rep1x.scale_factor());

  const gfx::ImageSkiaRep& image_rep2x =
      image_skia->GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_TRUE(!image_rep2x.is_null());
  EXPECT_EQ(kWidth1x, image_rep2x.GetWidth());
  EXPECT_EQ(kHeight1x, image_rep2x.GetHeight());
  EXPECT_EQ(kWidth2x, image_rep2x.pixel_width());
  EXPECT_EQ(kHeight2x, image_rep2x.pixel_height());
  EXPECT_EQ(ui::SCALE_FACTOR_200P, image_rep2x.scale_factor());

  // ToImageSkia should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

// Test that convertng to an ImageSkia from an NSImage with scale factors
// other than 1x and 2x results in an ImageSkia with scale factors 1x and
// 2x;
TEST_F(ImageMacTest, UnalignedMultiResolutionNSImageToImageSkia) {
  const int kWidth1x = 10;
  const int kHeight1x= 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;
  const int kWidth4x = 40;
  const int kHeight4x = 48;

  NSBitmapImageRep* ns_image_rep4;
  BitmapImageRep(kWidth4x, kHeight4x, &ns_image_rep4);
  scoped_nsobject<NSImage> ns_image(
      [[NSImage alloc] initWithSize:NSMakeSize(kWidth1x, kHeight1x)]);
  [ns_image addRepresentation:ns_image_rep4];

  gfx::Image image(ns_image.release());

  EXPECT_EQ(1u, image.RepresentationCount());

  const gfx::ImageSkia* image_skia = image.ToImageSkia();
  EXPECT_EQ(2u, image_skia->image_reps().size());

  EXPECT_EQ(kWidth1x, image_skia->width());
  EXPECT_EQ(kHeight1x, image_skia->height());

  const gfx::ImageSkiaRep& image_rep1x =
      image_skia->GetRepresentation(ui::SCALE_FACTOR_100P);
  EXPECT_TRUE(!image_rep1x.is_null());
  EXPECT_EQ(kWidth1x, image_rep1x.GetWidth());
  EXPECT_EQ(kHeight1x, image_rep1x.GetHeight());
  EXPECT_EQ(kWidth1x, image_rep1x.pixel_width());
  EXPECT_EQ(kHeight1x, image_rep1x.pixel_height());
  EXPECT_EQ(ui::SCALE_FACTOR_100P, image_rep1x.scale_factor());

  const gfx::ImageSkiaRep& image_rep2x =
      image_skia->GetRepresentation(ui::SCALE_FACTOR_200P);
  EXPECT_TRUE(!image_rep2x.is_null());
  EXPECT_EQ(kWidth1x, image_rep2x.GetWidth());
  EXPECT_EQ(kHeight1x, image_rep2x.GetHeight());
  EXPECT_EQ(kWidth2x, image_rep2x.pixel_width());
  EXPECT_EQ(kHeight2x, image_rep2x.pixel_height());
  EXPECT_EQ(ui::SCALE_FACTOR_200P, image_rep2x.scale_factor());

  // ToImageSkia should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

TEST_F(ImageMacTest, MultiResolutionImageSkiaToNSImage) {
  const int kWidth1x = 10;
  const int kHeight1x= 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  gfx::ImageSkia image_skia;
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth1x, kHeight1x), ui::SCALE_FACTOR_100P));
  image_skia.AddRepresentation(gfx::ImageSkiaRep(
      gt::CreateBitmap(kWidth2x, kHeight2x), ui::SCALE_FACTOR_200P));

  gfx::Image image(image_skia);

  EXPECT_EQ(1u, image.RepresentationCount());
  EXPECT_EQ(2u, image.ToImageSkia()->image_reps().size());

  NSImage* ns_image = image.ToNSImage();
  EXPECT_TRUE(ns_image);

  // Image size should be the same as the 1x bitmap.
  EXPECT_EQ([ns_image size].width, kWidth1x);
  EXPECT_EQ([ns_image size].height, kHeight1x);

  EXPECT_EQ(2u, [[ns_image representations] count]);
  NSImageRep* ns_image_rep1 = [[ns_image representations] objectAtIndex:0];
  NSImageRep* ns_image_rep2 = [[ns_image representations] objectAtIndex:1];

  if ([ns_image_rep1 size].width == kWidth1x) {
    EXPECT_EQ([ns_image_rep1 size].width, kWidth1x);
    EXPECT_EQ([ns_image_rep1 size].height, kHeight1x);
    EXPECT_EQ([ns_image_rep2 size].width, kWidth2x);
    EXPECT_EQ([ns_image_rep2 size].height, kHeight2x);
  } else {
    EXPECT_EQ([ns_image_rep1 size].width, kWidth2x);
    EXPECT_EQ([ns_image_rep1 size].height, kHeight2x);
    EXPECT_EQ([ns_image_rep2 size].width, kWidth1x);
    EXPECT_EQ([ns_image_rep2 size].height, kHeight1x);
  }

  // Cast to NSImage* should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

} // namespace
