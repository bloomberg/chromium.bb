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

TEST_F(ImageMacTest, MultiResolutionNSImageToSkBitmap) {
  const int width1 = 10;
  const int height1 = 12;
  const int width2 = 20;
  const int height2 = 24;

  NSImageRep* image_rep_1;
  CreateBitmapImageRep(width1, height1, &image_rep_1);
  NSImageRep* image_rep_2;
  CreateBitmapImageRep(width2, height2, &image_rep_2);
  scoped_nsobject<NSImage> ns_image(
      [[NSImage alloc] initWithSize:NSMakeSize(width1, height1)]);
  [ns_image addRepresentation:image_rep_1];
  [ns_image addRepresentation:image_rep_2];

  gfx::Image image(ns_image.release());

  EXPECT_EQ(1u, image.RepresentationCount());
  const std::vector<const SkBitmap*>& bitmaps = image.ToImageSkia()->bitmaps();
  EXPECT_EQ(2u, bitmaps.size());

  const SkBitmap* bitmap1 = bitmaps[0];
  EXPECT_TRUE(bitmap1);
  const SkBitmap* bitmap2 = bitmaps[1];
  EXPECT_TRUE(bitmap2);

  if (bitmap1->width() == width1) {
    EXPECT_EQ(bitmap1->height(), height1);
    EXPECT_EQ(bitmap2->width(), width2);
    EXPECT_EQ(bitmap2->height(), height2);
  } else {
    EXPECT_EQ(bitmap1->width(), width2);
    EXPECT_EQ(bitmap1->height(), height2);
    EXPECT_EQ(bitmap2->width(), width1);
    EXPECT_EQ(bitmap2->height(), height1);
  }

  // ToImageSkia should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

TEST_F(ImageMacTest, MultiResolutionSkBitmapToNSImage) {
  const int width1 = 10;
  const int height1 = 12;
  const int width2 = 20;
  const int height2 = 24;

  std::vector<const SkBitmap*> bitmaps;
  bitmaps.push_back(new SkBitmap(gt::CreateBitmap(width1, height1)));
  bitmaps.push_back(new SkBitmap(gt::CreateBitmap(width2, height2)));
  gfx::Image image(bitmaps);

  EXPECT_EQ(1u, image.RepresentationCount());
  EXPECT_EQ(2u, image.ToImageSkia()->bitmaps().size());

  NSImage* ns_image = image;
  EXPECT_TRUE(ns_image);

  EXPECT_EQ(2u, [[image representations] count]);
  NSImageRep* image_rep_1 = [[image representations] objectAtIndex:0];
  NSImageRep* image_rep_2 = [[image representations] objectAtIndex:1];

  if ([image_rep_1 size].width == width1) {
    EXPECT_EQ([image_rep_1 size].height, height1);
    EXPECT_EQ([image_rep_2 size].width, width2);
    EXPECT_EQ([image_rep_2 size].height, height2);
  } else {
    EXPECT_EQ([image_rep_1 size].width, width2);
    EXPECT_EQ([image_rep_1 size].height, height2);
    EXPECT_EQ([image_rep_2 size].width, width1);
    EXPECT_EQ([image_rep_2 size].height, height1);
  }

  // Cast to NSImage* should create a second representation.
  EXPECT_EQ(2u, image.RepresentationCount());
}

} // namespace
