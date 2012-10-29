// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"

#include <limits>

namespace ui {

TEST(RectTest, Contains) {
  static const struct ContainsCase {
    int rect_x;
    int rect_y;
    int rect_width;
    int rect_height;
    int point_x;
    int point_y;
    bool contained;
  } contains_cases[] = {
    {0, 0, 10, 10, 0, 0, true},
    {0, 0, 10, 10, 5, 5, true},
    {0, 0, 10, 10, 9, 9, true},
    {0, 0, 10, 10, 5, 10, false},
    {0, 0, 10, 10, 10, 5, false},
    {0, 0, 10, 10, -1, -1, false},
    {0, 0, 10, 10, 50, 50, false},
  #if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
    {0, 0, -10, -10, 0, 0, false},
  #endif
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(contains_cases); ++i) {
    const ContainsCase& value = contains_cases[i];
    gfx::Rect rect(value.rect_x, value.rect_y,
                   value.rect_width, value.rect_height);
    EXPECT_EQ(value.contained, rect.Contains(value.point_x, value.point_y));
  }
}

TEST(RectTest, Intersects) {
  static const struct {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    bool intersects;
  } tests[] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, false },
    { 0, 0, 10, 10, 0, 0, 10, 10, true },
    { 0, 0, 10, 10, 10, 10, 10, 10, false },
    { 10, 10, 10, 10, 0, 0, 10, 10, false },
    { 10, 10, 10, 10, 5, 5, 10, 10, true },
    { 10, 10, 10, 10, 15, 15, 10, 10, true },
    { 10, 10, 10, 10, 20, 15, 10, 10, false },
    { 10, 10, 10, 10, 21, 15, 10, 10, false }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    EXPECT_EQ(tests[i].intersects, r1.Intersects(r2));
  }
}

TEST(RectTest, Intersect) {
  static const struct {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: the union of rects 1 and 2
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 0, 0,   // zeros
      0, 0, 0, 0,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,   // equal
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 0, 0, 4, 4,   // neighboring
      4, 4, 4, 4,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,   // overlapping corners
      2, 2, 4, 4,
      2, 2, 2, 2 },
    { 0, 0, 4, 4,   // T junction
      3, 1, 4, 2,
      3, 1, 1, 2 },
    { 3, 0, 2, 2,   // gap
      0, 0, 2, 2,
      0, 0, 0, 0 }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    gfx::Rect ir = gfx::IntersectRects(r1, r2);
    EXPECT_EQ(r3.x(), ir.x());
    EXPECT_EQ(r3.y(), ir.y());
    EXPECT_EQ(r3.width(), ir.width());
    EXPECT_EQ(r3.height(), ir.height());
  }
}

TEST(RectTest, Union) {
  static const struct Test {
    int x1;  // rect 1
    int y1;
    int w1;
    int h1;
    int x2;  // rect 2
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: the union of rects 1 and 2
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0 },
    { 0, 0, 4, 4,
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 0, 0, 4, 4,
      4, 4, 4, 4,
      0, 0, 8, 8 },
    { 0, 0, 4, 4,
      0, 5, 4, 4,
      0, 0, 4, 9 },
    { 0, 0, 2, 2,
      3, 3, 2, 2,
      0, 0, 5, 5 },
    { 3, 3, 2, 2,   // reverse r1 and r2 from previous test
      0, 0, 2, 2,
      0, 0, 5, 5 },
    { 0, 0, 0, 0,   // union with empty rect
      2, 2, 2, 2,
      2, 2, 2, 2 }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    gfx::Rect u = gfx::UnionRects(r1, r2);
    EXPECT_EQ(r3.x(), u.x());
    EXPECT_EQ(r3.y(), u.y());
    EXPECT_EQ(r3.width(), u.width());
    EXPECT_EQ(r3.height(), u.height());
  }
}

