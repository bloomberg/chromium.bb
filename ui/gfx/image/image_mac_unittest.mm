// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

class ImageMacTest : public testing::Test {
 public:
  void CreateBitmapImageRep(int width, int height, NSImageRep** image_rep) {
    scoped_nsobject<NSImage> image(
        [[NSImage alloc] initWithSize:NSMakeSize(width, height)]);
    [image lockFocus];
    [[NSColor redColor] set];
    NSRectFill(NSMakeRect(0, 0, width, height));
    [image unlockFocus];
    EXPECT_TRUE([[[image representations] lastObject]
        isKindOfClass:[NSImageRep class]]);
    *image_rep = [[image representations] lastObject];
  }
};

namespace gt = gfx::test;

TEST_F(ImageMacTest, NSImageWithResizedNSImageRepToImageSkia) {
  const int kWidth1x = 10;
  const int kHeight1x = 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  NSImageRep* image_rep;
  CreateBitmapImageRep(kWidth2x, kHeight2x, &image_rep);

  scoped_nsobject<NSImage> ns_image(
      [[NSImage alloc] initWithSize:NSMakeSize(kWidth1x, kHeight1x)]);
  [ns_image addRepresentation:image_rep];

  [image_rep setSize:NSMakeSize(kWidth1x, kHeight1x)];

  gfx::Image image(ns_image.release());
  const gfx::ImageSkia* image_skia = image.ToImageSkia();

  float scale_factor;
  const SkBitmap& bitmap = image_skia->GetBitmapForScale(2.0f, 2.0f,
                                                         &scale_factor);
  EXPECT_EQ(2.0f, scale_factor);
  EXPECT_EQ(kWidth2x, bitmap.width());
  EXPECT_EQ(kHeight2x, bitmap.height());
}

TEST_F(ImageMacTest, MultiResolutionNSImageToImageSkia) {
  const int kWidth1x = 10;
  const int kHeight1x = 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  NSImageRep* image_rep_1;
  CreateBitmapImageRep(kWidth1x, kHeight1x, &image_rep_1);
  NSImageRep* image_rep_2;
  CreateBitmapImageRep(kWidth2x, kHeight2x, &image_rep_2);
  scoped_nsobject<NSImage> ns_image(
      [[NSImage alloc] initWithSize:NSMakeSize(kWidth1x, kHeight1x)]);
  [ns_image addRepresentation:image_rep_1];
  [ns_image addRepresentation:image_rep_2];

  gfx::Image image(ns_image.release());

  EXPECT_EQ(1u, image.RepresentationCount());

  const gfx::ImageSkia* image_skia = image.ToImageSkia();
  EXPECT_EQ(2u, image_skia->bitmaps().size());

  float scale_factor;
  const SkBitmap& bitmap1x = image_skia->GetBitmapForScale(1.0f, 1.0f,
                                                           &scale_factor);
  EXPECT_TRUE(!bitmap1x.isNull());
  EXPECT_EQ(1.0f, scale_factor);
  EXPECT_EQ(kWidth1x, bitmap1x.width());
  EXPECT_EQ(kHeight1x, bitmap1x.height());

  const SkBitmap& bitmap2x = image_skia->GetBitmapForScale(2.0f, 2.0f,
                                                           &scale_factor);
  EXPECT_TRUE(!bitmap2x.isNull());
  EXPECT_EQ(2.0f, scale_factor);
  EXPECT_EQ(kWidth2x, bitmap2x.width());
  EXPECT_EQ(kHeight2x, bitmap2x.height());

  // ToImageSkia should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

TEST_F(ImageMacTest, MultiResolutionImageSkiaToNSImage) {
  const int kWidth1x = 10;
  const int kHeight1x= 12;
  const int kWidth2x = 20;
  const int kHeight2x = 24;

  gfx::ImageSkia image_skia;
  image_skia.AddBitmapForScale(gt::CreateBitmap(kWidth1x, kHeight1x), 1.0f);
  image_skia.AddBitmapForScale(gt::CreateBitmap(kWidth2x, kHeight2x), 2.0f);

  gfx::Image image(image_skia);

  EXPECT_EQ(1u, image.RepresentationCount());
  EXPECT_EQ(2u, image.ToImageSkia()->bitmaps().size());

  NSImage* ns_image = image;
  EXPECT_TRUE(ns_image);

  // Image size should be the same as the 1x bitmap.
  EXPECT_EQ([ns_image size].width, kWidth1x);
  EXPECT_EQ([ns_image size].height, kHeight1x);

  EXPECT_EQ(2u, [[image representations] count]);
  NSImageRep* image_rep_1 = [[image representations] objectAtIndex:0];
  NSImageRep* image_rep_2 = [[image representations] objectAtIndex:1];

  if ([image_rep_1 size].width == kWidth1x) {
    EXPECT_EQ([image_rep_1 size].width, kWidth1x);
    EXPECT_EQ([image_rep_1 size].height, kHeight1x);
    EXPECT_EQ([image_rep_2 size].width, kWidth2x);
    EXPECT_EQ([image_rep_2 size].height, kHeight2x);
  } else {
    EXPECT_EQ([image_rep_1 size].width, kWidth2x);
    EXPECT_EQ([image_rep_1 size].height, kHeight2x);
    EXPECT_EQ([image_rep_2 size].width, kWidth1x);
    EXPECT_EQ([image_rep_2 size].height, kHeight1x);
  }

  // Cast to NSImage* should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

} // namespace
