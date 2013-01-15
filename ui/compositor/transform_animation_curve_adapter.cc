// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/transform_animation_curve_adapter.h"

using WebKit::WebTransformationMatrix;

namespace {
// TODO(ajuma): Remove this once the return type of
// cc::TransformAnimationCurve::getValue is changed to gfx::Transform.
WebTransformationMatrix convertTransformToWebTransformationMatrix(
    const gfx::Transform& transform) {
  return WebTransformationMatrix(transform.matrix().getDouble(0, 0),
                                 transform.matrix().getDouble(1, 0),
                                 transform.matrix().getDouble(2, 0),
                                 transform.matrix().getDouble(3, 0),
                                 transform.matrix().getDouble(0, 1),
                                 transform.matrix().getDouble(1, 1),
                                 transform.matrix().getDouble(2, 1),
                                 transform.matrix().getDouble(3, 1),
                                 transform.matrix().getDouble(0, 2),
                                 transform.matrix().getDouble(1, 2),
                                 transform.matrix().getDouble(2, 2),
                                 transform.matrix().getDouble(3, 2),
                                 transform.matrix().getDouble(0, 3),
                                 transform.matrix().getDouble(1, 3),
                                 transform.matrix().getDouble(2, 3),
                                 transform.matrix().getDouble(3, 3));

}
}  // namespace

namespace ui {

TransformAnimationCurveAdapter::TransformAnimationCurveAdapter(
    Tween::Type tween_type,
    gfx::Transform initial_value,
    gfx::Transform target_value,
    base::TimeDelta duration)
    : tween_type_(tween_type),
      initial_value_(initial_value),
      target_value_(target_value),
      duration_(duration) {
  gfx::DecomposeTransform(&decomposed_initial_value_, initial_value_);
  gfx::DecomposeTransform(&decomposed_target_value_, target_value_);
}

TransformAnimationCurveAdapter::~TransformAnimationCurveAdapter() {
}

double TransformAnimationCurveAdapter::duration() const {
  return duration_.InSecondsF();
}

scoped_ptr<cc::AnimationCurve> TransformAnimationCurveAdapter::clone() const {
  scoped_ptr<TransformAnimationCurveAdapter> to_return(
      new TransformAnimationCurveAdapter(tween_type_,
                                         initial_value_,
                                         target_value_,
                                         duration_));
  return to_return.PassAs<cc::AnimationCurve>();
}

WebTransformationMatrix TransformAnimationCurveAdapter::getValue(
    double t) const {
  if (t >= duration_.InSecondsF())
    return convertTransformToWebTransformationMatrix(target_value_);
  if (t <= 0.0)
    return convertTransformToWebTransformationMatrix(initial_value_);
  double progress = t / duration_.InSecondsF();

  gfx::DecomposedTransform to_return;
  gfx::BlendDecomposedTransforms(&to_return,
                                 decomposed_initial_value_,
                                 decomposed_target_value_,
                                 Tween::CalculateValue(tween_type_, progress));
  return convertTransformToWebTransformationMatrix(
      gfx::ComposeTransform(to_return));
}

}  // namespace ui
