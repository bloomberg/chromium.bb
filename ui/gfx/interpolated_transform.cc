// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/interpolated_transform.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "base/logging.h"
#include "ui/base/animation/tween.h"

namespace {

static const float EPSILON = 1e-6f;

} // namespace

namespace ui {

///////////////////////////////////////////////////////////////////////////////
// InterpolatedTransform
//

InterpolatedTransform::InterpolatedTransform()
    : start_time_(0.0f),
      end_time_(1.0f) {
}

InterpolatedTransform::InterpolatedTransform(float start_time,
                                             float end_time)
    : start_time_(start_time),
      end_time_(end_time) {
}

InterpolatedTransform::~InterpolatedTransform() {}

ui::Transform InterpolatedTransform::Interpolate(float t) const {
  ui::Transform result = InterpolateButDoNotCompose(t);
  if (child_.get()) {
    result.ConcatTransform(child_->Interpolate(t));
  }
  return result;
}

void InterpolatedTransform::SetChild(InterpolatedTransform* child) {
  child_.reset(child);
}

bool InterpolatedTransform::FactorTRS(const ui::Transform& transform,
                                      gfx::Point* translation,
                                      float* rotation,
                                      gfx::Point3f* scale) {
  const SkMatrix44& m = transform.matrix();
  float m00 = m.get(0, 0);
  float m01 = m.get(0, 1);
  float m10 = m.get(1, 0);
  float m11 = m.get(1, 1);

  // A factorable 2D TRS matrix must be of the form:
  //    [ sx*cos_theta -(sy*sin_theta) 0 tx ]
  //    [ sx*sin_theta   sy*cos_theta  0 ty ]
  //    [ 0              0             1 0  ]
  //    [ 0              0             0 1  ]
  if (m.get(0, 2) != 0 ||
      m.get(1, 2) != 0 ||
      m.get(2, 0) != 0 ||
      m.get(2, 1) != 0 ||
      m.get(2, 2) != 1 ||
      m.get(2, 3) != 0 ||
      m.get(3, 0) != 0 ||
      m.get(3, 1) != 0 ||
      m.get(3, 2) != 0 ||
      m.get(3, 3) != 1) {
    return false;
  }

  float scale_x = sqrt(m00 * m00 + m10 * m10);
  float scale_y = sqrt(m01 * m01 + m11 * m11);

  if (scale_x == 0 || scale_y == 0)
    return false;

  float cos_theta = m00 / scale_x;
  float sin_theta = m10 / scale_x;

  if ((fabs(cos_theta - (m11 / scale_y))) > EPSILON ||
      (fabs(sin_theta + (m01 / scale_y))) > EPSILON ||
      (fabs(cos_theta*cos_theta + sin_theta*sin_theta - 1.0f) > EPSILON)) {
    return false;
  }

  float radians = atan2(sin_theta, cos_theta);

  if (translation)
    *translation = gfx::Point(m.get(0, 3), m.get(1, 3));
  if (rotation)
    *rotation = radians * 180 / M_PI;
  if (scale)
    *scale = gfx::Point3f(scale_x, scale_y, 1.0f);

  return true;
}

inline float InterpolatedTransform::ValueBetween(float time,
                                                 float start_value,
                                                 float end_value) const {
  // can't handle NaN
  DCHECK(time == time && start_time_ == start_time_ && end_time_ == end_time_);
  if (time != time || start_time_ != start_time_ || end_time_ != end_time_)
    return start_value;

  // Ok if equal -- we'll get a step function. Note: if end_time_ ==
  // start_time_ == x, then if none of the numbers are NaN, then it
  // must be true that time < x or time >= x, so we will return early
  // due to one of the following if statements.
  DCHECK(end_time_ >= start_time_);

  if (time < start_time_)
    return start_value;

  if (time >= end_time_)
    return end_value;

  float t = (time - start_time_) / (end_time_ - start_time_);
  return static_cast<float>(Tween::ValueBetween(t, start_value, end_value));
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedRotation
//

InterpolatedRotation::InterpolatedRotation(float start_degrees,
                                           float end_degrees)
    : InterpolatedTransform(),
      start_degrees_(start_degrees),
      end_degrees_(end_degrees) {
}

InterpolatedRotation::InterpolatedRotation(float start_degrees,
                                           float end_degrees,
                                           float start_time,
                                           float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_degrees_(start_degrees),
      end_degrees_(end_degrees) {
}

InterpolatedRotation::~InterpolatedRotation() {}

ui::Transform InterpolatedRotation::InterpolateButDoNotCompose(float t) const {
  ui::Transform result;
  result.SetRotate(ValueBetween(t, start_degrees_, end_degrees_));
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedScale
//

InterpolatedScale::InterpolatedScale(float start_scale, float end_scale)
    : InterpolatedTransform(),
      start_scale_(gfx::Point3f(start_scale, start_scale, start_scale)),
      end_scale_(gfx::Point3f(end_scale, end_scale, end_scale)) {
}

InterpolatedScale::InterpolatedScale(float start_scale, float end_scale,
                                     float start_time, float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_scale_(gfx::Point3f(start_scale, start_scale, start_scale)),
      end_scale_(gfx::Point3f(end_scale, end_scale, end_scale)) {
}

InterpolatedScale::InterpolatedScale(const gfx::Point3f& start_scale,
                                     const gfx::Point3f& end_scale)
    : InterpolatedTransform(),
      start_scale_(start_scale),
      end_scale_(end_scale) {
}

InterpolatedScale::InterpolatedScale(const gfx::Point3f& start_scale,
                                     const gfx::Point3f& end_scale,
                                     float start_time,
                                     float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_scale_(start_scale),
      end_scale_(end_scale) {
}

InterpolatedScale::~InterpolatedScale() {}

ui::Transform InterpolatedScale::InterpolateButDoNotCompose(float t) const {
  ui::Transform result;
  float scale_x = ValueBetween(t, start_scale_.x(), end_scale_.x());
  float scale_y = ValueBetween(t, start_scale_.y(), end_scale_.y());
  // TODO(vollick) 3d xforms.
  result.SetScale(scale_x, scale_y);
  return result;
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedTranslation
//

InterpolatedTranslation::InterpolatedTranslation(const gfx::Point& start_pos,
                                                 const gfx::Point& end_pos)
    : InterpolatedTransform(),
      start_pos_(start_pos),
      end_pos_(end_pos) {
}

InterpolatedTranslation::InterpolatedTranslation(const gfx::Point& start_pos,
                                                 const gfx::Point& end_pos,
                                                 float start_time,
                                                 float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_pos_(start_pos),
      end_pos_(end_pos) {
}

InterpolatedTranslation::~InterpolatedTranslation() {}

ui::Transform
InterpolatedTranslation::InterpolateButDoNotCompose(float t) const {
  ui::Transform result;
  // TODO(vollick) 3d xforms.
  result.SetTranslate(ValueBetween(t, start_pos_.x(), end_pos_.x()),
                      ValueBetween(t, start_pos_.y(), end_pos_.y()));

  return result;
}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedConstantTransform
//

InterpolatedConstantTransform::InterpolatedConstantTransform(
  const ui::Transform& transform)
    : InterpolatedTransform(),
  transform_(transform) {
}

ui::Transform
InterpolatedConstantTransform::InterpolateButDoNotCompose(float t) const {
  return transform_;
}

InterpolatedConstantTransform::~InterpolatedConstantTransform() {}

///////////////////////////////////////////////////////////////////////////////
// InterpolatedTransformAboutPivot
//

InterpolatedTransformAboutPivot::InterpolatedTransformAboutPivot(
  const gfx::Point& pivot,
  InterpolatedTransform* transform)
    : InterpolatedTransform() {
  Init(pivot, transform);
}

InterpolatedTransformAboutPivot::InterpolatedTransformAboutPivot(
  const gfx::Point& pivot,
  InterpolatedTransform* transform,
  float start_time,
  float end_time)
    : InterpolatedTransform() {
  Init(pivot, transform);
}

InterpolatedTransformAboutPivot::~InterpolatedTransformAboutPivot() {}

ui::Transform
InterpolatedTransformAboutPivot::InterpolateButDoNotCompose(float t) const {
  if (transform_.get()) {
    return transform_->Interpolate(t);
  }
  return Transform();
}

void InterpolatedTransformAboutPivot::Init(const gfx::Point& pivot,
                                           InterpolatedTransform* xform) {
  ui::Transform to_pivot;
  ui::Transform from_pivot;
  to_pivot.SetTranslate(-pivot.x(), -pivot.y());
  from_pivot.SetTranslate(pivot.x(), pivot.y());

  scoped_ptr<InterpolatedTransform> pre_transform(
    new InterpolatedConstantTransform(to_pivot));
  scoped_ptr<InterpolatedTransform> post_transform(
    new InterpolatedConstantTransform(from_pivot));

  pre_transform->SetChild(xform);
  xform->SetChild(post_transform.release());
  transform_.reset(pre_transform.release());
}

InterpolatedTRSTransform::InterpolatedTRSTransform(
    const ui::Transform& start_transform,
    const ui::Transform& end_transform)
    : InterpolatedTransform() {
  Init(start_transform, end_transform);
}

InterpolatedTRSTransform::InterpolatedTRSTransform(
    const ui::Transform& start_transform,
    const ui::Transform& end_transform,
    float start_time,
    float end_time)
    : InterpolatedTransform() {
  Init(start_transform, end_transform);
}

InterpolatedTRSTransform::~InterpolatedTRSTransform() {}

ui::Transform
InterpolatedTRSTransform::InterpolateButDoNotCompose(float t) const {
  if (transform_.get()) {
    return transform_->Interpolate(t);
  }
  return Transform();
}

void InterpolatedTRSTransform::Init(const Transform& start_transform,
                                    const Transform& end_transform) {
  gfx::Point start_translation, end_translation;
  gfx::Point3f start_scale, end_scale;
  float start_degrees, end_degrees;
  if (FactorTRS(start_transform,
                &start_translation,
                &start_degrees,
                &start_scale) &&
      FactorTRS(end_transform,
                &end_translation,
                &end_degrees,
                &end_scale)) {
    scoped_ptr<InterpolatedTranslation> translation(
        new InterpolatedTranslation(start_translation, end_translation,
                                    start_time(), end_time()));

    scoped_ptr<InterpolatedScale> scale(
        new InterpolatedScale(start_scale, end_scale,
                              start_time(), end_time()));

    scoped_ptr<InterpolatedRotation> rotation(
        new InterpolatedRotation(start_degrees, end_degrees,
                                 start_time(), end_time()));

    rotation->SetChild(translation.release());
    scale->SetChild(rotation.release());
    transform_.reset(scale.release());
  } else {
    transform_.reset(new InterpolatedConstantTransform(end_transform));
  }
}

} // namespace ui
