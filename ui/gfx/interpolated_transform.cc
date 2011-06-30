// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/interpolated_transform.h"

#include "base/logging.h"
#include "ui/base/animation/tween.h"

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

InterpolatedScale::InterpolatedScale(float start_scale,
                                     float end_scale)
    : InterpolatedTransform(),
      start_scale_(start_scale),
      end_scale_(end_scale) {
}

InterpolatedScale::InterpolatedScale(float start_scale,
                                     float end_scale,
                                     float start_time,
                                     float end_time)
    : InterpolatedTransform(start_time, end_time),
      start_scale_(start_scale),
      end_scale_(end_scale) {
}

InterpolatedScale::~InterpolatedScale() {}

ui::Transform InterpolatedScale::InterpolateButDoNotCompose(float t) const {
  ui::Transform result;
  float interpolated_scale = ValueBetween(t, start_scale_, end_scale_);
  // TODO(vollick) 3d xforms.
  result.SetScale(interpolated_scale, interpolated_scale);
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
  return ui::Transform();
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

} // namespace ui
