// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // For VC++ to get M_PI. This has to be first.

#include <cmath>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

const double kEpsilon = 1e-7;

}  // namespace

TEST(QuatTest, DefaultConstruction) {
  Quaternion q;
  EXPECT_FLOAT_EQ(0.0, q.x());
  EXPECT_FLOAT_EQ(0.0, q.y());
  EXPECT_FLOAT_EQ(0.0, q.z());
  EXPECT_FLOAT_EQ(1.0, q.w());
}

TEST(QuatTest, AxisAngleCommon) {
  float radians = 0.5;
  Quaternion q(Vector3dF(1.0f, 0.0f, 0.0f), radians);
  EXPECT_FLOAT_EQ(std::sin(radians / 2), q.x());
  EXPECT_FLOAT_EQ(0.0, q.y());
  EXPECT_FLOAT_EQ(0.0, q.z());
  EXPECT_FLOAT_EQ(std::cos(radians / 2), q.w());
}

TEST(QuatTest, AxisAngleWithZeroLengthAxis) {
  Quaternion q(Vector3dF(0.0f, 0.0f, 0.0f), 0.5);
  // If the axis of zero length, we should assume the default values.
  EXPECT_FLOAT_EQ(0.0, q.x());
  EXPECT_FLOAT_EQ(0.0, q.y());
  EXPECT_FLOAT_EQ(0.0, q.z());
  EXPECT_FLOAT_EQ(1.0, q.w());
}

TEST(QuatTest, Addition) {
  float values[] = {0.f, 1.0f, 100.0f};
  for (size_t i = 0; i < arraysize(values); ++i) {
    float t = values[i];
    Quaternion a(t, 2 * t, 3 * t, 4 * t);
    Quaternion b(5 * t, 4 * t, 3 * t, 2 * t);
    Quaternion sum = a + b;
    EXPECT_FLOAT_EQ(6 * t, sum.x());
    EXPECT_FLOAT_EQ(6 * t, sum.y());
    EXPECT_FLOAT_EQ(6 * t, sum.z());
    EXPECT_FLOAT_EQ(6 * t, sum.w());
  }
}

TEST(QuatTest, Multiplication) {
  struct {
    Quaternion a;
    Quaternion b;
    Quaternion expected;
  } cases[] = {
      {Quaternion(1, 0, 0, 0), Quaternion(1, 0, 0, 0), Quaternion(0, 0, 0, -1)},
      {Quaternion(0, 1, 0, 0), Quaternion(0, 1, 0, 0), Quaternion(0, 0, 0, -1)},
      {Quaternion(0, 0, 1, 0), Quaternion(0, 0, 1, 0), Quaternion(0, 0, 0, -1)},
      {Quaternion(0, 0, 0, 1), Quaternion(0, 0, 0, 1), Quaternion(0, 0, 0, 1)},
      {Quaternion(1, 2, 3, 4), Quaternion(5, 6, 7, 8),
       Quaternion(24, 48, 48, -6)},
      {Quaternion(5, 6, 7, 8), Quaternion(1, 2, 3, 4),
       Quaternion(32, 32, 56, -6)},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    Quaternion product = cases[i].a * cases[i].b;
    EXPECT_FLOAT_EQ(cases[i].expected.x(), product.x());
    EXPECT_FLOAT_EQ(cases[i].expected.y(), product.y());
    EXPECT_FLOAT_EQ(cases[i].expected.z(), product.z());
    EXPECT_FLOAT_EQ(cases[i].expected.w(), product.w());
  }
}

TEST(QuatTest, Scaling) {
  float values[] = {0.f, 1.0f, 100.0f};
  for (size_t i = 0; i < arraysize(values); ++i) {
    Quaternion q(1, 2, 3, 4);
    Quaternion qs = q * values[i];
    Quaternion sq = values[i] * q;
    EXPECT_FLOAT_EQ(1 * values[i], qs.x());
    EXPECT_FLOAT_EQ(2 * values[i], qs.y());
    EXPECT_FLOAT_EQ(3 * values[i], qs.z());
    EXPECT_FLOAT_EQ(4 * values[i], qs.w());
    EXPECT_FLOAT_EQ(1 * values[i], sq.x());
    EXPECT_FLOAT_EQ(2 * values[i], sq.y());
    EXPECT_FLOAT_EQ(3 * values[i], sq.z());
    EXPECT_FLOAT_EQ(4 * values[i], sq.w());
  }
}

TEST(QuatTest, Lerp) {
  for (size_t i = 0; i < 100; ++i) {
    Quaternion a(0, 0, 0, 0);
    Quaternion b(1, 2, 3, 4);
    float t = static_cast<float>(i) / 100.0f;
    Quaternion interpolated = a.Lerp(b, t);
    EXPECT_FLOAT_EQ(1 * t, interpolated.x());
    EXPECT_FLOAT_EQ(2 * t, interpolated.y());
    EXPECT_FLOAT_EQ(3 * t, interpolated.z());
    EXPECT_FLOAT_EQ(4 * t, interpolated.w());
  }
}

TEST(QuatTest, Slerp) {
  Vector3dF axis(1, 1, 1);
  double start_radians = -0.5;
  double stop_radians = 0.5;
  Quaternion start(axis, start_radians);
  Quaternion stop(axis, stop_radians);

  for (size_t i = 0; i < 100; ++i) {
    float t = static_cast<float>(i) / 100.0f;
    double radians = (1.0 - t) * start_radians + t * stop_radians;
    Quaternion expected(axis, radians);
    Quaternion interpolated = start.Slerp(stop, t);
    EXPECT_NEAR(expected.x(), interpolated.x(), kEpsilon);
    EXPECT_NEAR(expected.y(), interpolated.y(), kEpsilon);
    EXPECT_NEAR(expected.z(), interpolated.z(), kEpsilon);
    EXPECT_NEAR(expected.w(), interpolated.w(), kEpsilon);
  }
}

TEST(QuatTest, SlerpOppositeAngles) {
  Vector3dF axis(1, 1, 1);
  double start_radians = -M_PI_2;
  double stop_radians = M_PI_2;
  Quaternion start(axis, start_radians);
  Quaternion stop(axis, stop_radians);

  // When quaternions are pointed in the fully opposite direction, we take the
  // interpolated quaternion to be the first. This is arbitrary, but if we
  // change this policy, this test should fail.
  Quaternion expected = start;

  for (size_t i = 0; i < 100; ++i) {
    float t = static_cast<float>(i) / 100.0f;
    Quaternion interpolated = start.Slerp(stop, t);
    EXPECT_FLOAT_EQ(expected.x(), interpolated.x());
    EXPECT_FLOAT_EQ(expected.y(), interpolated.y());
    EXPECT_FLOAT_EQ(expected.z(), interpolated.z());
    EXPECT_FLOAT_EQ(expected.w(), interpolated.w());
  }
}

}  // namespace gfx
