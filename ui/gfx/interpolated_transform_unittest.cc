// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/interpolated_transform.h"

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CheckApproximatelyEqual(const ui::Transform& lhs,
                             const ui::Transform& rhs) {
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_FLOAT_EQ(lhs.matrix().get(i, j), rhs.matrix().get(i, j));
    }
  }
}

float NormalizeAngle(float angle) {
  while (angle < 0.0f) {
    angle += 360.0f;
  }
  while (angle > 360.0f) {
    angle -= 360.0f;
  }
  return angle;
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
    CheckApproximatelyEqual(rotation, interpolated);
    interpolated = interpolated_rotation_diff_start_end.Interpolate(i + 100);
    CheckApproximatelyEqual(rotation, interpolated);
  }
}

TEST(InterpolatedTransformTest, InterpolatedScale) {
  ui::InterpolatedScale interpolated_scale(gfx::Point3f(0, 0, 0),
                                           gfx::Point3f(100, 100, 100));
  ui::InterpolatedScale interpolated_scale_diff_start_end(
      gfx::Point3f(0, 0, 0), gfx::Point3f(100, 100, 100), 100, 200);

  for (int i = 0; i <= 100; ++i) {
    ui::Transform scale;
    scale.SetScale(i, i);
    ui::Transform interpolated = interpolated_scale.Interpolate(i / 100.0f);
    CheckApproximatelyEqual(scale, interpolated);
    interpolated = interpolated_scale_diff_start_end.Interpolate(i + 100);
    CheckApproximatelyEqual(scale, interpolated);
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
    CheckApproximatelyEqual(xform, interpolated);
    interpolated = interpolated_xform_diff_start_end.Interpolate(i + 100);
    CheckApproximatelyEqual(xform, interpolated);
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
  CheckApproximatelyEqual(ui::Transform(), result);
  result = interpolated_xform.Interpolate(1.0f);
  gfx::Point expected_result = pivot;
  result.TransformPoint(pivot);
  EXPECT_EQ(expected_result, pivot);
  expected_result = gfx::Point(0, 100);
  result.TransformPoint(above_pivot);
  EXPECT_EQ(expected_result, above_pivot);
}

TEST(InterpolatedTransformTest, InterpolatedScaleAboutPivot) {
  gfx::Point pivot(100, 100);
  gfx::Point above_pivot(100, 200);
  ui::InterpolatedTransformAboutPivot interpolated_xform(
      pivot,
      new ui::InterpolatedScale(gfx::Point3f(1, 1, 1), gfx::Point3f(2, 2, 2)));
  ui::Transform result = interpolated_xform.Interpolate(0.0f);
  CheckApproximatelyEqual(ui::Transform(), result);
  result = interpolated_xform.Interpolate(1.0f);
  gfx::Point expected_result = pivot;
  result.TransformPoint(pivot);
  EXPECT_EQ(expected_result, pivot);
  expected_result = gfx::Point(100, 300);
  result.TransformPoint(above_pivot);
  EXPECT_EQ(expected_result, above_pivot);
}

TEST(InterpolatedTransformTest, FactorTRS) {
  for (int degrees = 0; degrees < 360; ++degrees) {
    // build a transformation matrix.
    ui::Transform transform;
    transform.SetScale(degrees + 1, 2 * degrees + 1);
    transform.ConcatRotate(degrees);
    transform.ConcatTranslate(degrees * 2, -degrees * 3);

    // factor the matrix
    gfx::Point translation;
    float rotation;
    gfx::Point3f scale;
    bool success = ui::InterpolatedTransform::FactorTRS(transform,
                                                        &translation,
                                                        &rotation,
                                                        &scale);
    EXPECT_TRUE(success);
    EXPECT_FLOAT_EQ(translation.x(), degrees * 2);
    EXPECT_FLOAT_EQ(translation.y(), -degrees * 3);
    EXPECT_FLOAT_EQ(NormalizeAngle(rotation), degrees);
    EXPECT_FLOAT_EQ(scale.x(), degrees + 1);
    EXPECT_FLOAT_EQ(scale.y(), 2 * degrees + 1);
  }
}
