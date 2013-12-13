/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/shapes/BoxShape.h"

#include <gtest/gtest.h>

namespace WebCore {

class BoxShapeTest : public ::testing::Test {
protected:
    BoxShapeTest() { }

    PassOwnPtr<Shape> createBoxShape(const LayoutSize& size, float shapeMargin, float shapePadding)
    {
        return Shape::createLayoutBoxShape(size, TopToBottomWritingMode, Length(shapeMargin, Fixed), Length(shapePadding, Fixed));
    }
};

} // namespace WebCore

namespace {

using namespace WebCore;

#define TEST_EXCLUDED_INTERVAL(shapePtr, lineTop, lineHeight, expectedLeft, expectedRight) \
{                                                                                          \
    SegmentList result;                                                                    \
    shapePtr->getExcludedIntervals(lineTop, lineHeight, result);                           \
    EXPECT_EQ(1u, result.size());                                                          \
    EXPECT_EQ(expectedLeft, result[0].logicalLeft);                                        \
    EXPECT_EQ(expectedRight, result[0].logicalRight);                                      \
}

#define TEST_NO_EXCLUDED_INTERVAL(shapePtr, lineTop, lineHeight) \
{                                                                \
    SegmentList result;                                          \
    shapePtr->getExcludedIntervals(lineTop, lineHeight, result); \
    EXPECT_EQ(0u, result.size());                                \
}

TEST_F(BoxShapeTest, zeroRadii)
{
    OwnPtr<Shape> shape = createBoxShape(LayoutSize(100, 50), 10, 20);
    EXPECT_FALSE(shape->isEmpty());

    EXPECT_EQ(LayoutRect(-10, -10, 120, 70), shape->shapeMarginLogicalBoundingBox());
    EXPECT_EQ(LayoutRect(20, 20, 60, 10), shape->shapePaddingLogicalBoundingBox());

    // A BoxShape's bounds include the top edge but not the bottom edge.
    // Similarly a "line", specified as top,height to the overlap methods,
    // is defined as top <= y < top + height.

    EXPECT_TRUE(shape->lineOverlapsShapeMarginBounds(-9, 1));
    EXPECT_TRUE(shape->lineOverlapsShapeMarginBounds(-10, 0));
    EXPECT_TRUE(shape->lineOverlapsShapeMarginBounds(-10, 200));
    EXPECT_TRUE(shape->lineOverlapsShapeMarginBounds(5, 10));
    EXPECT_TRUE(shape->lineOverlapsShapeMarginBounds(59, 1));

    EXPECT_FALSE(shape->lineOverlapsShapeMarginBounds(-12, 2));
    EXPECT_FALSE(shape->lineOverlapsShapeMarginBounds(60, 1));
    EXPECT_FALSE(shape->lineOverlapsShapeMarginBounds(100, 200));

    TEST_EXCLUDED_INTERVAL(shape, -9, 1, -10, 110);
    TEST_EXCLUDED_INTERVAL(shape, -10, 0, -10, 110);
    TEST_EXCLUDED_INTERVAL(shape, -10, 200, -10, 110);
    TEST_EXCLUDED_INTERVAL(shape, 5, 10, -10, 110);
    TEST_EXCLUDED_INTERVAL(shape, 59, 1, -10, 110);

    TEST_NO_EXCLUDED_INTERVAL(shape, -12, 2);
    TEST_NO_EXCLUDED_INTERVAL(shape, 60, 1);
    TEST_NO_EXCLUDED_INTERVAL(shape, 100, 200);

    EXPECT_TRUE(shape->lineOverlapsShapePaddingBounds(21, 1));
    EXPECT_TRUE(shape->lineOverlapsShapePaddingBounds(20, 0));
    EXPECT_TRUE(shape->lineOverlapsShapePaddingBounds(-10, 200));
    EXPECT_TRUE(shape->lineOverlapsShapePaddingBounds(25, 35));
    EXPECT_TRUE(shape->lineOverlapsShapePaddingBounds(29, 1));

    EXPECT_FALSE(shape->lineOverlapsShapePaddingBounds(18, 2));
    EXPECT_FALSE(shape->lineOverlapsShapePaddingBounds(30, 1));
    EXPECT_FALSE(shape->lineOverlapsShapePaddingBounds(100, 200));
}

} // namespace
