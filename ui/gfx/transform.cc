// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES

#include "ui/gfx/transform.h"

#include <cmath>

#include "ui/gfx/point.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/vector3d_f.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform_util.h"

namespace gfx {

namespace {

// Taken from SkMatrix44.
const double kTooSmallForDeterminant = 1e-8;

double TanDegrees(double degrees) {
  double radians = degrees * M_PI / 180;
  return std::tan(radians);
}

}  // namespace

Transform::Transform() {
}

Transform::~Transform() {}

bool Transform::operator==(const Transform& rhs) const {
  return matrix_ == rhs.matrix_;
}

bool Transform::operator!=(const Transform& rhs) const {
  return !(*this == rhs);
}

void Transform::MakeIdentity() {
  matrix_.setIdentity();
}

void Transform::RotateAboutXAxis(double degrees) {
  double radians = degrees * M_PI / 180;
  double cosTheta = std::cos(radians);
  double sinTheta = std::sin(radians);
  if (matrix_.isIdentity()) {
      matrix_.set3x3(1, 0, 0,
                     0, cosTheta, sinTheta,
                     0, -sinTheta, cosTheta);
  } else {
    SkMatrix44 rot;
    rot.set3x3(1, 0, 0,
               0, cosTheta, sinTheta,
               0, -sinTheta, cosTheta);
    matrix_.preConcat(rot);
  }
}

void Transform::RotateAboutYAxis(double degrees) {
  double radians = degrees * M_PI / 180;
  double cosTheta = std::cos(radians);
  double sinTheta = std::sin(radians);
  if (matrix_.isIdentity()) {
      // Note carefully the placement of the -sinTheta for rotation about
      // y-axis is different than rotation about x-axis or z-axis.
      matrix_.set3x3(cosTheta, 0, -sinTheta,
                     0, 1, 0,
                     sinTheta, 0, cosTheta);
  } else {
    SkMatrix44 rot;
    rot.set3x3(cosTheta, 0, -sinTheta,
               0, 1, 0,
               sinTheta, 0, cosTheta);
    matrix_.preConcat(rot);
  }
}

void Transform::RotateAboutZAxis(double degrees) {
  double radians = degrees * M_PI / 180;
  double cosTheta = std::cos(radians);
  double sinTheta = std::sin(radians);
  if (matrix_.isIdentity()) {
      matrix_.set3x3(cosTheta, sinTheta, 0,
                     -sinTheta, cosTheta, 0,
                     0, 0, 1);
  } else {
    SkMatrix44 rot;
    rot.set3x3(cosTheta, sinTheta, 0,
               -sinTheta, cosTheta, 0,
               0, 0, 1);
    matrix_.preConcat(rot);
  }
}

void Transform::RotateAbout(const Vector3dF& axis, double degrees) {
  if (matrix_.isIdentity()) {
    matrix_.setRotateDegreesAbout(SkDoubleToMScalar(axis.x()),
                                  SkDoubleToMScalar(axis.y()),
                                  SkDoubleToMScalar(axis.z()),
                                  SkDoubleToMScalar(degrees));
  } else {
    SkMatrix44 rot;
    rot.setRotateDegreesAbout(SkDoubleToMScalar(axis.x()),
                              SkDoubleToMScalar(axis.y()),
                              SkDoubleToMScalar(axis.z()),
                              SkDoubleToMScalar(degrees));
    matrix_.preConcat(rot);
  }
}

void Transform::Scale(double x, double y) {
  if (matrix_.isIdentity()) {
    matrix_.setScale(SkDoubleToMScalar(x),
                     SkDoubleToMScalar(y),
                     SkDoubleToMScalar(1));
  } else {
    SkMatrix44 scale;
    scale.setScale(SkDoubleToMScalar(x),
                   SkDoubleToMScalar(y),
                   SkDoubleToMScalar(1));
    matrix_.preConcat(scale);
  }
}

void Transform::Scale3d(double x, double y, double z) {
  if (matrix_.isIdentity()) {
    matrix_.setScale(SkDoubleToMScalar(x),
                     SkDoubleToMScalar(y),
                     SkDoubleToMScalar(z));
  } else {
    SkMatrix44 scale;
    scale.setScale(SkDoubleToMScalar(x),
                   SkDoubleToMScalar(y),
                   SkDoubleToMScalar(z));
    matrix_.preConcat(scale);
  }
}

void Transform::Translate(double x, double y) {
  if (matrix_.isIdentity()) {
    matrix_.setTranslate(SkDoubleToMScalar(x),
                         SkDoubleToMScalar(y),
                         SkDoubleToMScalar(0));
  } else {
    SkMatrix44 translate;
    translate.setTranslate(SkDoubleToMScalar(x),
                           SkDoubleToMScalar(y),
                           SkDoubleToMScalar(0));
    matrix_.preConcat(translate);
  }
}

void Transform::Translate3d(double x, double y, double z) {
  if (matrix_.isIdentity()) {
    matrix_.setTranslate(SkDoubleToMScalar(x),
                         SkDoubleToMScalar(y),
                         SkDoubleToMScalar(z));
  } else {
    SkMatrix44 translate;
    translate.setTranslate(SkDoubleToMScalar(x),
                           SkDoubleToMScalar(y),
                           SkDoubleToMScalar(z));
    matrix_.preConcat(translate);
  }
}

void Transform::SkewX(double angle_x) {
  if (matrix_.isIdentity())
    matrix_.setDouble(0, 1, TanDegrees(angle_x));
  else {
    SkMatrix44 skew;
    skew.setDouble(0, 1, TanDegrees(angle_x));
    matrix_.preConcat(skew);
  }
}

void Transform::SkewY(double angle_y) {
  if (matrix_.isIdentity())
    matrix_.setDouble(1, 0, TanDegrees(angle_y));
  else {
    SkMatrix44 skew;
    skew.setDouble(1, 0, TanDegrees(angle_y));
    matrix_.preConcat(skew);
  }
}

void Transform::ApplyPerspectiveDepth(double depth) {
  if (depth == 0)
    return;
  if (matrix_.isIdentity())
    matrix_.setDouble(3, 2, -1.0 / depth);
  else {
    SkMatrix44 m;
    m.setDouble(3, 2, -1.0 / depth);
    matrix_.preConcat(m);
  }
}

void Transform::PreconcatTransform(const Transform& transform) {
  if (!transform.matrix_.isIdentity()) {
    matrix_.preConcat(transform.matrix_);
  }
}

void Transform::ConcatTransform(const Transform& transform) {
  if (!transform.matrix_.isIdentity()) {
    matrix_.postConcat(transform.matrix_);
  }
}

bool Transform::IsIdentity() const {
  return matrix_.isIdentity();
}

bool Transform::IsIdentityOrTranslation() const {
  bool has_no_perspective = !matrix_.getDouble(3, 0) &&
                            !matrix_.getDouble(3, 1) &&
                            !matrix_.getDouble(3, 2) &&
                            (matrix_.getDouble(3, 3) == 1);

  bool has_no_rotation_or_skew = !matrix_.getDouble(0, 1) &&
                                 !matrix_.getDouble(0, 2) &&
                                 !matrix_.getDouble(1, 0) &&
                                 !matrix_.getDouble(1, 2) &&
                                 !matrix_.getDouble(2, 0) &&
                                 !matrix_.getDouble(2, 1);

  bool has_no_scale = matrix_.getDouble(0, 0) == 1 &&
                      matrix_.getDouble(1, 1) == 1 &&
                      matrix_.getDouble(2, 2) == 1;

  return has_no_perspective && has_no_rotation_or_skew && has_no_scale;
}

bool Transform::IsScaleOrTranslation() const {
  bool has_no_perspective = !matrix_.getDouble(3, 0) &&
                            !matrix_.getDouble(3, 1) &&
                            !matrix_.getDouble(3, 2) &&
                            (matrix_.getDouble(3, 3) == 1);

  bool has_no_rotation_or_skew = !matrix_.getDouble(0, 1) &&
                                 !matrix_.getDouble(0, 2) &&
                                 !matrix_.getDouble(1, 0) &&
                                 !matrix_.getDouble(1, 2) &&
                                 !matrix_.getDouble(2, 0) &&
                                 !matrix_.getDouble(2, 1);

  return has_no_perspective && has_no_rotation_or_skew;
}

bool Transform::HasPerspective() const {
  return matrix_.getDouble(3, 0) ||
         matrix_.getDouble(3, 1) ||
         matrix_.getDouble(3, 2) ||
         (matrix_.getDouble(3, 3) != 1);
}

bool Transform::IsInvertible() const {
  return std::abs(matrix_.determinant()) > kTooSmallForDeterminant;
}

bool Transform::IsBackFaceVisible() const {
  // Compute whether a layer with a forward-facing normal of (0, 0, 1) would
  // have its back face visible after applying the transform.
  //
  // This is done by transforming the normal and seeing if the resulting z
  // value is positive or negative. However, note that transforming a normal
  // actually requires using the inverse-transpose of the original transform.

  // TODO (shawnsingh) make this perform more efficiently - we do not
  // actually need to instantiate/invert/transpose any matrices, exploiting the
  // fact that we only need to transform (0, 0, 1, 0).
  SkMatrix44 inverse;
  bool invertible = matrix_.invert(&inverse);

  // Assume the transform does not apply if it's not invertible, so it's
  // front face remains visible.
  if (!invertible)
    return false;

  return inverse.getDouble(2, 2) < 0;
}

bool Transform::GetInverse(Transform* transform) const {
  return matrix_.invert(&transform->matrix_);
}

void Transform::Transpose() {
  matrix_.transpose();
}

void Transform::TransformPoint(Point& point) const {
  TransformPointInternal(matrix_, point);
}

void Transform::TransformPoint(Point3F& point) const {
  TransformPointInternal(matrix_, point);
}

bool Transform::TransformPointReverse(Point& point) const {
  // TODO(sad): Try to avoid trying to invert the matrix.
  SkMatrix44 inverse;
  if (!matrix_.invert(&inverse))
    return false;

  TransformPointInternal(inverse, point);
  return true;
}

bool Transform::TransformPointReverse(Point3F& point) const {
  // TODO(sad): Try to avoid trying to invert the matrix.
  SkMatrix44 inverse;
  if (!matrix_.invert(&inverse))
    return false;

  TransformPointInternal(inverse, point);
  return true;
}

void Transform::TransformRect(RectF* rect) const {
  SkRect src = RectFToSkRect(*rect);
  const SkMatrix& matrix = matrix_;
  matrix.mapRect(&src);
  *rect = SkRectToRectF(src);
}

bool Transform::TransformRectReverse(RectF* rect) const {
  SkMatrix44 inverse;
  if (!matrix_.invert(&inverse))
    return false;
  const SkMatrix& matrix = inverse;
  SkRect src = RectFToSkRect(*rect);
  matrix.mapRect(&src);
  *rect = SkRectToRectF(src);
  return true;
}

bool Transform::Blend(const Transform& from, double progress) {
  if (progress <= 0.0) {
    *this = from;
    return true;
  }

  if (progress >= 1.0)
    return true;

  DecomposedTransform to_decomp;
  DecomposedTransform from_decomp;
  if (!DecomposeTransform(&to_decomp, *this) ||
      !DecomposeTransform(&from_decomp, from))
    return false;

  if (!BlendDecomposedTransforms(&to_decomp, to_decomp, from_decomp, progress))
    return false;

  matrix_ = ComposeTransform(to_decomp).matrix();
  return true;
}

Transform Transform::operator*(const Transform& other) const {
  Transform to_return;
  to_return.matrix_.setConcat(matrix_, other.matrix_);
  return to_return;
}

Transform& Transform::operator*=(const Transform& other) {
  matrix_.preConcat(other.matrix_);
  return *this;
}

void Transform::TransformPointInternal(const SkMatrix44& xform,
                                       Point3F& point) const {
  SkMScalar p[4] = {
    SkDoubleToMScalar(point.x()),
    SkDoubleToMScalar(point.y()),
    SkDoubleToMScalar(point.z()),
    SkDoubleToMScalar(1)
  };

  xform.mapMScalars(p);

  if (p[3] != 1 && abs(p[3]) > 0) {
    point.SetPoint(p[0] / p[3], p[1] / p[3], p[2]/ p[3]);
  } else {
    point.SetPoint(p[0], p[1], p[2]);
  }
}

void Transform::TransformPointInternal(const SkMatrix44& xform,
                                       Point& point) const {
  SkMScalar p[4] = {
    SkDoubleToMScalar(point.x()),
    SkDoubleToMScalar(point.y()),
    SkDoubleToMScalar(0),
    SkDoubleToMScalar(1)
  };

  xform.mapMScalars(p);

  point.SetPoint(ToRoundedInt(p[0]), ToRoundedInt(p[1]));
}

}  // namespace gfx
