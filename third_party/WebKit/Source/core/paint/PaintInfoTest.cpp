// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInfo.h"

#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class PaintInfoTest : public testing::Test {
 protected:
  PaintInfoTest()
      : paint_controller_(PaintController::Create()),
        context_(*paint_controller_) {}

  std::unique_ptr<PaintController> paint_controller_;
  GraphicsContext context_;
};

TEST_F(PaintInfoTest, intersectsCullRect) {
  PaintInfo paint_info(context_, IntRect(0, 0, 50, 50),
                       kPaintPhaseSelfBlockBackgroundOnly,
                       kGlobalPaintNormalPhase, kPaintLayerNoFlag);

  EXPECT_TRUE(paint_info.GetCullRect().IntersectsCullRect(IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(
      paint_info.GetCullRect().IntersectsCullRect(IntRect(51, 51, 1, 1)));
}

TEST_F(PaintInfoTest, intersectsCullRectWithLayoutRect) {
  PaintInfo paint_info(context_, IntRect(0, 0, 50, 50),
                       kPaintPhaseSelfBlockBackgroundOnly,
                       kGlobalPaintNormalPhase, kPaintLayerNoFlag);

  EXPECT_TRUE(
      paint_info.GetCullRect().IntersectsCullRect(LayoutRect(0, 0, 1, 1)));
  EXPECT_TRUE(paint_info.GetCullRect().IntersectsCullRect(LayoutRect(
      LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1), LayoutUnit(0.1))));
}

TEST_F(PaintInfoTest, intersectsCullRectWithTransform) {
  PaintInfo paint_info(context_, IntRect(0, 0, 50, 50),
                       kPaintPhaseSelfBlockBackgroundOnly,
                       kGlobalPaintNormalPhase, kPaintLayerNoFlag);
  AffineTransform transform;
  transform.Translate(-2, -2);

  EXPECT_TRUE(paint_info.GetCullRect().IntersectsCullRect(
      transform, IntRect(51, 51, 1, 1)));
  EXPECT_FALSE(
      paint_info.GetCullRect().IntersectsCullRect(IntRect(52, 52, 1, 1)));
}

TEST_F(PaintInfoTest, updateCullRect) {
  PaintInfo paint_info(context_, IntRect(1, 1, 50, 50),
                       kPaintPhaseSelfBlockBackgroundOnly,
                       kGlobalPaintNormalPhase, kPaintLayerNoFlag);
  AffineTransform transform;
  transform.Translate(1, 1);
  paint_info.UpdateCullRect(transform);

  EXPECT_TRUE(paint_info.GetCullRect().IntersectsCullRect(IntRect(0, 0, 1, 1)));
  EXPECT_FALSE(
      paint_info.GetCullRect().IntersectsCullRect(IntRect(51, 51, 1, 1)));
}

TEST_F(PaintInfoTest, intersectsVerticalRange) {
  PaintInfo paint_info(context_, IntRect(0, 0, 50, 100),
                       kPaintPhaseSelfBlockBackgroundOnly,
                       kGlobalPaintNormalPhase, kPaintLayerNoFlag);

  EXPECT_TRUE(paint_info.GetCullRect().IntersectsVerticalRange(LayoutUnit(),
                                                               LayoutUnit(1)));
  EXPECT_FALSE(paint_info.GetCullRect().IntersectsVerticalRange(
      LayoutUnit(100), LayoutUnit(101)));
}

TEST_F(PaintInfoTest, intersectsHorizontalRange) {
  PaintInfo paint_info(context_, IntRect(0, 0, 50, 100),
                       kPaintPhaseSelfBlockBackgroundOnly,
                       kGlobalPaintNormalPhase, kPaintLayerNoFlag);

  EXPECT_TRUE(paint_info.GetCullRect().IntersectsHorizontalRange(
      LayoutUnit(), LayoutUnit(1)));
  EXPECT_FALSE(paint_info.GetCullRect().IntersectsHorizontalRange(
      LayoutUnit(50), LayoutUnit(51)));
}

}  // namespace blink
