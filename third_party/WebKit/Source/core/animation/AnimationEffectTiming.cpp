// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationEffectTiming.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/unrestricted_double_or_string.h"
#include "core/animation/AnimationEffectReadOnly.h"
#include "core/animation/AnimationEffectTimingReadOnly.h"
#include "core/animation/KeyframeEffect.h"
#include "core/animation/TimingInput.h"
#include "platform/animation/TimingFunction.h"

namespace blink {

AnimationEffectTiming* AnimationEffectTiming::Create(
    AnimationEffectReadOnly* parent) {
  return new AnimationEffectTiming(parent);
}

AnimationEffectTiming::AnimationEffectTiming(AnimationEffectReadOnly* parent)
    : AnimationEffectTimingReadOnly(parent) {}

void AnimationEffectTiming::setDelay(double delay) {
  Timing timing = parent_->SpecifiedTiming();
  TimingInput::SetStartDelay(timing, delay);
  parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setEndDelay(double end_delay) {
  Timing timing = parent_->SpecifiedTiming();
  TimingInput::SetEndDelay(timing, end_delay);
  parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setFill(String fill) {
  Timing timing = parent_->SpecifiedTiming();
  TimingInput::SetFillMode(timing, fill);
  parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setIterationStart(double iteration_start,
                                              ExceptionState& exception_state) {
  Timing timing = parent_->SpecifiedTiming();
  if (TimingInput::SetIterationStart(timing, iteration_start, exception_state))
    parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setIterations(double iterations,
                                          ExceptionState& exception_state) {
  Timing timing = parent_->SpecifiedTiming();
  if (TimingInput::SetIterationCount(timing, iterations, exception_state))
    parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setDuration(
    const UnrestrictedDoubleOrString& duration,
    ExceptionState& exception_state) {
  Timing timing = parent_->SpecifiedTiming();
  if (TimingInput::SetIterationDuration(timing, duration, exception_state))
    parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::SetPlaybackRate(double playback_rate) {
  Timing timing = parent_->SpecifiedTiming();
  TimingInput::SetPlaybackRate(timing, playback_rate);
  parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setDirection(String direction) {
  Timing timing = parent_->SpecifiedTiming();
  TimingInput::SetPlaybackDirection(timing, direction);
  parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::setEasing(String easing,
                                      ExceptionState& exception_state) {
  Timing timing = parent_->SpecifiedTiming();
  // The AnimationEffectTiming might not be attached to a document at this
  // point, so we pass nullptr in to setTimingFunction. This means that these
  // calls are not considered in the WebAnimationsEasingAsFunction*
  // UseCounters, but the bug we are tracking there does not come through
  // this interface.
  if (TimingInput::SetTimingFunction(timing, easing, nullptr, exception_state))
    parent_->UpdateSpecifiedTiming(timing);
}

void AnimationEffectTiming::Trace(blink::Visitor* visitor) {
  AnimationEffectTimingReadOnly::Trace(visitor);
}

}  // namespace blink
