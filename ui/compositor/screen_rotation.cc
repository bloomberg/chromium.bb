// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/screen_rotation.h"

#include "base/debug/trace_event.h"
#include "base/time.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

const int k90DegreeTransitionDurationMs = 350;
const int k180DegreeTransitionDurationMs = 550;
const int k360DegreeTransitionDurationMs = 750;

base::TimeDelta GetTransitionDuration(int degrees) {
  if (degrees == 360)
    return base::TimeDelta::FromMilliseconds(k360DegreeTransitionDurationMs);
  if (degrees == 180)
    return base::TimeDelta::FromMilliseconds(k180DegreeTransitionDurationMs);
  if (degrees == 0)
    return base::TimeDelta::FromMilliseconds(0);
  return base::TimeDelta::FromMilliseconds(k90DegreeTransitionDurationMs);
}

}  // namespace

ScreenRotation::ScreenRotation(int degrees)
    : LayerAnimationElement(GetProperties(), GetTransitionDuration(degrees)),
      degrees_(degrees) {
}

ScreenRotation::~ScreenRotation() {
}

void ScreenRotation::OnStart(LayerAnimationDelegate* delegate) {
  // No rotation required.
  if (degrees_ == 0)
    return;

  const Transform& current_transform = delegate->GetTransformForAnimation();
  const gfx::Rect& bounds = delegate->GetBoundsForAnimation();

  gfx::Point old_pivot;
  gfx::Point new_pivot;

  int width = bounds.width();
  int height = bounds.height();

  switch (degrees_) {
    case 90:
      new_origin_ = new_pivot = gfx::Point(width, 0);
      break;
    case -90:
      new_origin_ = new_pivot = gfx::Point(0, height);
      break;
    case 180:
    case 360:
      new_pivot = old_pivot = gfx::Point(width / 2, height / 2);
      new_origin_.SetPoint(width, height);
      break;
  }

  // Convert points to world space.
  current_transform.TransformPoint(old_pivot);
  current_transform.TransformPoint(new_pivot);
  current_transform.TransformPoint(new_origin_);

  scoped_ptr<InterpolatedTransform> rotation(
      new InterpolatedTransformAboutPivot(
          old_pivot,
          new InterpolatedRotation(0, degrees_)));

  scoped_ptr<InterpolatedTransform> translation(
      new InterpolatedTranslation(
          gfx::Point(0, 0),
          gfx::Point(new_pivot.x() - old_pivot.x(),
                     new_pivot.y() - old_pivot.y())));

  float scale_factor = 0.9f;
  scoped_ptr<InterpolatedTransform> scale_down(
      new InterpolatedScale(1.0f, scale_factor, 0.0f, 0.5f));

  scoped_ptr<InterpolatedTransform> scale_up(
      new InterpolatedScale(1.0f, 1.0f / scale_factor, 0.5f, 1.0f));

  interpolated_transform_.reset(
      new InterpolatedConstantTransform(current_transform));

  scale_up->SetChild(scale_down.release());
  translation->SetChild(scale_up.release());
  rotation->SetChild(translation.release());
  interpolated_transform_->SetChild(rotation.release());
}

bool ScreenRotation::OnProgress(double t,
                                LayerAnimationDelegate* delegate) {
  delegate->SetTransformFromAnimation(interpolated_transform_->Interpolate(t));
  return true;
}

void ScreenRotation::OnGetTarget(TargetValue* target) const {
  target->transform = interpolated_transform_->Interpolate(1.0);
}

void ScreenRotation::OnAbort() {
}

// static
const LayerAnimationElement::AnimatableProperties&
ScreenRotation::GetProperties() {
  static LayerAnimationElement::AnimatableProperties properties;
  if (properties.empty())
    properties.insert(LayerAnimationElement::TRANSFORM);
  return properties;
}

}  // namespace ui