TEST(RectTest, Equals) {
  ASSERT_TRUE(gfx::Rect(0, 0, 0, 0) == gfx::Rect(0, 0, 0, 0));
  ASSERT_TRUE(gfx::Rect(1, 2, 3, 4) == gfx::Rect(1, 2, 3, 4));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0) == gfx::Rect(0, 0, 0, 1));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0) == gfx::Rect(0, 0, 1, 0));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0) == gfx::Rect(0, 1, 0, 0));
  ASSERT_FALSE(gfx::Rect(0, 0, 0, 0) == gfx::Rect(1, 0, 0, 0));
}

TEST(RectTest, AdjustToFit) {
  static const struct Test {
    int x1;  // source
    int y1;
    int w1;
    int h1;
    int x2;  // target
    int y2;
    int w2;
    int h2;
    int x3;  // rect 3: results of invoking AdjustToFit
    int y3;
    int w3;
    int h3;
  } tests[] = {
    { 0, 0, 2, 2,
      0, 0, 2, 2,
      0, 0, 2, 2 },
    { 2, 2, 3, 3,
      0, 0, 4, 4,
      1, 1, 3, 3 },
    { -1, -1, 5, 5,
      0, 0, 4, 4,
      0, 0, 4, 4 },
    { 2, 2, 4, 4,
      0, 0, 3, 3,
      0, 0, 3, 3 },
    { 2, 2, 1, 1,
      0, 0, 3, 3,
      2, 2, 1, 1 }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);
    gfx::Rect r3(tests[i].x3, tests[i].y3, tests[i].w3, tests[i].h3);
    gfx::Rect u = r1;
    u.AdjustToFit(r2);
    EXPECT_EQ(r3.x(), u.x());
    EXPECT_EQ(r3.y(), u.y());
    EXPECT_EQ(r3.width(), u.width());
    EXPECT_EQ(r3.height(), u.height());
  }
}

TEST(RectTest, Subtract) {
  gfx::Rect result;

  // Matching
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(10, 10, 20, 20));
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(), result.ToString());

  // Contains
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(5, 5, 30, 30));
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(), result.ToString());

  // No intersection
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(30, 30, 30, 30));
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), result.ToString());

  // Not a complete intersection in either direction
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(15, 15, 20, 20));
  EXPECT_EQ(gfx::Rect(10, 10, 20, 20).ToString(), result.ToString());

  // Complete intersection in the x-direction
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(10, 15, 20, 20));
  EXPECT_EQ(gfx::Rect(10, 10, 20, 5).ToString(), result.ToString());

  // Complete intersection in the x-direction
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(5, 15, 30, 20));
  EXPECT_EQ(gfx::Rect(10, 10, 20, 5).ToString(), result.ToString());

  // Complete intersection in the x-direction
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(5, 5, 30, 20));
  EXPECT_EQ(gfx::Rect(10, 25, 20, 5).ToString(), result.ToString());

  // Complete intersection in the y-direction
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(10, 10, 10, 30));
  EXPECT_EQ(gfx::Rect(20, 10, 10, 20).ToString(), result.ToString());

  // Complete intersection in the y-direction
  result = gfx::Rect(10, 10, 20, 20);
  result.Subtract(gfx::Rect(5, 5, 20, 30));
  EXPECT_EQ(gfx::Rect(25, 10, 5, 20).ToString(), result.ToString());
}

TEST(RectTest, IsEmpty) {
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 0).IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 0).size().IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 10, 0).IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 10, 0).size().IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 10).IsEmpty());
  EXPECT_TRUE(gfx::Rect(0, 0, 0, 10).size().IsEmpty());
  EXPECT_FALSE(gfx::Rect(0, 0, 10, 10).IsEmpty());
  EXPECT_FALSE(gfx::Rect(0, 0, 10, 10).size().IsEmpty());
}

