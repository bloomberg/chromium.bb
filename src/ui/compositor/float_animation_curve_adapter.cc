// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/float_animation_curve_adapter.h"

#include "base/memory/ptr_util.h"
#include "cc/base/time_util.h"

namespace ui {

FloatAnimationCurveAdapter::FloatAnimationCurveAdapter(
    gfx::Tween::Type tween_type,
    float initial_value,
    float target_value,
    base::TimeDelta duration)
    : tween_type_(tween_type),
      initial_value_(initial_value),
      target_value_(target_value),
      duration_(duration) {
}

base::TimeDelta FloatAnimationCurveAdapter::Duration() const {
  return duration_;
}

std::unique_ptr<cc::AnimationCurve> FloatAnimationCurveAdapter::Clone() const {
  return base::WrapUnique(new FloatAnimationCurveAdapter(
      tween_type_, initial_value_, target_value_, duration_));
}

float FloatAnimationCurveAdapter::GetValue(base::TimeDelta t) const {
  if (t >= duration_)
    return target_value_;
  if (t <= base::TimeDelta())
    return initial_value_;
  double progress = cc::TimeUtil::Divide(t, duration_);
  return gfx::Tween::FloatValueBetween(
      gfx::Tween::CalculateValue(tween_type_, progress),
      initial_value_,
      target_value_);
}

}  // namespace ui
