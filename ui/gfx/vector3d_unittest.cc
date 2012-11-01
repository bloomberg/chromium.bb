// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector3d_f.h"

#include <cmath>
#include <limits>

namespace gfx {

TEST(Vector3dTest, IsZero) {
  gfx::Vector3dF float_zero(0, 0, 0);
  gfx::Vector3dF float_nonzero(0.1f, -0.1f, 0.1f);

  EXPECT_TRUE(float_zero.IsZero());
  EXPECT_FALSE(float_nonzero.IsZero());
}

TEST(Vector3dTest, Add) {
  gfx::Vector3dF f1(3.1f, 5.1f, 2.7f);
  gfx::Vector3dF f2(4.3f, -1.3f, 8.1f);

  const struct {
    gfx::Vector3dF expected;
    gfx::Vector3dF actual;
  } float_tests[] = {
    { gfx::Vector3dF(3.1F, 5.1F, 2.7f), f1 + gfx::Vector3dF() },
    { gfx::Vector3dF(3.1f + 4.3f, 5.1f - 1.3f, 2.7f + 8.1f), f1 + f2 },
    { gfx::Vector3dF(3.1f - 4.3f, 5.1f + 1.3f, 2.7f - 8.1f), f1 - f2 }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(float_tests); ++i)
    EXPECT_EQ(float_tests[i].expected.ToString(),
              float_tests[i].actual.ToString());
}

TEST(Vector3dTest, Negative) {
  const struct {
    gfx::Vector3dF expected;
    gfx::Vector3dF actual;
  } float_tests[] = {
    { gfx::Vector3dF(-0.0f, -0.0f, -0.0f), -gfx::Vector3dF(0, 0, 0) },
    { gfx::Vector3dF(-0.3f, -0.3f, -0.3f), -gfx::Vector3dF(0.3f, 0.3f, 0.3f) },
    { gfx::Vector3dF(0.3f, 0.3f, 0.3f), -gfx::Vector3dF(-0.3f, -0.3f, -0.3f) },
    { gfx::Vector3dF(-0.3f, 0.3f, -0.3f), -gfx::Vector3dF(0.3f, -0.3f, 0.3f) },
    { gfx::Vector3dF(0.3f, -0.3f, -0.3f), -gfx::Vector3dF(-0.3f, 0.3f, 0.3f) },
    { gfx::Vector3dF(-0.3f, -0.3f, 0.3f), -gfx::Vector3dF(0.3f, 0.3f, -0.3f) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(float_tests); ++i)
    EXPECT_EQ(float_tests[i].expected.ToString(),
              float_tests[i].actual.ToString());
}

TEST(Vector3dTest, Scale) {
  float triple_values[][6] = {
    { 4.5f, 1.2f, 1.8f, 3.3f, 5.6f, 4.2f },
    { 4.5f, -1.2f, -1.8f, 3.3f, 5.6f, 4.2f },
    { 4.5f, 1.2f, -1.8f, 3.3f, 5.6f, 4.2f },
    { 4.5f, -1.2f -1.8f, 3.3f, 5.6f, 4.2f },

    { 4.5f, 1.2f, 1.8f, 3.3f, -5.6f, -4.2f },
    { 4.5f, 1.2f, 1.8f, -3.3f, -5.6f, -4.2f },
    { 4.5f, 1.2f, -1.8f, 3.3f, -5.6f, -4.2f },
    { 4.5f, 1.2f, -1.8f, -3.3f, -5.6f, -4.2f },

    { -4.5f, 1.2f, 1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, 1.8f, 0, 5.6f, 4.2f },
    { -4.5f, 1.2f, -1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, -1.8f, 0, 5.6f, 4.2f },

    { -4.5f, 1.2f, 1.8f, 3.3f, 0, 4.2f },
    { 4.5f, 0, 1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, -1.8f, 3.3f, 0, 4.2f },
    { 4.5f, 0, -1.8f, 3.3f, 5.6f, 4.2f },
    { -4.5f, 1.2f, 1.8f, 3.3f, 5.6f, 0 },
    { -4.5f, 1.2f, -1.8f, 3.3f, 5.6f, 0 },

    { 0, 1.2f, 0, 3.3f, 5.6f, 4.2f },
    { 0, 1.2f, 1.8f, 3.3f, 5.6f, 4.2f }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(triple_values); ++i) {
    gfx::Vector3dF v(triple_values[i][0],
                     triple_values[i][1],
                     triple_values[i][2]);
    v.Scale(triple_values[i][3], triple_values[i][4], triple_values[i][5]);
    EXPECT_EQ(v.x(), triple_values[i][0] * triple_values[i][3]);
    EXPECT_EQ(v.y(), triple_values[i][1] * triple_values[i][4]);
    EXPECT_EQ(v.z(), triple_values[i][2] * triple_values[i][5]);
  }

  float single_values[][4] = {
    { 4.5f, 1.2f, 1.8f, 3.3f },
    { 4.5f, -1.2f, 1.8f, 3.3f },
    { 4.5f, 1.2f, -1.8f, 3.3f },
    { 4.5f, -1.2f, -1.8f, 3.3f },
    { -4.5f, 1.2f, 3.3f },
    { -4.5f, 1.2f, 0 },
    { -4.5f, 1.2f, 1.8f, 3.3f },
    { -4.5f, 1.2f, 1.8f, 0 },
    { 4.5f, 0, 1.8f, 3.3f },
    { 0, 1.2f, 1.8f, 3.3f },
    { 4.5f, 0, 1.8f, 3.3f },
    { 0, 1.2f, 1.8f, 3.3f },
    { 4.5f, 1.2f, 0, 3.3f },
    { 4.5f, 1.2f, 0, 3.3f }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(single_values); ++i) {
    gfx::Vector3dF v(single_values[i][0],
                     single_values[i][1],
                     single_values[i][2]);
    v.Scale(single_values[i][3]);
    EXPECT_EQ(v.x(), single_values[i][0] * single_values[i][3]);
    EXPECT_EQ(v.y(), single_values[i][1] * single_values[i][3]);
    EXPECT_EQ(v.z(), single_values[i][2] * single_values[i][3]);
  }
}

TEST(Vector3dTest, Length) {
  float float_values[][3] = {
    { 0, 0, 0 },
    { 10.5f, 20.5f, 8.5f },
    { 20.5f, 10.5f, 8.5f },
    { 8.5f, 20.5f, 10.5f },
    { 10.5f, 8.5f, 20.5f },
    { -10.5f, -20.5f, -8.5f },
    { -20.5f, 10.5f, -8.5f },
    { -8.5f, -20.5f, -10.5f },
    { -10.5f, -8.5f, -20.5f },
    { 10.5f, -20.5f, 8.5f },
    { -10.5f, 20.5f, 8.5f },
    { 10.5f, -20.5f, -8.5f },
    { -10.5f, 20.5f, -8.5f },
    // A large vector that fails if the Length function doesn't use
    // double precision internally.
    { 1236278317862780234892374893213178027.12122348904204230f,
      335890352589839028212313231225425134332.38123f,
      27861786423846742743236423478236784678.236713617231f }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(float_values); ++i) {
    double v0 = float_values[i][0];
    double v1 = float_values[i][1];
    double v2 = float_values[i][2];
    double length_squared =
        static_cast<double>(v0) * v0 +
        static_cast<double>(v1) * v1 +
        static_cast<double>(v2) * v2;
    double length = std::sqrt(length_squared);
    gfx::Vector3dF vector(v0, v1, v2);
    EXPECT_EQ(length_squared, vector.LengthSquared());
    EXPECT_EQ(static_cast<float>(length), vector.Length());
  }
}

TEST(Vector3dTest, DotProduct) {
  const struct {
    float expected;
    gfx::Vector3dF input1;
    gfx::Vector3dF input2;
  } tests[] = {
    { 0, gfx::Vector3dF(1, 0, 0), gfx::Vector3dF(0, 1, 1) },
    { 0, gfx::Vector3dF(0, 1, 0), gfx::Vector3dF(1, 0, 1) },
    { 0, gfx::Vector3dF(0, 0, 1), gfx::Vector3dF(1, 1, 0) },

    { 3, gfx::Vector3dF(1, 1, 1), gfx::Vector3dF(1, 1, 1) },

    { 1.2f, gfx::Vector3dF(1.2f, -1.2f, 1.2f), gfx::Vector3dF(1, 1, 1) },
    { 1.2f, gfx::Vector3dF(1, 1, 1), gfx::Vector3dF(1.2f, -1.2f, 1.2f) },

    { 38.72f,
      gfx::Vector3dF(1.1f, 2.2f, 3.3f), gfx::Vector3dF(4.4f, 5.5f, 6.6f) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    float actual = gfx::DotProduct(tests[i].input1, tests[i].input2);
    EXPECT_EQ(tests[i].expected, actual);
  }
}

TEST(Vector3dTest, CrossProduct) {
  const struct {
    gfx::Vector3dF expected;
    gfx::Vector3dF input1;
    gfx::Vector3dF input2;
  } tests[] = {
    { Vector3dF(), Vector3dF(), Vector3dF(1, 1, 1) },
    { Vector3dF(), Vector3dF(1, 1, 1), Vector3dF() },
    { Vector3dF(), Vector3dF(1, 1, 1), Vector3dF(1, 1, 1) },
    { Vector3dF(),
      Vector3dF(1.6f, 10.6f, -10.6f),
      Vector3dF(1.6f, 10.6f, -10.6f) },

    { Vector3dF(1, -1, 0), Vector3dF(1, 1, 1), Vector3dF(0, 0, 1) },
    { Vector3dF(-1, 0, 1), Vector3dF(1, 1, 1), Vector3dF(0, 1, 0) },
    { Vector3dF(0, 1, -1), Vector3dF(1, 1, 1), Vector3dF(1, 0, 0) },

    { Vector3dF(-1, 1, 0), Vector3dF(0, 0, 1), Vector3dF(1, 1, 1) },
    { Vector3dF(1, 0, -1), Vector3dF(0, 1, 0), Vector3dF(1, 1, 1) },
    { Vector3dF(0, -1, 1), Vector3dF(1, 0, 0), Vector3dF(1, 1, 1) }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    Vector3dF actual = gfx::CrossProduct(tests[i].input1, tests[i].input2);
    EXPECT_EQ(tests[i].expected.ToString(), actual.ToString());
  }

}

}  // namespace gfx
