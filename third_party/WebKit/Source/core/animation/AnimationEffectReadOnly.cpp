/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/animation/AnimationEffectReadOnly.h"

#include "core/animation/Animation.h"
#include "core/animation/AnimationEffectTimingReadOnly.h"
#include "core/animation/ComputedTimingProperties.h"
#include "core/animation/TimingCalculations.h"

namespace blink {

namespace {

Timing::FillMode ResolvedFillMode(Timing::FillMode fill_mode,
                                  bool is_animation) {
  if (fill_mode != Timing::FillMode::AUTO)
    return fill_mode;
  if (is_animation)
    return Timing::FillMode::NONE;
  return Timing::FillMode::BOTH;
}

}  // namespace

AnimationEffectReadOnly::AnimationEffectReadOnly(const Timing& timing,
                                                 EventDelegate* event_delegate)
    : animation_(nullptr),
      timing_(timing),
      event_delegate_(event_delegate),
      calculated_(),
      needs_update_(true),
      last_update_time_(NullValue()) {
  timing_.AssertValid();
}

double AnimationEffectReadOnly::IterationDuration() const {
  double result = std::isnan(timing_.iteration_duration)
                      ? IntrinsicIterationDuration()
                      : timing_.iteration_duration;
  DCHECK_GE(result, 0);
  return result;
}

double AnimationEffectReadOnly::RepeatedDuration() const {
  const double result =
      MultiplyZeroAlwaysGivesZero(IterationDuration(), timing_.iteration_count);
  DCHECK_GE(result, 0);
  return result;
}

double AnimationEffectReadOnly::ActiveDurationInternal() const {
  const double result =
      timing_.playback_rate
          ? RepeatedDuration() / std::abs(timing_.playback_rate)
          : std::numeric_limits<double>::infinity();
  DCHECK_GE(result, 0);
  return result;
}

void AnimationEffectReadOnly::UpdateSpecifiedTiming(const Timing& timing) {
  // FIXME: Test whether the timing is actually different?
  timing_ = timing;
  Invalidate();
  if (animation_)
    animation_->SetOutdated();
  SpecifiedTimingChanged();
}

void AnimationEffectReadOnly::getComputedTiming(
    ComputedTimingProperties& computed_timing) {
  // ComputedTimingProperties members.
  computed_timing.setEndTime(EndTimeInternal() * 1000);
  computed_timing.setActiveDuration(ActiveDurationInternal() * 1000);

  if (EnsureCalculated().is_in_effect) {
    computed_timing.setLocalTime(EnsureCalculated().local_time * 1000);
    computed_timing.setProgress(EnsureCalculated().progress);
    computed_timing.setCurrentIteration(EnsureCalculated().current_iteration);
  } else {
    computed_timing.setLocalTimeToNull();
    computed_timing.setProgressToNull();
    computed_timing.setCurrentIterationToNull();
  }

  // KeyframeEffectOptions members.
  computed_timing.setDelay(SpecifiedTiming().start_delay * 1000);
  computed_timing.setEndDelay(SpecifiedTiming().end_delay * 1000);
  computed_timing.setFill(Timing::FillModeString(ResolvedFillMode(
      SpecifiedTiming().fill_mode, IsKeyframeEffectReadOnly())));
  computed_timing.setIterationStart(SpecifiedTiming().iteration_start);
  computed_timing.setIterations(SpecifiedTiming().iteration_count);

  UnrestrictedDoubleOrString duration;
  duration.SetUnrestrictedDouble(IterationDuration() * 1000);
  computed_timing.setDuration(duration);

  computed_timing.setDirection(
      Timing::PlaybackDirectionString(SpecifiedTiming().direction));
  computed_timing.setEasing(SpecifiedTiming().timing_function->ToString());
}

ComputedTimingProperties AnimationEffectReadOnly::getComputedTiming() {
  ComputedTimingProperties result;
  getComputedTiming(result);
  return result;
}

void AnimationEffectReadOnly::UpdateInheritedTime(
    double inherited_time,
    TimingUpdateReason reason) const {
  bool needs_update =
      needs_update_ ||
      (last_update_time_ != inherited_time &&
       !(IsNull(last_update_time_) && IsNull(inherited_time))) ||
      (GetAnimation() && GetAnimation()->EffectSuppressed());
  needs_update_ = false;
  last_update_time_ = inherited_time;

  const double local_time = inherited_time;
  double time_to_next_iteration = std::numeric_limits<double>::infinity();
  if (needs_update) {
    const double active_duration = this->ActiveDurationInternal();

    const Phase current_phase =
        CalculatePhase(active_duration, local_time, timing_);
    // FIXME: parentPhase depends on groups being implemented.
    const AnimationEffectReadOnly::Phase kParentPhase =
        AnimationEffectReadOnly::kPhaseActive;
    const double active_time = CalculateActiveTime(
        active_duration,
        ResolvedFillMode(timing_.fill_mode, IsKeyframeEffectReadOnly()),
        local_time, kParentPhase, current_phase, timing_);

    double current_iteration;
    double progress;
    if (const double iteration_duration = this->IterationDuration()) {
      const double start_offset = MultiplyZeroAlwaysGivesZero(
          timing_.iteration_start, iteration_duration);
      DCHECK_GE(start_offset, 0);
      const double scaled_active_time = CalculateScaledActiveTime(
          active_duration, active_time, start_offset, timing_);
      const double iteration_time = CalculateIterationTime(
          iteration_duration, RepeatedDuration(), scaled_active_time,
          start_offset, current_phase, timing_);

      current_iteration = CalculateCurrentIteration(
          iteration_duration, iteration_time, scaled_active_time, timing_);
      const double transformed_time = CalculateTransformedTime(
          current_iteration, iteration_duration, iteration_time, timing_);

      // The infinite iterationDuration case here is a workaround because
      // the specified behaviour does not handle infinite durations well.
      // There is an open issue against the spec to fix this:
      // https://github.com/w3c/web-animations/issues/142
      if (!std::isfinite(iteration_duration))
        progress = fmod(timing_.iteration_start, 1.0);
      else
        progress = transformed_time / iteration_duration;

      if (!IsNull(iteration_time)) {
        time_to_next_iteration = (iteration_duration - iteration_time) /
                                 std::abs(timing_.playback_rate);
        if (active_duration - active_time < time_to_next_iteration)
          time_to_next_iteration = std::numeric_limits<double>::infinity();
      }
    } else {
      const double kLocalIterationDuration = 1;
      const double local_repeated_duration =
          kLocalIterationDuration * timing_.iteration_count;
      DCHECK_GE(local_repeated_duration, 0);
      const double local_active_duration =
          timing_.playback_rate
              ? local_repeated_duration / std::abs(timing_.playback_rate)
              : std::numeric_limits<double>::infinity();
      DCHECK_GE(local_active_duration, 0);
      const double local_local_time =
          local_time < timing_.start_delay
              ? local_time
              : local_active_duration + timing_.start_delay;
      const AnimationEffectReadOnly::Phase local_current_phase =
          CalculatePhase(local_active_duration, local_local_time, timing_);
      const double local_active_time = CalculateActiveTime(
          local_active_duration,
          ResolvedFillMode(timing_.fill_mode, IsKeyframeEffectReadOnly()),
          local_local_time, kParentPhase, local_current_phase, timing_);
      const double start_offset =
          timing_.iteration_start * kLocalIterationDuration;
      DCHECK_GE(start_offset, 0);
      const double scaled_active_time = CalculateScaledActiveTime(
          local_active_duration, local_active_time, start_offset, timing_);
      const double iteration_time = CalculateIterationTime(
          kLocalIterationDuration, local_repeated_duration, scaled_active_time,
          start_offset, current_phase, timing_);

      current_iteration = CalculateCurrentIteration(
          kLocalIterationDuration, iteration_time, scaled_active_time, timing_);
      progress = CalculateTransformedTime(
          current_iteration, kLocalIterationDuration, iteration_time, timing_);
    }

    calculated_.current_iteration = current_iteration;
    calculated_.progress = progress;

    calculated_.phase = current_phase;
    calculated_.is_in_effect = !IsNull(active_time);
    calculated_.is_in_play = GetPhase() == kPhaseActive;
    calculated_.is_current = GetPhase() == kPhaseBefore || IsInPlay();
    calculated_.local_time = last_update_time_;
  }

  // Test for events even if timing didn't need an update as the animation may
  // have gained a start time.
  // FIXME: Refactor so that we can DCHECK(m_animation) here, this is currently
  // required to be nullable for testing.
  if (reason == kTimingUpdateForAnimationFrame &&
      (!animation_ || animation_->HasStartTime() || animation_->Paused())) {
    if (event_delegate_)
      event_delegate_->OnEventCondition(*this);
  }

  if (needs_update) {
    // FIXME: This probably shouldn't be recursive.
    UpdateChildrenAndEffects();
    calculated_.time_to_forwards_effect_change =
        CalculateTimeToEffectChange(true, local_time, time_to_next_iteration);
    calculated_.time_to_reverse_effect_change =
        CalculateTimeToEffectChange(false, local_time, time_to_next_iteration);
  }
}

const AnimationEffectReadOnly::CalculatedTiming&
AnimationEffectReadOnly::EnsureCalculated() const {
  if (!animation_)
    return calculated_;
  if (animation_->Outdated())
    animation_->Update(kTimingUpdateOnDemand);
  DCHECK(!animation_->Outdated());
  return calculated_;
}

AnimationEffectTimingReadOnly* AnimationEffectReadOnly::timing() {
  return AnimationEffectTimingReadOnly::Create(this);
}

void AnimationEffectReadOnly::Trace(blink::Visitor* visitor) {
  visitor->Trace(animation_);
  visitor->Trace(event_delegate_);
}

}  // namespace blink
