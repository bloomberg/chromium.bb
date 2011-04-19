// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_mac.mm"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SkiaUtilsMacTest : public testing::Test {
 public:
  // Creates a red or blue bitmap.
  SkBitmap CreateSkBitmap(int width, int height, bool isred, bool tfbit);

  // Creates a red or blue image.
  NSImage* CreateNSImage(int width, int height, bool isred);

  // Checks that the given bitmap rep is actually red or blue.
  void TestImageRep(NSBitmapImageRep* imageRep, bool isred);

  // Checks that the given bitmap is actually red or blue.
  void TestSkBitmap(const SkBitmap& bitmap, bool isred);

  // If not red, is blue.
  // If not tfbit (twenty-four-bit), is 444.
  void ShapeHelper(int width, int height, bool isred, bool tfbit);
};

SkBitmap SkiaUtilsMacTest::CreateSkBitmap(int width, int height,
                                          bool isred, bool tfbit) {
  SkBitmap bitmap;

  if (tfbit)
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  else
    bitmap.setConfig(SkBitmap::kARGB_4444_Config, width, height);
  bitmap.allocPixels();

  if (isred)
    bitmap.eraseRGB(0xff, 0, 0);
  else
    bitmap.eraseRGB(0, 0, 0xff);

  return bitmap;
}

NSImage* SkiaUtilsMacTest::CreateNSImage(int width, int height, bool isred) {
  NSImage* image = [[[NSImage alloc] initWithSize:NSMakeSize(width, height)]
      autorelease];
  [image lockFocus];
  if (isred)
    [[NSColor colorWithDeviceRed:1.0 green:0.0 blue:0.0 alpha:1.0] set];
  else
    [[NSColor colorWithDeviceRed:0.0 green:0.0 blue:1.0 alpha:1.0] set];
  NSRectFill(NSMakeRect(0, 0, width, height));
  [image unlockFocus];
  return image;
}

void SkiaUtilsMacTest::TestImageRep(NSBitmapImageRep* imageRep, bool isred) {
  // Get the color of a pixel and make sure it looks fine
  int x = [imageRep size].width > 17 ? 17 : 0;
  int y = [imageRep size].height > 17 ? 17 : 0;
  NSColor* color = [imageRep colorAtX:x y:y];
  CGFloat red = 0, green = 0, blue = 0, alpha = 0;

  // SkBitmapToNSImage returns a bitmap in the calibrated color space (sRGB),
  // while NSReadPixel returns a color in the device color space. Convert back
  // to the calibrated color space before testing.
  color = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];

  [color getRed:&red green:&green blue:&blue alpha:&alpha];

  // Be tolerant of floating point rounding and lossy color space conversions.
  if (isred) {
    EXPECT_NEAR(red, 1.0, 0.025);
    EXPECT_NEAR(blue, 0.0, 0.025);
  } else {
    EXPECT_NEAR(red, 0.0, 0.025);
    EXPECT_NEAR(blue, 1.0, 0.025);
  }
  EXPECT_NEAR(green, 0.0, 0.025);
  EXPECT_NEAR(alpha, 1.0, 0.025);
}

void SkiaUtilsMacTest::TestSkBitmap(const SkBitmap& bitmap, bool isred) {
  int x = bitmap.width() > 17 ? 17 : 0;
  int y = bitmap.height() > 17 ? 17 : 0;
  SkColor color = bitmap.getColor(x, y);

  // Due to colorspace issues the colors may not match exactly.
  // TODO(sail): Need to fix this, http://crbug.com/79946
  if (isred) {
    EXPECT_NEAR(255u, SkColorGetR(color), 20);
    EXPECT_NEAR(0u, SkColorGetB(color), 20);
  } else {
    EXPECT_NEAR(0u, SkColorGetR(color), 20);
    EXPECT_NEAR(255u, SkColorGetB(color), 20);
  }
  EXPECT_NEAR(0u, SkColorGetG(color), 20);
  EXPECT_NEAR(255u, SkColorGetA(color), 20);
}

void SkiaUtilsMacTest::ShapeHelper(int width, int height,
                                   bool isred, bool tfbit) {
  SkBitmap thing(CreateSkBitmap(width, height, isred, tfbit));

  // Confirm size
  NSImage* image = gfx::SkBitmapToNSImage(thing);
  EXPECT_DOUBLE_EQ([image size].width, (double)width);
  EXPECT_DOUBLE_EQ([image size].height, (double)height);

  EXPECT_TRUE([[image representations] count] == 1);
  EXPECT_TRUE([[[image representations] lastObject]
      isKindOfClass:[NSBitmapImageRep class]]);
  TestImageRep([[image representations] lastObject], isred);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_RedSquare64x64) {
  ShapeHelper(64, 64, true, true);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_BlueRectangle199x19) {
  ShapeHelper(199, 19, false, true);
}

TEST_F(SkiaUtilsMacTest, BitmapToNSImage_BlueRectangle444) {
  ShapeHelper(200, 200, false, false);
}

TEST_F(SkiaUtilsMacTest, MultipleBitmapsToNSImage) {
  int redWidth = 10;
  int redHeight = 15;
  int blueWidth = 20;
  int blueHeight = 30;

  SkBitmap redBitmap(CreateSkBitmap(redWidth, redHeight, true, true));
  SkBitmap blueBitmap(CreateSkBitmap(blueWidth, blueHeight, false, true));
  std::vector<const SkBitmap*> bitmaps;
  bitmaps.push_back(&redBitmap);
  bitmaps.push_back(&blueBitmap);

  NSImage* image = gfx::SkBitmapsToNSImage(bitmaps);

  // Image size should be the same as the smallest bitmap.
  EXPECT_DOUBLE_EQ(redWidth, [image size].width);
  EXPECT_DOUBLE_EQ(redHeight, [image size].height);

  EXPECT_EQ(2u, [[image representations] count]);

  for (NSBitmapImageRep* imageRep in [image representations]) {
    NSBitmapImageRep* imageRep = [[image representations] objectAtIndex:0];
    bool isred = [imageRep size].width == redWidth;
    if (isred) {
      EXPECT_DOUBLE_EQ(redHeight, [imageRep size].height);
    } else {
      EXPECT_DOUBLE_EQ(blueWidth, [imageRep size].width);
      EXPECT_DOUBLE_EQ(blueHeight, [imageRep size].height);
    }
    TestImageRep(imageRep, isred);
  }
}

TEST_F(SkiaUtilsMacTest, NSImageRepToSkBitmap) {
  int width = 10;
  int height = 15;
  bool isred = true;

  NSImage* image = CreateNSImage(width, height, isred);
  EXPECT_EQ(1u, [[image representations] count]);
  NSBitmapImageRep* imageRep = [[image representations] lastObject];
  SkBitmap bitmap(gfx::NSImageRepToSkBitmap(imageRep, [image size], false));
  TestSkBitmap(bitmap, isred);
}

}  // namespace