TEST(RectTest, SplitVertically) {
  gfx::Rect left_half, right_half;

  // Splitting when origin is (0, 0).
  gfx::Rect(0, 0, 20, 20).SplitVertically(&left_half, &right_half);
  EXPECT_TRUE(left_half == gfx::Rect(0, 0, 10, 20));
  EXPECT_TRUE(right_half == gfx::Rect(10, 0, 10, 20));

  // Splitting when origin is arbitrary.
  gfx::Rect(10, 10, 20, 10).SplitVertically(&left_half, &right_half);
  EXPECT_TRUE(left_half == gfx::Rect(10, 10, 10, 10));
  EXPECT_TRUE(right_half == gfx::Rect(20, 10, 10, 10));

  // Splitting a rectangle of zero width.
  gfx::Rect(10, 10, 0, 10).SplitVertically(&left_half, &right_half);
  EXPECT_TRUE(left_half == gfx::Rect(10, 10, 0, 10));
  EXPECT_TRUE(right_half == gfx::Rect(10, 10, 0, 10));

  // Splitting a rectangle of odd width.
  gfx::Rect(10, 10, 5, 10).SplitVertically(&left_half, &right_half);
  EXPECT_TRUE(left_half == gfx::Rect(10, 10, 2, 10));
  EXPECT_TRUE(right_half == gfx::Rect(12, 10, 3, 10));
}

TEST(RectTest, CenterPoint) {
  gfx::Point center;

  // When origin is (0, 0).
  center = gfx::Rect(0, 0, 20, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::Point(10, 10));

  // When origin is even.
  center = gfx::Rect(10, 10, 20, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::Point(20, 20));

  // When origin is odd.
  center = gfx::Rect(11, 11, 20, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::Point(21, 21));

  // When 0 width or height.
  center = gfx::Rect(10, 10, 0, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::Point(10, 20));
  center = gfx::Rect(10, 10, 20, 0).CenterPoint();
  EXPECT_TRUE(center == gfx::Point(20, 10));

  // When an odd size.
  center = gfx::Rect(10, 10, 21, 21).CenterPoint();
  EXPECT_TRUE(center == gfx::Point(20, 20));

  // When an odd size and position.
  center = gfx::Rect(11, 11, 21, 21).CenterPoint();
  EXPECT_TRUE(center == gfx::Point(21, 21));
}

TEST(RectTest, CenterPointF) {
  gfx::PointF center;

  // When origin is (0, 0).
  center = gfx::RectF(0, 0, 20, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::PointF(10, 10));

  // When origin is even.
  center = gfx::RectF(10, 10, 20, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::PointF(20, 20));

  // When origin is odd.
  center = gfx::RectF(11, 11, 20, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::PointF(21, 21));

  // When 0 width or height.
  center = gfx::RectF(10, 10, 0, 20).CenterPoint();
  EXPECT_TRUE(center == gfx::PointF(10, 20));
  center = gfx::RectF(10, 10, 20, 0).CenterPoint();
  EXPECT_TRUE(center == gfx::PointF(20, 10));

  // When an odd size.
  center = gfx::RectF(10, 10, 21, 21).CenterPoint();
  EXPECT_TRUE(center == gfx::PointF(20.5f, 20.5f));

  // When an odd size and position.
  center = gfx::RectF(11, 11, 21, 21).CenterPoint();
  EXPECT_TRUE(center == gfx::PointF(21.5f, 21.5f));
}

