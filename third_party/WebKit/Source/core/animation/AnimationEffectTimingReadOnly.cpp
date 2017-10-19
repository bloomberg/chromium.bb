// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationEffectTimingReadOnly.h"

#include "bindings/core/v8/unrestricted_double_or_string.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/KeyframeEffect.h"
#include "platform/animation/TimingFunction.h"

namespace blink {

AnimationEffectTimingReadOnly* AnimationEffectTimingReadOnly::Create(
    AnimationEffectReadOnly* parent) {
  return new AnimationEffectTimingReadOnly(parent);
}

AnimationEffectTimingReadOnly::AnimationEffectTimingReadOnly(
    AnimationEffectReadOnly* parent)
    : parent_(parent) {}

double AnimationEffectTimingReadOnly::delay() {
  return parent_->SpecifiedTiming().start_delay * 1000;
}

double AnimationEffectTimingReadOnly::endDelay() {
  return parent_->SpecifiedTiming().end_delay * 1000;
}

String AnimationEffectTimingReadOnly::fill() {
  return Timing::FillModeString(parent_->SpecifiedTiming().fill_mode);
}

double AnimationEffectTimingReadOnly::iterationStart() {
  return parent_->SpecifiedTiming().iteration_start;
}

double AnimationEffectTimingReadOnly::iterations() {
  return parent_->SpecifiedTiming().iteration_count;
}

void AnimationEffectTimingReadOnly::duration(
    UnrestrictedDoubleOrString& return_value) {
  if (std::isnan(parent_->SpecifiedTiming().iteration_duration)) {
    return_value.SetString("auto");
  } else {
    return_value.SetUnrestrictedDouble(
        parent_->SpecifiedTiming().iteration_duration * 1000);
  }
}

double AnimationEffectTimingReadOnly::PlaybackRate() {
  return parent_->SpecifiedTiming().playback_rate;
}

String AnimationEffectTimingReadOnly::direction() {
  return Timing::PlaybackDirectionString(parent_->SpecifiedTiming().direction);
}

String AnimationEffectTimingReadOnly::easing() {
  return parent_->SpecifiedTiming().timing_function->ToString();
}

void AnimationEffectTimingReadOnly::Trace(blink::Visitor* visitor) {
  visitor->Trace(parent_);
}

}  // namespace blink
