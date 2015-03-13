// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/nine_image_painter.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {

TEST(NineImagePainterTest, GetSubsetRegions) {
  SkBitmap src;
  src.allocN32Pixels(40, 50);
  const ImageSkia image_skia(ImageSkiaRep(src, 1.0));
  const Insets insets(1, 2, 3, 4);
  std::vector<Rect> rects;
  NineImagePainter::GetSubsetRegions(image_skia, insets, &rects);
  ASSERT_EQ(9u, rects.size());
  EXPECT_EQ(gfx::Rect(0, 0, 2, 1), rects[0]);
  EXPECT_EQ(gfx::Rect(2, 0, 34, 1), rects[1]);
  EXPECT_EQ(gfx::Rect(36, 0, 4, 1), rects[2]);
  EXPECT_EQ(gfx::Rect(0, 1, 2, 46), rects[3]);
  EXPECT_EQ(gfx::Rect(2, 1, 34, 46), rects[4]);
  EXPECT_EQ(gfx::Rect(36, 1, 4, 46), rects[5]);
  EXPECT_EQ(gfx::Rect(0, 47, 2, 3), rects[6]);
  EXPECT_EQ(gfx::Rect(2, 47, 34, 3), rects[7]);
  EXPECT_EQ(gfx::Rect(36, 47, 4, 3), rects[8]);
}

}  // namespace gfx
