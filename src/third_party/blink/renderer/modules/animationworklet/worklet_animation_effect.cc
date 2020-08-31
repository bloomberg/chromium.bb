// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_effect.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"

namespace blink {

WorkletAnimationEffect::WorkletAnimationEffect(
    base::Optional<base::TimeDelta> local_time,
    const Timing& specified_timing)
    : local_time_(local_time),
      specified_timing_(specified_timing),
      calculated_() {
  specified_timing_.AssertValid();
}

EffectTiming* WorkletAnimationEffect::getTiming() const {
  return specified_timing_.ConvertToEffectTiming();
}

ComputedEffectTiming* WorkletAnimationEffect::getComputedTiming() const {
  base::Optional<double> local_time;
  if (local_time_)
    local_time = base::Optional<double>(local_time_.value().InSecondsF());

  bool needs_update = last_update_time_ != local_time;
  last_update_time_ = local_time;

  if (needs_update) {
    // The playback rate is needed to calculate whether the effect is current or
    // not (https://drafts.csswg.org/web-animations-1/#current). Since we only
    // use this information to create a ComputedEffectTiming, which does not
    // include that information, we do not need to supply one.
    base::Optional<double> playback_rate = base::nullopt;
    calculated_ = specified_timing_.CalculateTimings(
        local_time, Timing::AnimationDirection::kForwards, false,
        playback_rate);
  }

  return specified_timing_.getComputedTiming(calculated_,
                                             /*is_keyframe_effect*/ false);
}

base::Optional<double> WorkletAnimationEffect::localTime() const {
  if (!local_time_)
    return base::nullopt;
  return local_time_.value().InMillisecondsF();
}

void WorkletAnimationEffect::setLocalTime(base::Optional<double> time_ms) {
  if (!time_ms) {
    local_time_.reset();
    return;
  }
  DCHECK(!std::isnan(time_ms.value()));
  // Convert double to base::TimeDelta because cc/animation expects
  // base::TimeDelta.
  //
  // Note on precision loss: base::TimeDelta has microseconds precision which is
  // also the precision recommended by the web animation specification as well
  // [1]. If the input time value has a bigger precision then the conversion
  // causes precision loss. Doing the conversion here ensures that reading the
  // value back provides the actual value we use in further computation which
  // is the least surprising path.
  // [1] https://drafts.csswg.org/web-animations/#precision-of-time-values
  local_time_ = base::TimeDelta::FromMillisecondsD(time_ms.value());
}

base::Optional<base::TimeDelta> WorkletAnimationEffect::local_time() const {
  return local_time_;
}

}  // namespace blink
