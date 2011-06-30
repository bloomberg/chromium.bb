// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/interpolated_transform.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const static float EPSILON = 1e-6f;

bool ApproximatelyEqual(float lhs, float rhs) {
  if (lhs == 0)
    return fabs(rhs) < EPSILON;
  if (rhs == 0)
    return fabs(lhs) < EPSILON;
  return fabs(lhs - rhs) / std::max(fabs(rhs), fabs(lhs)) < EPSILON;
}

bool ApproximatelyEqual(const ui::Transform& lhs, const ui::Transform& rhs) {
  for (int i = 0; i < 9; ++i) {
    if (!ApproximatelyEqual(lhs.matrix()[i], rhs.matrix()[i]))
      return false;
  }
  return true;
}

} // namespace

TEST(InterpolatedTransformTest, InterpolatedRotation) {
  ui::InterpolatedRotation interpolated_rotation(0, 100);
  ui::InterpolatedRotation interpolated_rotation_diff_start_end(
      0, 100, 100, 200);

  for (int i = 0; i <= 100; ++i) {
    ui::Transform rotation;
    rotation.SetRotate(i);
    ui::Transform interpolated = interpolated_rotation.Interpolate(i / 100.0f);
    EXPECT_TRUE(ApproximatelyEqual(rotation, interpolated));
    interpolated = interpolated_rotation_diff_start_end.Interpolate(i + 100);
    EXPECT_TRUE(ApproximatelyEqual(rotation, interpolated));
  }
}

TEST(InterpolatedTransformTest, InterpolatedScale) {
  ui::InterpolatedScale interpolated_scale(0, 100);
  ui::InterpolatedScale interpolated_scale_diff_start_end(
      0, 100, 100, 200);

  for (int i = 0; i <= 100; ++i) {
    ui::Transform scale;
    scale.SetScale(i, i);
    ui::Transform interpolated = interpolated_scale.Interpolate(i / 100.0f);
    EXPECT_TRUE(ApproximatelyEqual(scale, interpolated));
    interpolated = interpolated_scale_diff_start_end.Interpolate(i + 100);
    EXPECT_TRUE(ApproximatelyEqual(scale, interpolated));
  }
}

TEST(InterpolatedTransformTest, InterpolatedTranslate) {
  ui::InterpolatedTranslation interpolated_xform(gfx::Point(0, 0),
                                                 gfx::Point(100, 100));

  ui::InterpolatedTranslation interpolated_xform_diff_start_end(
      gfx::Point(0, 0), gfx::Point(100, 100), 100, 200);

  for (int i = 0; i <= 100; ++i) {
    ui::Transform xform;
    xform.SetTranslate(i, i);
    ui::Transform interpolated = interpolated_xform.Interpolate(i / 100.0f);
    EXPECT_TRUE(ApproximatelyEqual(xform, interpolated));
    interpolated = interpolated_xform_diff_start_end.Interpolate(i + 100);
    EXPECT_TRUE(ApproximatelyEqual(xform, interpolated));
  }
}

TEST(InterpolatedTransformTest, InterpolatedRotationAboutPivot) {
  gfx::Point pivot(100, 100);
  gfx::Point above_pivot(100, 200);
  ui::InterpolatedRotation rot(0, 90);
  ui::InterpolatedTransformAboutPivot interpolated_xform(
      pivot,
      new ui::InterpolatedRotation(0, 90));
  ui::Transform result = interpolated_xform.Interpolate(0.0f);
  EXPECT_TRUE(ApproximatelyEqual(ui::Transform(), result));
  result = interpolated_xform.Interpolate(1.0f);
  gfx::Point expected_result = pivot;
  EXPECT_TRUE(result.TransformPoint(&pivot));
  EXPECT_EQ(expected_result, pivot);
  expected_result = gfx::Point(0, 100);
  EXPECT_TRUE(result.TransformPoint(&above_pivot));
  EXPECT_EQ(expected_result, above_pivot);
}

TEST(InterpolatedTransformTest, InterpolatedScaleAboutPivot) {
  gfx::Point pivot(100, 100);
  gfx::Point above_pivot(100, 200);
  ui::InterpolatedTransformAboutPivot interpolated_xform(
      pivot,
      new ui::InterpolatedScale(1, 2));
  ui::Transform result = interpolated_xform.Interpolate(0.0f);
  EXPECT_TRUE(ApproximatelyEqual(ui::Transform(), result));
  result = interpolated_xform.Interpolate(1.0f);
  gfx::Point expected_result = pivot;
  EXPECT_TRUE(result.TransformPoint(&pivot));
  EXPECT_EQ(expected_result, pivot);
  expected_result = gfx::Point(100, 300);
  EXPECT_TRUE(result.TransformPoint(&above_pivot));
  EXPECT_EQ(expected_result, above_pivot);
}
