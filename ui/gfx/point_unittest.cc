// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/point_base.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_f.h"

namespace ui {

namespace {

int TestPointF(const gfx::PointF& p) {
  return p.x();
}

}  // namespace

TEST(PointTest, ToPointF) {
  // Check that implicit conversion from integer to float compiles.
  gfx::Point a(10, 20);
  float x = TestPointF(a);
  EXPECT_EQ(x, a.x());

  gfx::PointF b(10, 20);
  EXPECT_EQ(a, b);
  EXPECT_EQ(b, a);
}

TEST(PointTest, IsOrigin) {
  EXPECT_FALSE(gfx::Point(1, 0).IsOrigin());
  EXPECT_FALSE(gfx::Point(0, 1).IsOrigin());
  EXPECT_FALSE(gfx::Point(1, 2).IsOrigin());
  EXPECT_FALSE(gfx::Point(-1, 0).IsOrigin());
  EXPECT_FALSE(gfx::Point(0, -1).IsOrigin());
  EXPECT_FALSE(gfx::Point(-1, -2).IsOrigin());
  EXPECT_TRUE(gfx::Point(0, 0).IsOrigin());

  EXPECT_FALSE(gfx::PointF(0.1f, 0).IsOrigin());
  EXPECT_FALSE(gfx::PointF(0, 0.1f).IsOrigin());
  EXPECT_FALSE(gfx::PointF(0.1f, 2).IsOrigin());
  EXPECT_FALSE(gfx::PointF(-0.1f, 0).IsOrigin());
  EXPECT_FALSE(gfx::PointF(0, -0.1f).IsOrigin());
  EXPECT_FALSE(gfx::PointF(-0.1f, -2).IsOrigin());
  EXPECT_TRUE(gfx::PointF(0, 0).IsOrigin());
}

TEST(PointTest, VectorArithmetic) {
  gfx::Point a(1, 5);
  gfx::Vector2d v1(3, -3);
  gfx::Vector2d v2(-8, 1);

  static const struct {
    gfx::Point expected;
    gfx::Point actual;
  } tests[] = {
    { gfx::Point(4, 2), a + v1 },
    { gfx::Point(-2, 8), a - v1 },
    { a, a - v1 + v1 },
    { a, a + v1 - v1 },
    { a, a + gfx::Vector2d() },
    { gfx::Point(12, 1), a + v1 - v2 },
    { gfx::Point(-10, 9), a - v1 + v2 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i)
    EXPECT_EQ(tests[i].expected.ToString(),
              tests[i].actual.ToString());
}

TEST(PointTest, OffsetFromPoint) {
  gfx::Point a(1, 5);
  gfx::Point b(-20, 8);
  EXPECT_EQ(gfx::Vector2d(-20 - 1, 8 - 5).ToString(),
            b.OffsetFrom(a).ToString());
  EXPECT_EQ(gfx::Vector2d(-20 - 1, 8 - 5).ToString(),
            (b - a).ToString());
}

}  // namespace ui
