// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/transform.h"

#include "ui/gfx/point.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/safe_integer_conversions.h"
#include "ui/gfx/skia_util.h"

namespace gfx {

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

void Transform::SetRotate(float degree) {
  matrix_.setRotateDegreesAbout(0, 0, 1, SkFloatToScalar(degree));
}

void Transform::SetRotateAbout(const Point3F& axis, float degree) {
  matrix_.setRotateDegreesAbout(axis.x(),
                                axis.y(),
                                axis.z(),
                                SkFloatToScalar(degree));
}

void Transform::SetScaleX(float x) {
  matrix_.set(0, 0, SkFloatToScalar(x));
}

void Transform::SetScaleY(float y) {
  matrix_.set(1, 1, SkFloatToScalar(y));
}

void Transform::SetScale(float x, float y) {
  matrix_.setScale(SkFloatToScalar(x),
                   SkFloatToScalar(y),
                   matrix_.get(2, 2));
}

void Transform::SetTranslateX(float x) {
  matrix_.set(0, 3, SkFloatToScalar(x));
}

void Transform::SetTranslateY(float y) {
  matrix_.set(1, 3, SkFloatToScalar(y));
}

void Transform::SetTranslate(float x, float y) {
  matrix_.setTranslate(SkFloatToScalar(x),
                       SkFloatToScalar(y),
                       matrix_.get(2, 3));
}

void Transform::SetPerspectiveDepth(float depth) {
  SkMatrix44 m;
  m.set(3, 2, -1 / depth);
  matrix_ = m;
}

void Transform::ConcatRotate(float degree) {
  SkMatrix44 rot;
  rot.setRotateDegreesAbout(0, 0, 1, SkFloatToScalar(degree));
  matrix_.postConcat(rot);
}

void Transform::ConcatRotateAbout(const Point3F& axis, float degree) {
  SkMatrix44 rot;
  rot.setRotateDegreesAbout(axis.x(),
                            axis.y(),
                            axis.z(),
                            SkFloatToScalar(degree));
  matrix_.postConcat(rot);
}

void Transform::ConcatScale(float x, float y) {
  SkMatrix44 scale;
  scale.setScale(SkFloatToScalar(x), SkFloatToScalar(y), 1);
  matrix_.postConcat(scale);
}

void Transform::ConcatTranslate(float x, float y) {
  SkMatrix44 translate;
  translate.setTranslate(SkFloatToScalar(x), SkFloatToScalar(y), 0);
  matrix_.postConcat(translate);
}

void Transform::ConcatPerspectiveDepth(float depth) {
  SkMatrix44 m;
  m.set(3, 2, -1 / depth);
  matrix_.postConcat(m);
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

bool Transform::HasChange() const {
  return !matrix_.isIdentity();
}

bool Transform::GetInverse(Transform* transform) const {
  return matrix_.invert(&transform->matrix_);
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

void Transform::TransformPointInternal(const SkMatrix44& xform,
                                       Point3F& point) const {
  SkScalar p[4] = {
    SkFloatToScalar(point.x()),
    SkFloatToScalar(point.y()),
    SkFloatToScalar(point.z()),
    1 };

  xform.map(p);

  if (p[3] != 1 && abs(p[3]) > 0) {
    point.SetPoint(p[0] / p[3], p[1] / p[3], p[2]/ p[3]);
  } else {
    point.SetPoint(p[0], p[1], p[2]);
  }
}

void Transform::TransformPointInternal(const SkMatrix44& xform,
                                       Point& point) const {
  SkScalar p[4] = { SkIntToScalar(point.x()),  SkIntToScalar(point.y()),
                    0, 1 };

  xform.map(p);

  point.SetPoint(ToRoundedInt(p[0]), ToRoundedInt(p[1]));
}

}  // namespace gfx
