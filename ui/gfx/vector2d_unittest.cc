// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/vector2d_f.h"

#include <cmath>
#include <limits>

namespace gfx {

TEST(Vector2dTest, ConversionToFloat) {
  gfx::Vector2d i(3, 4);
  gfx::Vector2dF f = i;
  EXPECT_EQ(i, f);
}

TEST(Vector2dTest, IsZero) {
  gfx::Vector2d int_zero(0, 0);
  gfx::Vector2d int_nonzero(2, -2);
  gfx::Vector2dF float_zero(0, 0);
  gfx::Vector2dF float_nonzero(0.1f, -0.1f);

  EXPECT_TRUE(int_zero.IsZero());
  EXPECT_FALSE(int_nonzero.IsZero());
  EXPECT_TRUE(float_zero.IsZero());
  EXPECT_FALSE(float_nonzero.IsZero());
}

TEST(Vector2dTest, Add) {
  gfx::Vector2d i1(3, 5);
  gfx::Vector2d i2(4, -1);

  const struct {
    gfx::Vector2d expected;
    gfx::Vector2d actual;
  } int_tests[] = {
    { gfx::Vector2d(3, 5), i1 + gfx::Vector2d() },
    { gfx::Vector2d(3 + 4, 5 - 1), i1 + i2 },
    { gfx::Vector2d(3 - 4, 5 + 1), i1 - i2 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(int_tests); ++i)
    EXPECT_EQ(int_tests[i].expected.ToString(),
              int_tests[i].actual.ToString());

  gfx::Vector2dF f1(3.1f, 5.1f);
  gfx::Vector2dF f2(4.3f, -1.3f);

  const struct {
    gfx::Vector2dF expected;
    gfx::Vector2dF actual;
  } float_tests[] = {
    { gfx::Vector2dF(3.1F, 5.1F), f1 + gfx::Vector2d() },
    { gfx::Vector2dF(3.1F, 5.1F), f1 + gfx::Vector2dF() },
    { gfx::Vector2dF(3.1f + 4.3f, 5.1f - 1.3f), f1 + f2 },
    { gfx::Vector2dF(3.1f - 4.3f, 5.1f + 1.3f), f1 - f2 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(float_tests); ++i)
    EXPECT_EQ(float_tests[i].expected.ToString(),
              float_tests[i].actual.ToString());
}

TEST(Vector2dTest, Negative) {
  const struct {
    gfx::Vector2d expected;
    gfx::Vector2d actual;
  } int_tests[] = {
    { gfx::Vector2d(0, 0), -gfx::Vector2d(0, 0) },
    { gfx::Vector2d(-3, -3), -gfx::Vector2d(3, 3) },
    { gfx::Vector2d(3, 3), -gfx::Vector2d(-3, -3) },
    { gfx::Vector2d(-3, 3), -gfx::Vector2d(3, -3) },
    { gfx::Vector2d(3, -3), -gfx::Vector2d(-3, 3) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(int_tests); ++i)
    EXPECT_EQ(int_tests[i].expected.ToString(),
              int_tests[i].actual.ToString());

  const struct {
    gfx::Vector2dF expected;
    gfx::Vector2dF actual;
  } float_tests[] = {
    { gfx::Vector2dF(0, 0), -gfx::Vector2d(0, 0) },
    { gfx::Vector2dF(-0.3f, -0.3f), -gfx::Vector2dF(0.3f, 0.3f) },
    { gfx::Vector2dF(0.3f, 0.3f), -gfx::Vector2dF(-0.3f, -0.3f) },
    { gfx::Vector2dF(-0.3f, 0.3f), -gfx::Vector2dF(0.3f, -0.3f) },
    { gfx::Vector2dF(0.3f, -0.3f), -gfx::Vector2dF(-0.3f, 0.3f) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(float_tests); ++i)
    EXPECT_EQ(float_tests[i].expected.ToString(),
              float_tests[i].actual.ToString());
}

TEST(Vector2dTest, Scale) {
  float double_values[][4] = {
    { 4.5f, 1.2f, 3.3f, 5.6f },
    { 4.5f, -1.2f, 3.3f, 5.6f },
    { 4.5f, 1.2f, 3.3f, -5.6f },
    { 4.5f, 1.2f, -3.3f, -5.6f },
    { -4.5f, 1.2f, 3.3f, 5.6f },
    { -4.5f, 1.2f, 0, 5.6f },
    { -4.5f, 1.2f, 3.3f, 0 },
    { 4.5f, 0, 3.3f, 5.6f },
    { 0, 1.2f, 3.3f, 5.6f }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(double_values); ++i) {
    gfx::Vector2dF v(double_values[i][0], double_values[i][1]);
    v.Scale(double_values[i][2], double_values[i][3]);
    EXPECT_EQ(v.x(), double_values[i][0] * double_values[i][2]);
    EXPECT_EQ(v.y(), double_values[i][1] * double_values[i][3]);
  }

  float single_values[][3] = {
    { 4.5f, 1.2f, 3.3f },
    { 4.5f, -1.2f, 3.3f },
    { 4.5f, 1.2f, 3.3f },
    { 4.5f, 1.2f, -3.3f },
    { -4.5f, 1.2f, 3.3f },
    { -4.5f, 1.2f, 0 },
    { -4.5f, 1.2f, 3.3f },
    { 4.5f, 0, 3.3f },
    { 0, 1.2f, 3.3f }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(single_values); ++i) {
    gfx::Vector2dF v(single_values[i][0], single_values[i][1]);
    v.Scale(single_values[i][2]);
    EXPECT_EQ(v.x(), single_values[i][0] * single_values[i][2]);
    EXPECT_EQ(v.y(), single_values[i][1] * single_values[i][2]);
  }
}

TEST(Vector2dTest, Length) {
  int int_values[][2] = {
    { 0, 0 },
    { 10, 20 },
    { 20, 10 },
    { -10, -20 },
    { -20, 10 },
    { 10, -20 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(int_values); ++i) {
    int v0 = int_values[i][0];
    int v1 = int_values[i][1];
    double length_squared =
        static_cast<double>(v0) * v0 + static_cast<double>(v1) * v1;
    double length = std::sqrt(length_squared);
    gfx::Vector2d vector(v0, v1);
    EXPECT_EQ(static_cast<float>(length_squared), vector.LengthSquared());
    EXPECT_EQ(static_cast<float>(length), vector.Length());
  }

  float float_values[][2] = {
    { 0, 0 },
    { 10.5f, 20.5f },
    { 20.5f, 10.5f },
    { -10.5f, -20.5f },
    { -20.5f, 10.5f },
    { 10.5f, -20.5f },
    // A large vector that fails if the Length function doesn't use
    // double precision internally.
    { 1236278317862780234892374893213178027.12122348904204230f,
      335890352589839028212313231225425134332.38123f },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(float_values); ++i) {
    double v0 = float_values[i][0];
    double v1 = float_values[i][1];
    double length_squared =
        static_cast<double>(v0) * v0 + static_cast<double>(v1) * v1;
    double length = std::sqrt(length_squared);
    gfx::Vector2dF vector(v0, v1);
    EXPECT_EQ(length_squared, vector.LengthSquared());
    EXPECT_EQ(static_cast<float>(length), vector.Length());
  }
}

}  // namespace ui
