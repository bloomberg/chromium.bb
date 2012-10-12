// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/point_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_f.h"

namespace ui {

static int test_pointf(const gfx::PointF& p) {
  return p.x();
}

TEST(PointTest, ToPointF) {
  // Check that implicit conversion from integer to float compiles.
  gfx::Point a(10, 20);
  float x = test_pointf(a);
  EXPECT_EQ(x, a.x());

  gfx::PointF b(10, 20);
  bool equals = a == b;
  EXPECT_EQ(true, equals);

  equals = b == a;
  EXPECT_EQ(true, equals);
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

}  // namespace ui
