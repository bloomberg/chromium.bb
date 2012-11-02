// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/point_base.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/point_f.h"

namespace gfx {

namespace {

int TestPointF(const PointF& p) {
  return p.x();
}

}  // namespace

TEST(PointTest, ToPointF) {
  // Check that implicit conversion from integer to float compiles.
  Point a(10, 20);
  float x = TestPointF(a);
  EXPECT_EQ(x, a.x());

  PointF b(10, 20);
  EXPECT_EQ(a, b);
  EXPECT_EQ(b, a);
}

TEST(PointTest, IsOrigin) {
  EXPECT_FALSE(Point(1, 0).IsOrigin());
  EXPECT_FALSE(Point(0, 1).IsOrigin());
  EXPECT_FALSE(Point(1, 2).IsOrigin());
  EXPECT_FALSE(Point(-1, 0).IsOrigin());
  EXPECT_FALSE(Point(0, -1).IsOrigin());
  EXPECT_FALSE(Point(-1, -2).IsOrigin());
  EXPECT_TRUE(Point(0, 0).IsOrigin());

  EXPECT_FALSE(PointF(0.1f, 0).IsOrigin());
  EXPECT_FALSE(PointF(0, 0.1f).IsOrigin());
  EXPECT_FALSE(PointF(0.1f, 2).IsOrigin());
  EXPECT_FALSE(PointF(-0.1f, 0).IsOrigin());
  EXPECT_FALSE(PointF(0, -0.1f).IsOrigin());
  EXPECT_FALSE(PointF(-0.1f, -2).IsOrigin());
  EXPECT_TRUE(PointF(0, 0).IsOrigin());
}

TEST(PointTest, VectorArithmetic) {
  Point a(1, 5);
  Vector2d v1(3, -3);
  Vector2d v2(-8, 1);

  static const struct {
    Point expected;
    Point actual;
  } tests[] = {
    { Point(4, 2), a + v1 },
    { Point(-2, 8), a - v1 },
    { a, a - v1 + v1 },
    { a, a + v1 - v1 },
    { a, a + Vector2d() },
    { Point(12, 1), a + v1 - v2 },
    { Point(-10, 9), a - v1 + v2 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i)
    EXPECT_EQ(tests[i].expected.ToString(), tests[i].actual.ToString());
}

TEST(PointTest, OffsetFromPoint) {
  Point a(1, 5);
  Point b(-20, 8);
  EXPECT_EQ(Vector2d(-20 - 1, 8 - 5).ToString(), b.OffsetFrom(a).ToString());
  EXPECT_EQ(Vector2d(-20 - 1, 8 - 5).ToString(), (b - a).ToString());
}

TEST(PointTest, ToRoundedPoint) {
  EXPECT_EQ(Point(0, 0), ToRoundedPoint(PointF(0, 0)));
  EXPECT_EQ(Point(0, 0), ToRoundedPoint(PointF(0.0001f, 0.0001f)));
  EXPECT_EQ(Point(0, 0), ToRoundedPoint(PointF(0.4999f, 0.4999f)));
  EXPECT_EQ(Point(1, 1), ToRoundedPoint(PointF(0.5f, 0.5f)));
  EXPECT_EQ(Point(1, 1), ToRoundedPoint(PointF(0.9999f, 0.9999f)));

  EXPECT_EQ(Point(10, 10), ToRoundedPoint(PointF(10, 10)));
  EXPECT_EQ(Point(10, 10), ToRoundedPoint(PointF(10.0001f, 10.0001f)));
  EXPECT_EQ(Point(10, 10), ToRoundedPoint(PointF(10.4999f, 10.4999f)));
  EXPECT_EQ(Point(11, 11), ToRoundedPoint(PointF(10.5f, 10.5f)));
  EXPECT_EQ(Point(11, 11), ToRoundedPoint(PointF(10.9999f, 10.9999f)));

  EXPECT_EQ(Point(-10, -10), ToRoundedPoint(PointF(-10, -10)));
  EXPECT_EQ(Point(-10, -10), ToRoundedPoint(PointF(-10.0001f, -10.0001f)));
  EXPECT_EQ(Point(-10, -10), ToRoundedPoint(PointF(-10.4999f, -10.4999f)));
  EXPECT_EQ(Point(-11, -11), ToRoundedPoint(PointF(-10.5f, -10.5f)));
  EXPECT_EQ(Point(-11, -11), ToRoundedPoint(PointF(-10.9999f, -10.9999f)));
}

}  // namespace gfx
