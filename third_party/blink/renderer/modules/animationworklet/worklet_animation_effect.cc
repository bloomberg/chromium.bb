// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_animation_effect.h"

namespace blink {

WorkletAnimationEffect::WorkletAnimationEffect(
    base::Optional<TimeDelta> local_time,
    const Timing& specified_timing)
    : local_time_(local_time), specified_timing_(specified_timing) {
  specified_timing_.AssertValid();
}

EffectTiming* WorkletAnimationEffect::getTiming() const {
  EffectTiming* effect_timing = EffectTiming::Create();

  // This logic mirrors the blink side logic contained in
  // third_party\blink\renderer\core\animation\animation_effect.cc

  // TODO(jortaylo): Extract this logic to Timing.h aso that it can be
  // shared between blink and animation worklet (https://crbug.com/915344).
  effect_timing->setDelay(specified_timing_.start_delay * 1000);
  effect_timing->setEndDelay(specified_timing_.end_delay * 1000);
  effect_timing->setFill(Timing::FillModeString(specified_timing_.fill_mode));
  effect_timing->setIterationStart(specified_timing_.iteration_start);
  effect_timing->setIterations(specified_timing_.iteration_count);
  UnrestrictedDoubleOrString duration;
  if (specified_timing_.iteration_duration) {
    duration.SetUnrestrictedDouble(
        specified_timing_.iteration_duration->InMillisecondsF());
  } else {
    duration.SetString("auto");
  }
  effect_timing->setDuration(duration);
  effect_timing->setDirection(
      Timing::PlaybackDirectionString(specified_timing_.direction));
  effect_timing->setEasing(specified_timing_.timing_function->ToString());

  return effect_timing;
}

void WorkletAnimationEffect::setLocalTime(double time_ms, bool is_null) {
  if (is_null) {
    local_time_.reset();
    return;
  }
  DCHECK(!std::isnan(time_ms));
  // Convert double to TimeDelta because cc/animation expects TimeDelta.
  //
  // Note on precision loss: TimeDelta has microseconds precision which is
  // also the precision recommended by the web animation specification as well
  // [1]. If the input time value has a bigger precision then the conversion
  // causes precision loss. Doing the conversion here ensures that reading the
  // value back provides the actual value we use in further computation which
  // is the least surprising path.
  // [1] https://drafts.csswg.org/web-animations/#precision-of-time-values
  local_time_ = WTF::TimeDelta::FromMillisecondsD(time_ms);
}

double WorkletAnimationEffect::localTime(bool& is_null) const {
  is_null = !local_time_.has_value();
  return local_time_.value_or(base::TimeDelta()).InMillisecondsF();
}

base::Optional<WTF::TimeDelta> WorkletAnimationEffect::local_time() const {
  return local_time_;
}
}  // namespace blink