TEST(RectTest, SharesEdgeWith) {
  gfx::Rect r(2, 3, 4, 5);

  // Must be non-overlapping
  EXPECT_FALSE(r.SharesEdgeWith(r));

  gfx::Rect just_above(2, 1, 4, 2);
  gfx::Rect just_below(2, 8, 4, 2);
  gfx::Rect just_left(0, 3, 2, 5);
  gfx::Rect just_right(6, 3, 2, 5);

  EXPECT_TRUE(r.SharesEdgeWith(just_above));
  EXPECT_TRUE(r.SharesEdgeWith(just_below));
  EXPECT_TRUE(r.SharesEdgeWith(just_left));
  EXPECT_TRUE(r.SharesEdgeWith(just_right));

  // Wrong placement
  gfx::Rect same_height_no_edge(0, 0, 1, 5);
  gfx::Rect same_width_no_edge(0, 0, 4, 1);

  EXPECT_FALSE(r.SharesEdgeWith(same_height_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(same_width_no_edge));

  gfx::Rect just_above_no_edge(2, 1, 5, 2);  // too wide
  gfx::Rect just_below_no_edge(2, 8, 3, 2);  // too narrow
  gfx::Rect just_left_no_edge(0, 3, 2, 6);   // too tall
  gfx::Rect just_right_no_edge(6, 3, 2, 4);  // too short

  EXPECT_FALSE(r.SharesEdgeWith(just_above_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(just_below_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(just_left_no_edge));
  EXPECT_FALSE(r.SharesEdgeWith(just_right_no_edge));
}

TEST(RectTest, SkRectToRect) {
  gfx::Rect src(10, 20, 30, 40);
  SkRect skrect = gfx::RectToSkRect(src);
  EXPECT_EQ(src, gfx::SkRectToRect(skrect));
}

// Similar to EXPECT_FLOAT_EQ, but lets NaN equal NaN
#define EXPECT_FLOAT_AND_NAN_EQ(a, b) \
  { if (a == a || b == b) { EXPECT_FLOAT_EQ(a, b); } }

TEST(RectTest, ScaleRect) {
  static const struct Test {
    int x1;  // source
    int y1;
    int w1;
    int h1;
    float scale;
    float x2;  // target
    float y2;
    float w2;
    float h2;
  } tests[] = {
    { 3, 3, 3, 3,
      1.5f,
      4.5f, 4.5f, 4.5f, 4.5f },
    { 3, 3, 3, 3,
      0.0f,
      0.0f, 0.0f, 0.0f, 0.0f },
    { 3, 3, 3, 3,
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN() },
    { 3, 3, 3, 3,
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max() },
    { 3, 3, 3, 3,
      -1.0f,
      -3.0f, -3.0f, 0.0f, 0.0f }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::Rect r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::RectF r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);

    gfx::RectF scaled = gfx::ScaleRect(r1, tests[i].scale);
    EXPECT_FLOAT_AND_NAN_EQ(r2.x(), scaled.x());
    EXPECT_FLOAT_AND_NAN_EQ(r2.y(), scaled.y());
    EXPECT_FLOAT_AND_NAN_EQ(r2.width(), scaled.width());
    EXPECT_FLOAT_AND_NAN_EQ(r2.height(), scaled.height());
  }
}

TEST(RectTest, ToEnclosedRect) {
  static const struct Test {
    float x1; // source
    float y1;
    float w1;
    float h1;
    int x2; // target
    int y2;
    int w2;
    int h2;
  } tests [] = {
    { 0.0f, 0.0f, 0.0f, 0.0f,
      0, 0, 0, 0 },
    { -1.5f, -1.5f, 3.0f, 3.0f,
      -1, -1, 2, 2 },
    { -1.5f, -1.5f, 3.5f, 3.5f,
      -1, -1, 3, 3 },
    { std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      2.0f, 2.0f,
      std::numeric_limits<int>::max(),
      std::numeric_limits<int>::max(),
      0, 0 },
    { 0.0f, 0.0f,
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      0, 0,
      std::numeric_limits<int>::max(),
      std::numeric_limits<int>::max() },
    { 20000.5f, 20000.5f, 0.5f, 0.5f,
      20001, 20001, 0, 0 },
    { std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      0, 0, 0, 0 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::RectF r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);

    gfx::Rect enclosed = ToEnclosedRect(r1);
    EXPECT_FLOAT_AND_NAN_EQ(r2.x(), enclosed.x());
    EXPECT_FLOAT_AND_NAN_EQ(r2.y(), enclosed.y());
    EXPECT_FLOAT_AND_NAN_EQ(r2.width(), enclosed.width());
    EXPECT_FLOAT_AND_NAN_EQ(r2.height(), enclosed.height());
  }
}

TEST(RectTest, ToEnclosingRect) {
  static const struct Test {
    float x1; // source
    float y1;
    float w1;
    float h1;
    int x2; // target
    int y2;
    int w2;
    int h2;
  } tests [] = {
    { 0.0f, 0.0f, 0.0f, 0.0f,
      0, 0, 0, 0 },
    { -1.5f, -1.5f, 3.0f, 3.0f,
      -2, -2, 4, 4 },
    { -1.5f, -1.5f, 3.5f, 3.5f,
      -2, -2, 4, 4 },
    { std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      2.0f, 2.0f,
      std::numeric_limits<int>::max(),
      std::numeric_limits<int>::max(),
      0, 0 },
    { 0.0f, 0.0f,
      std::numeric_limits<float>::max(),
      std::numeric_limits<float>::max(),
      0, 0,
      std::numeric_limits<int>::max(),
      std::numeric_limits<int>::max() },
    { 20000.5f, 20000.5f, 0.5f, 0.5f,
      20000, 20000, 1, 1 },
    { std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      std::numeric_limits<float>::quiet_NaN(),
      0, 0, 0, 0 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::RectF r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);

    gfx::Rect enclosed = ToEnclosingRect(r1);
    EXPECT_FLOAT_AND_NAN_EQ(r2.x(), enclosed.x());
    EXPECT_FLOAT_AND_NAN_EQ(r2.y(), enclosed.y());
    EXPECT_FLOAT_AND_NAN_EQ(r2.width(), enclosed.width());
    EXPECT_FLOAT_AND_NAN_EQ(r2.height(), enclosed.height());
  }
}

TEST(RectTest, ToFlooredRect) {
  static const struct Test {
    float x1; // source
    float y1;
    float w1;
    float h1;
    int x2; // target
    int y2;
    int w2;
    int h2;
  } tests [] = {
    { 0.0f, 0.0f, 0.0f, 0.0f,
      0, 0, 0, 0 },
    { -1.5f, -1.5f, 3.0f, 3.0f,
      -2, -2, 3, 3 },
    { -1.5f, -1.5f, 3.5f, 3.5f,
      -2, -2, 3, 3 },
    { 20000.5f, 20000.5f, 0.5f, 0.5f,
      20000, 20000, 0, 0 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    gfx::RectF r1(tests[i].x1, tests[i].y1, tests[i].w1, tests[i].h1);
    gfx::Rect r2(tests[i].x2, tests[i].y2, tests[i].w2, tests[i].h2);

    gfx::Rect floored = ToFlooredRectDeprecated(r1);
    EXPECT_FLOAT_EQ(r2.x(), floored.x());
    EXPECT_FLOAT_EQ(r2.y(), floored.y());
    EXPECT_FLOAT_EQ(r2.width(), floored.width());
    EXPECT_FLOAT_EQ(r2.height(), floored.height());
  }
}

#if defined(OS_WIN)
TEST(RectTest, ConstructAndAssign) {
  const RECT rect_1 = { 0, 0, 10, 10 };
  const RECT rect_2 = { 0, 0, -10, -10 };
  gfx::Rect test1(rect_1);
  gfx::Rect test2(rect_2);
}
#endif

TEST(RectTest, ToRectF) {
  // Check that implicit conversion from integer to float compiles.
  gfx::Rect a(10, 20, 30, 40);
  gfx::RectF b(10, 20, 30, 40);

  gfx::RectF intersect = gfx::IntersectRects(a, b);
  EXPECT_EQ(b.ToString(), intersect.ToString());

  EXPECT_EQ(a, b);
  EXPECT_EQ(b, a);
}

}  // namespace ui
