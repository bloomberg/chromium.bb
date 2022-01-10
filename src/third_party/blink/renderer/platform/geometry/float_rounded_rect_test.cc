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

#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

#define TEST_INTERCEPTS(roundedRect, yCoordinate, expectedMinXIntercept, \
                        expectedMaxXIntercept)                           \
  {                                                                      \
    float min_x_intercept;                                               \
    float max_x_intercept;                                               \
    EXPECT_TRUE(roundedRect.XInterceptsAtY(yCoordinate, min_x_intercept, \
                                           max_x_intercept));            \
    EXPECT_FLOAT_EQ(expectedMinXIntercept, min_x_intercept);             \
    EXPECT_FLOAT_EQ(expectedMaxXIntercept, max_x_intercept);             \
  }

TEST(FloatRoundedRectTest, zeroRadii) {
  FloatRoundedRect r = FloatRoundedRect(1, 2, 3, 4);

  EXPECT_EQ(gfx::RectF(1, 2, 3, 4), r.Rect());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().TopLeft());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().TopRight());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().BottomLeft());
  EXPECT_EQ(gfx::SizeF(), r.GetRadii().BottomRight());
  EXPECT_TRUE(r.GetRadii().IsZero());
  EXPECT_FALSE(r.IsRounded());
  EXPECT_FALSE(r.IsEmpty());

  EXPECT_EQ(gfx::RectF(1, 2, 0, 0), r.TopLeftCorner());
  EXPECT_EQ(gfx::RectF(4, 2, 0, 0), r.TopRightCorner());
  EXPECT_EQ(gfx::RectF(4, 6, 0, 0), r.BottomRightCorner());
  EXPECT_EQ(gfx::RectF(1, 6, 0, 0), r.BottomLeftCorner());

  TEST_INTERCEPTS(r, 2, r.Rect().x(), r.Rect().right());
  TEST_INTERCEPTS(r, 4, r.Rect().x(), r.Rect().right());
  TEST_INTERCEPTS(r, 6, r.Rect().x(), r.Rect().right());

  float min_x_intercept;
  float max_x_intercept;

  EXPECT_FALSE(r.XInterceptsAtY(1, min_x_intercept, max_x_intercept));
  EXPECT_FALSE(r.XInterceptsAtY(7, min_x_intercept, max_x_intercept));

  // The FloatRoundedRect::expandRadii() function doesn't change radii
  // gfx::SizeFs that are <= zero. Same as RoundedRect::expandRadii().
  r.ExpandRadii(20);
  r.ShrinkRadii(10);
  EXPECT_TRUE(r.GetRadii().IsZero());
}

TEST(FloatRoundedRectTest, circle) {
  gfx::SizeF corner_radii(50, 50);
  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), corner_radii, corner_radii,
                     corner_radii, corner_radii);

  EXPECT_EQ(gfx::RectF(0, 0, 100, 100), r.Rect());
  EXPECT_EQ(corner_radii, r.GetRadii().TopLeft());
  EXPECT_EQ(corner_radii, r.GetRadii().TopRight());
  EXPECT_EQ(corner_radii, r.GetRadii().BottomLeft());
  EXPECT_EQ(corner_radii, r.GetRadii().BottomRight());
  EXPECT_FALSE(r.GetRadii().IsZero());
  EXPECT_TRUE(r.IsRounded());
  EXPECT_FALSE(r.IsEmpty());

  EXPECT_EQ(gfx::RectF(0, 0, 50, 50), r.TopLeftCorner());
  EXPECT_EQ(gfx::RectF(50, 0, 50, 50), r.TopRightCorner());
  EXPECT_EQ(gfx::RectF(0, 50, 50, 50), r.BottomLeftCorner());
  EXPECT_EQ(gfx::RectF(50, 50, 50, 50), r.BottomRightCorner());

  TEST_INTERCEPTS(r, 0, 50, 50);
  TEST_INTERCEPTS(r, 25, 6.69873, 93.3013);
  TEST_INTERCEPTS(r, 50, 0, 100);
  TEST_INTERCEPTS(r, 75, 6.69873, 93.3013);
  TEST_INTERCEPTS(r, 100, 50, 50);

  float min_x_intercept;
  float max_x_intercept;

  EXPECT_FALSE(r.XInterceptsAtY(-1, min_x_intercept, max_x_intercept));
  EXPECT_FALSE(r.XInterceptsAtY(101, min_x_intercept, max_x_intercept));
}

