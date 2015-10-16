// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/PaintInfo.h"

#include <gtest/gtest.h>

namespace blink {

class PaintInfoTest : public testing::Test {
};

TEST_F(PaintInfoTest, intersectsCullRect)
{
    PaintInfo paintInfo(nullptr, IntRect(0, 0, 50, 50), PaintPhaseBlockBackground, GlobalPaintNormalPhase, PaintLayerNoFlag);

    EXPECT_TRUE(paintInfo.intersectsCullRect(IntRect(0, 0, 1, 1)));
    EXPECT_FALSE(paintInfo.intersectsCullRect(IntRect(51, 51, 1, 1)));
}

TEST_F(PaintInfoTest, intersectsCullRectWithLayoutRect)
{
    PaintInfo paintInfo(nullptr, IntRect(0, 0, 50, 50), PaintPhaseBlockBackground, GlobalPaintNormalPhase, PaintLayerNoFlag);

    EXPECT_TRUE(paintInfo.intersectsCullRect(LayoutRect(0, 0, 1, 1)));
    EXPECT_TRUE(paintInfo.intersectsCullRect(LayoutRect(0.1, 0.1, 0.1, 0.1)));
}

TEST_F(PaintInfoTest, intersectsCullRectWithTransform)
{
    PaintInfo paintInfo(nullptr, IntRect(0, 0, 50, 50), PaintPhaseBlockBackground, GlobalPaintNormalPhase, PaintLayerNoFlag);
    AffineTransform transform;
    transform.translate(-2, -2);

    EXPECT_TRUE(paintInfo.intersectsCullRect(transform, IntRect(51, 51, 1, 1)));
    EXPECT_FALSE(paintInfo.intersectsCullRect(IntRect(52, 52, 1, 1)));
}

TEST_F(PaintInfoTest, updateCullRectForSVGTransform)
{
    PaintInfo paintInfo(nullptr, IntRect(1, 1, 50, 50), PaintPhaseBlockBackground, GlobalPaintNormalPhase, PaintLayerNoFlag);
    AffineTransform transform;
    transform.translate(1, 1);
    paintInfo.updateCullRectForSVGTransform(transform);

    EXPECT_TRUE(paintInfo.intersectsCullRect(IntRect(0, 0, 1, 1)));
    EXPECT_FALSE(paintInfo.intersectsCullRect(IntRect(51, 51, 1, 1)));
}

} // namespace blink
