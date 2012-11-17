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
  matrix_.reset();
}

Transform::~Transform() {}

bool Transform::operator==(const Transform& rhs) const {
  return matrix_ == rhs.matrix_;
}

bool Transform::operator!=(const Transform& rhs) const {
  return !(*this == rhs);
}

void Transform::SetRotate(double degree) {
  matrix_.setRotateDegreesAbout(SkDoubleToMScalar(0),
                                SkDoubleToMScalar(0),
                                SkDoubleToMScalar(1),
                                SkDoubleToMScalar(degree));
}

void Transform::SetRotateAbout(const Point3F& axis, double degree) {
  matrix_.setRotateDegreesAbout(SkDoubleToMScalar(axis.x()),
                                SkDoubleToMScalar(axis.y()),
                                SkDoubleToMScalar(axis.z()),
                                SkDoubleToMScalar(degree));
}

void Transform::SetScaleX(double x) {
  matrix_.setDouble(0, 0, x);
}

void Transform::SetScaleY(double y) {
  matrix_.setDouble(1, 1, y);
}

void Transform::SetScaleZ(double z) {
  matrix_.setDouble(2, 2, z);
}

void Transform::SetScale(double x, double y) {
  matrix_.setScale(SkDoubleToMScalar(x),
                   SkDoubleToMScalar(y),
                   matrix_.get(2, 2));
}

void Transform::SetScale3d(double x, double y, double z) {
  matrix_.setScale(SkDoubleToMScalar(x),
                   SkDoubleToMScalar(y),
                   SkDoubleToMScalar(z));
}

void Transform::SetTranslateX(double x) {
  matrix_.setDouble(0, 3, x);
}

void Transform::SetTranslateY(double y) {
  matrix_.setDouble(1, 3, y);
}

void Transform::SetTranslateZ(double z) {
  matrix_.setDouble(2, 3, z);
}

void Transform::SetTranslate(double x, double y) {
  matrix_.setTranslate(SkDoubleToMScalar(x),
                       SkDoubleToMScalar(y),
                       matrix_.get(2, 3));
}

void Transform::SetTranslate3d(double x, double y, double z) {
  matrix_.setTranslate(SkDoubleToMScalar(x),
                       SkDoubleToMScalar(y),
                       SkDoubleToMScalar(z));
}

void Transform::SetSkewX(double angle) {
  matrix_.setDouble(0, 1, TanDegrees(angle));
}

void Transform::SetSkewY(double angle) {
  matrix_.setDouble(1, 0, TanDegrees(angle));
}

void Transform::SetPerspectiveDepth(double depth) {
  if (depth == 0)
      return;

  SkMatrix44 m;
  m.setDouble(3, 2, -1.0 / depth);
  matrix_ = m;
}

void Transform::ConcatRotate(double degree) {
  SkMatrix44 rot;
  rot.setRotateDegreesAbout(SkDoubleToMScalar(0),
                            SkDoubleToMScalar(0),
                            SkDoubleToMScalar(1),
                            SkDoubleToMScalar(degree));
  matrix_.postConcat(rot);
}

void Transform::ConcatRotateAbout(const Point3F& axis, double degree) {
  SkMatrix44 rot;
  rot.setRotateDegreesAbout(SkDoubleToMScalar(axis.x()),
                            SkDoubleToMScalar(axis.y()),
                            SkDoubleToMScalar(axis.z()),
                            SkDoubleToMScalar(degree));
  matrix_.postConcat(rot);
}

void Transform::ConcatScale(double x, double y) {
  SkMatrix44 scale;
  scale.setScale(SkDoubleToMScalar(x),
                 SkDoubleToMScalar(y),
                 SkDoubleToMScalar(1));
  matrix_.postConcat(scale);
}

void Transform::ConcatScale3d(double x, double y, double z) {
  SkMatrix44 scale;
  scale.setScale(SkDoubleToMScalar(x),
                 SkDoubleToMScalar(y),
                 SkDoubleToMScalar(z));
  matrix_.postConcat(scale);
}

void Transform::ConcatTranslate(double x, double y) {
  SkMatrix44 translate;
  translate.setTranslate(SkDoubleToMScalar(x),
                         SkDoubleToMScalar(y),
                         SkDoubleToMScalar(0));
  matrix_.postConcat(translate);
}

void Transform::ConcatTranslate3d(double x, double y, double z) {
  SkMatrix44 translate;
  translate.setTranslate(SkDoubleToMScalar(x),
                         SkDoubleToMScalar(y),
                         SkDoubleToMScalar(z));
  matrix_.postConcat(translate);
}

void Transform::ConcatSkewX(double angle_x) {
  Transform t;
  t.SetSkewX(angle_x);
  matrix_.postConcat(t.matrix_);
}

void Transform::ConcatSkewY(double angle_y) {
  Transform t;
  t.SetSkewY(angle_y);
  matrix_.postConcat(t.matrix_);
}

void Transform::ConcatPerspectiveDepth(double depth) {
  if (depth == 0)
      return;

  SkMatrix44 m;
  m.setDouble(3, 2, -1.0 / depth);
  matrix_.postConcat(m);
}

void Transform::PreconcatRotate(double degree) {
  SkMatrix44 rot;
  rot.setRotateDegreesAbout(SkDoubleToMScalar(0),
                            SkDoubleToMScalar(0),
                            SkDoubleToMScalar(1),
                            SkDoubleToMScalar(degree));
  matrix_.preConcat(rot);
}

void Transform::PreconcatRotateAbout(const Point3F& axis, double degree) {
  SkMatrix44 rot;
  rot.setRotateDegreesAbout(SkDoubleToMScalar(axis.x()),
                            SkDoubleToMScalar(axis.y()),
                            SkDoubleToMScalar(axis.z()),
                            SkDoubleToMScalar(degree));
  matrix_.preConcat(rot);
}

void Transform::PreconcatScale(double x, double y) {
  SkMatrix44 scale;
  scale.setScale(SkDoubleToMScalar(x),
                 SkDoubleToMScalar(y),
                 SkDoubleToMScalar(1));
  matrix_.preConcat(scale);
}

void Transform::PreconcatScale3d(double x, double y, double z) {
  SkMatrix44 scale;
  scale.setScale(SkDoubleToMScalar(x),
                 SkDoubleToMScalar(y),
                 SkDoubleToMScalar(z));
  matrix_.preConcat(scale);
}

void Transform::PreconcatTranslate(double x, double y) {
  SkMatrix44 translate;
  translate.setTranslate(SkDoubleToMScalar(x),
                         SkDoubleToMScalar(y),
                         SkDoubleToMScalar(0));
  matrix_.preConcat(translate);
}

void Transform::PreconcatTranslate3d(double x, double y, double z) {
  SkMatrix44 translate;
  translate.setTranslate(SkDoubleToMScalar(x),
                         SkDoubleToMScalar(y),
                         SkDoubleToMScalar(z));
  matrix_.preConcat(translate);
}

void Transform::PreconcatSkewX(double angle_x) {
  Transform t;
  t.SetSkewX(angle_x);
  matrix_.preConcat(t.matrix_);
}

void Transform::PreconcatSkewY(double angle_y) {
  Transform t;
  t.SetSkewY(angle_y);
  matrix_.preConcat(t.matrix_);
}

void Transform::PreconcatPerspectiveDepth(double depth) {
  if (depth == 0)
      return;

  SkMatrix44 m;
  m.setDouble(3, 2, -1.0 / depth);
  matrix_.preConcat(m);
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

bool Transform::IsInvertible() const {
  return std::abs(matrix_.determinant()) > kTooSmallForDeterminant;
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
