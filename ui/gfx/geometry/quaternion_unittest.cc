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

void CompareQuaternions(const Quaternion& a, const Quaternion& b) {
  EXPECT_FLOAT_EQ(a.x(), b.x());
  EXPECT_FLOAT_EQ(a.y(), b.y());
  EXPECT_FLOAT_EQ(a.z(), b.z());
  EXPECT_FLOAT_EQ(a.w(), b.w());
}

}  // namespace

TEST(QuatTest, DefaultConstruction) {
  CompareQuaternions(Quaternion(0, 0, 0, 1), Quaternion());
}

TEST(QuatTest, AxisAngleCommon) {
  double radians = 0.5;
  Quaternion q(Vector3dF(1, 0, 0), radians);
  CompareQuaternions(
      Quaternion(std::sin(radians / 2), 0, 0, std::cos(radians / 2)), q);
}

TEST(QuatTest, AxisAngleWithZeroLengthAxis) {
  Quaternion q(Vector3dF(0, 0, 0), 0.5);
  // If the axis of zero length, we should assume the default values.
  CompareQuaternions(q, Quaternion());
}

TEST(QuatTest, Addition) {
  double values[] = {0, 1, 100};
  for (size_t i = 0; i < arraysize(values); ++i) {
    float t = values[i];
    Quaternion a(t, 2 * t, 3 * t, 4 * t);
    Quaternion b(5 * t, 4 * t, 3 * t, 2 * t);
    Quaternion sum = a + b;
    CompareQuaternions(Quaternion(t, t, t, t) * 6, sum);
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
    CompareQuaternions(cases[i].expected, product);
  }
}

TEST(QuatTest, Scaling) {
  double values[] = {0, 0, 100};
  for (size_t i = 0; i < arraysize(values); ++i) {
    double s = values[i];
    Quaternion q(1, 2, 3, 4);
    Quaternion qs = q * s;
    Quaternion sq = s * q;
    Quaternion expected(s, 2 * s, 3 * s, 4 * s);
    CompareQuaternions(expected, qs);
    CompareQuaternions(expected, sq);
  }
}

TEST(QuatTest, Lerp) {
  for (size_t i = 1; i < 100; ++i) {
    Quaternion a(0, 0, 0, 0);
    Quaternion b(1, 2, 3, 4);
    float t = static_cast<float>(i) / 100.0f;
    Quaternion interpolated = a.Lerp(b, t);
    double s = 1.0 / sqrt(30.0);
    CompareQuaternions(Quaternion(1, 2, 3, 4) * s, interpolated);
  }

  Quaternion a(4, 3, 2, 1);
  Quaternion b(1, 2, 3, 4);
  CompareQuaternions(a.normalized(), a.Lerp(b, 0));
  CompareQuaternions(b.normalized(), a.Lerp(b, 1));
  CompareQuaternions(Quaternion(1, 1, 1, 1).normalized(), a.Lerp(b, 0.5));
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
    CompareQuaternions(expected, interpolated);
  }
}

}  // namespace gfx
