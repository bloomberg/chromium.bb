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

#include "third_party/blink/renderer/core/layout/shapes/box_shape.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"

namespace blink {

class BoxShapeTest : public testing::Test {
 protected:
  BoxShapeTest() = default;

  std::unique_ptr<Shape> CreateBoxShape(const FloatRoundedRect& bounds,
                                        float shape_margin) {
    return Shape::CreateLayoutBoxShape(bounds, WritingMode::kHorizontalTb,
                                       shape_margin);
  }
};

namespace {

#define TEST_EXCLUDED_INTERVAL(shapePtr, lineTop, lineHeight, expectedLeft,   \
                               expectedRight)                                 \
  {                                                                           \
    LineSegment segment = shapePtr->GetExcludedInterval(lineTop, lineHeight); \
    EXPECT_TRUE(segment.is_valid);                                            \
    if (segment.is_valid) {                                                   \
      EXPECT_FLOAT_EQ(expectedLeft, segment.logical_left);                    \
      EXPECT_FLOAT_EQ(expectedRight, segment.logical_right);                  \
    }                                                                         \
  }

#define TEST_NO_EXCLUDED_INTERVAL(shapePtr, lineTop, lineHeight)              \
  {                                                                           \
    LineSegment segment = shapePtr->GetExcludedInterval(lineTop, lineHeight); \
    EXPECT_FALSE(segment.is_valid);                                           \
  }

/* The BoxShape is based on a 100x50 rectangle at 0,0. The shape-margin value is
 * 10, so the shapeMarginBoundingBox rectangle is 120x70 at -10,-10:
 *
 *   -10,-10   110,-10
 *       +--------+
 *       |        |
 *       +--------+
 *   -10,60     60,60
 */
TEST_F(BoxShapeTest, zeroRadii) {
  std::unique_ptr<Shape> shape =
      CreateBoxShape(FloatRoundedRect(0, 0, 100, 50), 10);
  EXPECT_FALSE(shape->IsEmpty());

  EXPECT_EQ(LayoutRect(-10, -10, 120, 70),
            shape->ShapeMarginLogicalBoundingBox());

  // A BoxShape's bounds include the top edge but not the bottom edge.
  // Similarly a "line", specified as top,height to the overlap methods,
  // is defined as top <= y < top + height.

  EXPECT_TRUE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(-9), LayoutUnit(1)));
  EXPECT_TRUE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(-10), LayoutUnit()));
  EXPECT_TRUE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(-10), LayoutUnit(200)));
  EXPECT_TRUE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(5), LayoutUnit(10)));
  EXPECT_TRUE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(59), LayoutUnit(1)));

  EXPECT_FALSE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(-12), LayoutUnit(2)));
  EXPECT_FALSE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(60), LayoutUnit(1)));
  EXPECT_FALSE(
      shape->LineOverlapsShapeMarginBounds(LayoutUnit(100), LayoutUnit(200)));

  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(-9), LayoutUnit(1), -10, 110);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(-10), LayoutUnit(), -10, 110);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(-10), LayoutUnit(200), -10, 110);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(5), LayoutUnit(10), -10, 110);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(59), LayoutUnit(1), -10, 110);

  TEST_NO_EXCLUDED_INTERVAL(shape, LayoutUnit(-12), LayoutUnit(2));
  TEST_NO_EXCLUDED_INTERVAL(shape, LayoutUnit(60), LayoutUnit(1));
  TEST_NO_EXCLUDED_INTERVAL(shape, LayoutUnit(100), LayoutUnit(200));
}

/* BoxShape geometry for this test. Corner radii are in parens, x and y
 * intercepts for the elliptical corners are noted. The rectangle itself is at
 * 0,0 with width and height 100.
 *
 *         (10, 15)  x=10      x=90 (10, 20)
 *                (--+---------+--)
 *           y=15 +--|         |-+ y=20
 *                |               |
 *                |               |
 *           y=85 + -|         |- + y=70
 *                (--+---------+--)
 *       (25, 15)  x=25      x=80  (20, 30)
 */
TEST_F(BoxShapeTest, getIntervals) {
  const FloatRoundedRect::Radii corner_radii(
      FloatSize(10, 15), FloatSize(10, 20), FloatSize(25, 15),
      FloatSize(20, 30));
  std::unique_ptr<Shape> shape = CreateBoxShape(
      FloatRoundedRect(IntRect(0, 0, 100, 100), corner_radii), 0);
  EXPECT_FALSE(shape->IsEmpty());

  EXPECT_EQ(LayoutRect(0, 0, 100, 100), shape->ShapeMarginLogicalBoundingBox());

  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(10), LayoutUnit(95), 0, 100);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(5), LayoutUnit(25), 0, 100);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(15), LayoutUnit(6), 0, 100);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(20), LayoutUnit(50), 0, 100);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(69), LayoutUnit(5), 0, 100);
  TEST_EXCLUDED_INTERVAL(shape, LayoutUnit(85), LayoutUnit(10), 0, 97.3125f);
}

}  // anonymous namespace

}  // namespace blink