/*
 * FloatRoundedRect geometry for this test. Corner radii are in parens, x and y
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
TEST(FloatRoundedRectTest, ellipticalCorners) {
  FloatRoundedRect::Radii corner_radii;
  corner_radii.SetTopLeft(gfx::SizeF(10, 15));
  corner_radii.SetTopRight(gfx::SizeF(10, 20));
  corner_radii.SetBottomLeft(gfx::SizeF(25, 15));
  corner_radii.SetBottomRight(gfx::SizeF(20, 30));

  FloatRoundedRect r(gfx::RectF(0, 0, 100, 100), corner_radii);

  EXPECT_EQ(r.GetRadii(),
            FloatRoundedRect::Radii(gfx::SizeF(10, 15), gfx::SizeF(10, 20),
                                    gfx::SizeF(25, 15), gfx::SizeF(20, 30)));
  EXPECT_EQ(r, FloatRoundedRect(gfx::RectF(0, 0, 100, 100), corner_radii));

  EXPECT_EQ(gfx::RectF(0, 0, 10, 15), r.TopLeftCorner());
  EXPECT_EQ(gfx::RectF(90, 0, 10, 20), r.TopRightCorner());
  EXPECT_EQ(gfx::RectF(0, 85, 25, 15), r.BottomLeftCorner());
  EXPECT_EQ(gfx::RectF(80, 70, 20, 30), r.BottomRightCorner());

  TEST_INTERCEPTS(r, 5, 2.5464401, 96.61438);
  TEST_INTERCEPTS(r, 15, 0, 99.682457);
  TEST_INTERCEPTS(r, 20, 0, 100);
  TEST_INTERCEPTS(r, 50, 0, 100);
  TEST_INTERCEPTS(r, 70, 0, 100);
  TEST_INTERCEPTS(r, 85, 0, 97.320511);
  TEST_INTERCEPTS(r, 95, 6.3661003, 91.05542);

  float min_x_intercept;
  float max_x_intercept;

  EXPECT_FALSE(r.XInterceptsAtY(-1, min_x_intercept, max_x_intercept));
  EXPECT_FALSE(r.XInterceptsAtY(101, min_x_intercept, max_x_intercept));
}

TEST(FloatRoundedRectTest, radiusCenterRect) {
  gfx::SizeF corner_rect(10, 10);
  FloatRoundedRect r0(gfx::RectF(0, 0, 100, 50),
                      FloatRoundedRect::Radii(corner_rect, corner_rect,
                                              corner_rect, corner_rect));
  EXPECT_EQ(gfx::RectF(10, 10, 80, 30), r0.RadiusCenterRect());

  // "Degenerate" cases all return an empty rectangle.
  gfx::RectF collapsed_rect(0, 0, 100, 50);
  collapsed_rect.Inset(200);
  FloatRoundedRect r1(collapsed_rect);
  EXPECT_TRUE(r1.RadiusCenterRect().IsEmpty());

  FloatRoundedRect::Radii radii_with_too_large_corner(
      gfx::SizeF(55, 55), gfx::SizeF(), gfx::SizeF(), gfx::SizeF());
  FloatRoundedRect r2(gfx::RectF(0, 0, 100, 50), radii_with_too_large_corner);
  EXPECT_TRUE(r2.RadiusCenterRect().IsEmpty());
}

TEST(FloatRoundedRectTest, IntersectsQuadIsInclusive) {
  FloatRoundedRect::Radii corner_radii;
  corner_radii.SetTopLeft(gfx::SizeF(5, 5));
  corner_radii.SetTopRight(gfx::SizeF(5, 5));
  corner_radii.SetBottomLeft(gfx::SizeF(5, 5));
  corner_radii.SetBottomRight(gfx::SizeF(5, 5));
  // A rect at (10, 10) with dimensions 20x20 and radii of size 5x5.
  FloatRoundedRect r(gfx::RectF(10, 10, 20, 20), corner_radii);

  // A quad fully inside the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(11, 11, 8, 8))));

  // A quad fully outside the rounded rect should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(FloatQuad(gfx::RectF(0, 0, 1, 1))));

  // A quad touching the top edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(15, 9, 5, 1))));

  // A quad touching the right edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(30, 15, 1, 1))));

  // A quad touching the bottom edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(15, 30, 1, 1))));

  // A quad touching the left edge of the rounded rect should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(9, 15, 1, 1))));

  // A quad outside the top-left arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(FloatQuad(gfx::RectF(10, 10, 1, 1))));

  // A quad inside the top-left arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(13, 13, 1, 1))));

  // A quad outside the top-right arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(FloatQuad(gfx::RectF(29, 10, 1, 1))));

  // A quad inside the top-right arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(26, 13, 1, 1))));

  // A quad outside the bottom-right arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(FloatQuad(gfx::RectF(29, 29, 1, 1))));

  // A quad inside the bottom-right arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(26, 26, 1, 1))));

  // A quad outside the bottom-left arc should not intersect.
  EXPECT_FALSE(r.IntersectsQuad(FloatQuad(gfx::RectF(10, 29, 1, 1))));

  // A quad inside the bottom-left arc should intersect.
  EXPECT_TRUE(r.IntersectsQuad(FloatQuad(gfx::RectF(13, 26, 1, 1))));
}

TEST(FloatRoundedrectTest, ConstrainRadii) {
  FloatRoundedRect empty;
  empty.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(), empty);

  FloatRoundedRect r1(-100, -100, 200, 200);
  r1.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(-100, -100, 200, 200), r1);

  FloatRoundedRect r2(gfx::RectF(-100, -100, 200, 200), 10);
  r2.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200), 10), r2);

  FloatRoundedRect r3(gfx::RectF(-100, -100, 200, 200), 100);
  r3.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200), 100), r3);

  FloatRoundedRect r4(gfx::RectF(-100, -100, 200, 200), 160);
  r4.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200), 100), r4);

  FloatRoundedRect r5(gfx::RectF(-100, -100, 200, 200), gfx::SizeF(10, 20),
                      gfx::SizeF(100, 250), gfx::SizeF(200, 60),
                      gfx::SizeF(50, 150));
  r5.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200),
                             gfx::SizeF(5, 10), gfx::SizeF(50, 125),
                             gfx::SizeF(100, 30), gfx::SizeF(25, 75)),
            r5);

  FloatRoundedRect r6(gfx::RectF(-100, -100, 200, 200), gfx::SizeF(10, 20),
                      gfx::SizeF(60, 200), gfx::SizeF(250, 100),
                      gfx::SizeF(150, 50));
  r6.ConstrainRadii();
  EXPECT_EQ(FloatRoundedRect(gfx::RectF(-100, -100, 200, 200),
                             gfx::SizeF(5, 10), gfx::SizeF(30, 100),
                             gfx::SizeF(125, 50), gfx::SizeF(75, 25)),
            r6);

  FloatRoundedRect r7(gfx::RectF(0, 0, 85089, 21377),
                      gfx::SizeF(1388.89, 1388.89),
                      gfx::SizeF(58711.2, 14750.3), gfx::SizeF(0, 13467.7),
                      gfx::SizeF(85088.6, 21377.3));
  r7.ConstrainRadii();
  EXPECT_TRUE(r7.IsRenderable());
}

TEST(FloatRoundedRectTest, ToString) {
  gfx::SizeF corner_rect(1, 2);
  FloatRoundedRect rounded_rect(
      gfx::RectF(3, 5, 7, 11),
      FloatRoundedRect::Radii(corner_rect, corner_rect, corner_rect,
                              corner_rect));
  EXPECT_EQ("3,5 7x11 radii:(tl:1x2; tr:1x2; bl:1x2; br:1x2)",
            rounded_rect.ToString());

  FloatRoundedRect infinite((gfx::RectF(LayoutRect::InfiniteIntRect())));
  EXPECT_EQ("InfiniteIntRect", infinite.ToString());

  FloatRoundedRect rect_without_radii(gfx::RectF(1, 3, 5, 7));
  EXPECT_EQ("1,3 5x7", rect_without_radii.ToString());
}

}  // namespace blink
