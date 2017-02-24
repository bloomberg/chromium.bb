// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mac/GraphicsContextCanvas.h"
#include "skia/ext/skia_utils_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

enum TestType {
  TestIdentity = 0,
  TestTranslate = 1,
  TestClip = 2,
  TestXClip = TestTranslate | TestClip,
};

void RunTest(TestType test) {
  const unsigned width = 2;
  const unsigned height = 2;
  const unsigned storageSize = width * height;
  const unsigned original[] = {0xFF333333, 0xFF666666, 0xFF999999, 0xFFCCCCCC};
  EXPECT_EQ(storageSize, sizeof(original) / sizeof(original[0]));
  unsigned bits[storageSize];
  memcpy(bits, original, sizeof(original));
  SkImageInfo info = SkImageInfo::MakeN32Premul(width, height);
  SkBitmap bitmap;
  bitmap.installPixels(info, bits, info.minRowBytes());

  PaintCanvas canvas(bitmap);
  if (test & TestTranslate)
    canvas.translate(width / 2, 0);
  if (test & TestClip) {
    SkRect clipRect = {0, height / 2, width, height};
    canvas.clipRect(clipRect);
  }
  {
    SkIRect clip =
        SkIRect::MakeSize(canvas.getBaseLayerSize())
            .makeOffset(
                (test & TestTranslate) ? -(static_cast<int>(width)) / 2 : 0, 0);
    GraphicsContextCanvas bitLocker(&canvas, clip);
    CGContextRef cgContext = bitLocker.cgContext();
    CGColorRef testColor = CGColorGetConstantColor(kCGColorWhite);
    CGContextSetFillColorWithColor(cgContext, testColor);
    CGRect cgRect = {{0, 0}, {width, height}};
    CGContextFillRect(cgContext, cgRect);
  }
  const unsigned results[][storageSize] = {
      {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},  // identity
      {0xFF333333, 0xFFFFFFFF, 0xFF999999, 0xFFFFFFFF},  // translate
      {0xFF333333, 0xFF666666, 0xFFFFFFFF, 0xFFFFFFFF},  // clip
      {0xFF333333, 0xFF666666, 0xFF999999, 0xFFFFFFFF}   // translate | clip
  };
  for (unsigned index = 0; index < storageSize; index++)
    EXPECT_EQ(results[test][index], bits[index]);
}

TEST(GraphicsContextCanvasTest, Identity) {
  RunTest(TestIdentity);
}

TEST(GraphicsContextCanvasTest, Translate) {
  RunTest(TestTranslate);
}

TEST(GraphicsContextCanvasTest, Clip) {
  RunTest(TestClip);
}

TEST(GraphicsContextCanvasTest, XClip) {
  RunTest(TestXClip);
}

}  // namespace
