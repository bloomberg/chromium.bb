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

#include "third_party/blink/renderer/core/animation/animation_effect.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_computed_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_optional_effect_timing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string_unrestricteddouble.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/core/animation/timing_input.h"

namespace blink {

AnimationEffect::AnimationEffect(const Timing& timing,
                                 EventDelegate* event_delegate)
    : owner_(nullptr),
      timing_(timing),
      event_delegate_(event_delegate),
      needs_update_(true),
      cancel_time_(AnimationTimeDelta()) {
  timing_.AssertValid();
  InvalidateNormalizedTiming();
}

// Scales all timing values so that end_time == timeline_duration
void AnimationEffect::EnsureNormalizedTiming() const {
  // Only run the normalization process if needed
  if (normalized_)
    return;

  normalized_ = Timing::NormalizedTiming();
  // A valid timeline duration signifies use of a progress based timeline.
  if (TimelineDuration()) {
    // Normalize timings for progress based timelines
    normalized_->timeline_duration = TimelineDuration();

    if (timing_.iteration_duration) {
      // Scaling up iteration_duration allows animation effect to be able to
      // handle values produced by progress based timelines. At this point it
      // can be assumed that EndTimeInternal() will give us a good value.

      const AnimationTimeDelta active_duration = MultiplyZeroAlwaysGivesZero(
          timing_.iteration_duration.value(), timing_.iteration_count);
      DCHECK_GE(active_duration, AnimationTimeDelta());

      // Per the spec, the end time has a lower bound of 0.0:
      // https://drafts.csswg.org/web-animations-1/#end-time
      const AnimationTimeDelta end_time =
          std::max(timing_.start_delay + active_duration + timing_.end_delay,
                   AnimationTimeDelta());

      // Exceptions should have already been thrown when trying to input values
      // that would result in an infinite end_time for progress based timelines.
      DCHECK(!end_time.is_inf());

      // Negative start_delay that is >= iteration_duration or iteration_count
      // of 0 will cause end_time to be 0 or negative.
      if (end_time.is_zero()) {
        // end_time of zero causes division by zero so we handle it here
        normalized_->start_delay = AnimationTimeDelta();
        normalized_->end_delay = AnimationTimeDelta();
        normalized_->iteration_duration = AnimationTimeDelta();
      } else {
        // convert to percentages then multiply by the timeline_duration
        normalized_->start_delay = (timing_.start_delay / end_time) *
                                   normalized_->timeline_duration.value();

        normalized_->end_delay = (timing_.end_delay / end_time) *
                                 normalized_->timeline_duration.value();

        normalized_->iteration_duration =
            (timing_.iteration_duration.value() / end_time) *
            normalized_->timeline_duration.value();
      }
    } else {
      // Handle iteration_duration value of "auto"

      // TODO(crbug.com/1216527)
      // this will change to support percentage delays and possibly mixed
      // delays.
      DCHECK(normalized_->start_delay.is_zero() &&
             normalized_->end_delay.is_zero());

      normalized_->iteration_duration = IntrinsicIterationDuration();

      // TODO: add support for progress based timelines and "auto" duration
      // effects
    }
  } else {
    // Populates normalized values for use with time based timelines.
    normalized_->start_delay = timing_.start_delay;
    normalized_->end_delay = timing_.end_delay;
    normalized_->iteration_duration =
        timing_.iteration_duration.value_or(AnimationTimeDelta());
  }

  normalized_->active_duration = MultiplyZeroAlwaysGivesZero(
      normalized_->iteration_duration, timing_.iteration_count);

  // Per the spec, the end time has a lower bound of 0.0:
  // https://drafts.csswg.org/web-animations-1/#end-time
  normalized_->end_time =
      std::max(normalized_->start_delay + normalized_->active_duration +
                   normalized_->end_delay,
               AnimationTimeDelta());
}

void AnimationEffect::UpdateSpecifiedTiming(const Timing& timing) {
  if (!timing_.HasTimingOverrides()) {
    timing_ = timing;
  } else {
    // Style changes that are overridden due to an explicit call to
    // AnimationEffect.updateTiming are not applied.
    if (!timing_.HasTimingOverride(Timing::kOverrideStartDelay))
      timing_.start_delay = timing.start_delay;

    if (!timing_.HasTimingOverride(Timing::kOverrideDirection))
      timing_.direction = timing.direction;

    if (!timing_.HasTimingOverride(Timing::kOverrideDuration))
      timing_.iteration_duration = timing.iteration_duration;

    if (!timing_.HasTimingOverride(Timing::kOverrideEndDelay))
      timing_.end_delay = timing.end_delay;

    if (!timing_.HasTimingOverride(Timing::kOverideFillMode))
      timing_.fill_mode = timing.fill_mode;

    if (!timing_.HasTimingOverride(Timing::kOverrideIterationCount))
      timing_.iteration_count = timing.iteration_count;

    if (!timing_.HasTimingOverride(Timing::kOverrideIterationStart))
      timing_.iteration_start = timing.iteration_start;

    if (!timing_.HasTimingOverride(Timing::kOverrideTimingFunction))
      timing_.timing_function = timing.timing_function;
  }

  InvalidateNormalizedTiming();
  InvalidateAndNotifyOwner();
}

void AnimationEffect::SetIgnoreCssTimingProperties() {
  timing_.SetTimingOverride(Timing::kOverrideAll);
}

EffectTiming* AnimationEffect::getTiming() const {
  if (const Animation* animation = GetAnimation())
    animation->FlushPendingUpdates();
  return SpecifiedTiming().ConvertToEffectTiming();
}

ComputedEffectTiming* AnimationEffect::getComputedTiming() const {
  return SpecifiedTiming().getComputedTiming(
      EnsureCalculated(), NormalizedTiming(), IsA<KeyframeEffect>(this));
}

void AnimationEffect::updateTiming(OptionalEffectTiming* optional_timing,
                                   ExceptionState& exception_state) {
  if (GetAnimation() && GetAnimation()->timeline() &&
      GetAnimation()->timeline()->IsScrollTimeline()) {
    if (optional_timing->hasDuration()) {
      if (optional_timing->duration()->IsUnrestrictedDouble()) {
        double duration =
            optional_timing->duration()->GetAsUnrestrictedDouble();
        if (duration == std::numeric_limits<double>::infinity()) {
          exception_state.ThrowTypeError(
              "Effect duration cannot be Infinity when used with Scroll "
              "Timelines");
          return;
        }
      } else if (optional_timing->duration()->GetAsString() == "auto") {
        // TODO(crbug.com/1216527)
        // Eventually we hope to be able to be more flexible with
        // iteration_duration "auto" and its interaction with start_delay and
        // end_delay. For now we will throw an exception if either delay is set.
        // Once delays are changed to CSSNumberish, we will need to adjust logic
        // here to allow for percentage values but not time values.

        // If either delay or end_delay are non-zero, we can't handle "auto"
        if (!SpecifiedTiming().start_delay.is_zero() ||
            !SpecifiedTiming().end_delay.is_zero()) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              "Effect duration \"auto\" with delays is not yet implemented "
              "when used with Scroll Timelines");
          return;
        }
      }
    }

    if (optional_timing->hasIterations() &&
        optional_timing->iterations() ==
            std::numeric_limits<double>::infinity()) {
      // iteration count of infinity makes no sense for scroll timelines
      exception_state.ThrowTypeError(
          "Effect iterations cannot be Infinity when used with Scroll "
          "Timelines");
      return;
    }
  }

  // TODO(crbug.com/827178): Determine whether we should pass a Document in here
  // (and which) to resolve the CSS secure/insecure context against.
  if (!TimingInput::Update(timing_, optional_timing, nullptr, exception_state))
    return;

  InvalidateNormalizedTiming();
  InvalidateAndNotifyOwner();
}

absl::optional<Timing::Phase> TimelinePhaseToTimingPhase(
    absl::optional<TimelinePhase> phase) {
  absl::optional<Timing::Phase> result;
  if (phase) {
    switch (phase.value()) {
      case TimelinePhase::kBefore:
        result = Timing::Phase::kPhaseBefore;
        break;
      case TimelinePhase::kActive:
        result = Timing::Phase::kPhaseActive;
        break;
      case TimelinePhase::kAfter:
        result = Timing::Phase::kPhaseAfter;
        break;
      case TimelinePhase::kInactive:
        // Timing::Phase does not have an inactive phase.
        break;
    }
  }
  return result;
}

void AnimationEffect::UpdateInheritedTime(
    absl::optional<AnimationTimeDelta> inherited_time,
    absl::optional<TimelinePhase> inherited_timeline_phase,
    bool at_progress_timeline_boundary,
    double inherited_playback_rate,
    TimingUpdateReason reason) const {
  const Timing::AnimationDirection direction =
      (inherited_playback_rate < 0) ? Timing::AnimationDirection::kBackwards
                                    : Timing::AnimationDirection::kForwards;

  absl::optional<Timing::Phase> timeline_phase =
      TimelinePhaseToTimingPhase(inherited_timeline_phase);

  bool needs_update = needs_update_ || last_update_time_ != inherited_time ||
                      (owner_ && owner_->EffectSuppressed()) ||
                      last_update_phase_ != timeline_phase;
  needs_update_ = false;
  last_update_time_ = inherited_time;
  last_update_phase_ = timeline_phase;

  if (needs_update) {
    Timing::CalculatedTiming calculated = SpecifiedTiming().CalculateTimings(
        inherited_time, timeline_phase, at_progress_timeline_boundary,
        NormalizedTiming(), direction, IsA<KeyframeEffect>(this),
        inherited_playback_rate);

    const bool was_canceled = calculated.phase != calculated_.phase &&
                              calculated.phase == Timing::kPhaseNone;

    // If the animation was canceled, we need to fire the event condition before
    // updating the calculated timing so that the cancellation time can be
    // determined.
    if (was_canceled && event_delegate_) {
      event_delegate_->OnEventCondition(*this, calculated.phase);
    }

    calculated_ = calculated;
  }

  // Test for events even if timing didn't need an update as the animation may
  // have gained a start time.
  // FIXME: Refactor so that we can DCHECK(owner_) here, this is currently
  // required to be nullable for testing.
  if (reason == kTimingUpdateForAnimationFrame &&
      (!owner_ || owner_->IsEventDispatchAllowed())) {
    if (event_delegate_)
      event_delegate_->OnEventCondition(*this, calculated_.phase);
  }

  if (needs_update) {
    // FIXME: This probably shouldn't be recursive.
    UpdateChildrenAndEffects();
    calculated_.time_to_forwards_effect_change = CalculateTimeToEffectChange(
        true, inherited_time, calculated_.time_to_next_iteration);
    calculated_.time_to_reverse_effect_change = CalculateTimeToEffectChange(
        false, inherited_time, calculated_.time_to_next_iteration);
  }
}

void AnimationEffect::InvalidateAndNotifyOwner() const {
  Invalidate();
  if (owner_)
    owner_->EffectInvalidated();
}

const Timing::CalculatedTiming& AnimationEffect::EnsureCalculated() const {
  if (owner_)
    owner_->UpdateIfNecessary();
  return calculated_;
}

Animation* AnimationEffect::GetAnimation() {
  return owner_ ? owner_->GetAnimation() : nullptr;
}
const Animation* AnimationEffect::GetAnimation() const {
  return owner_ ? owner_->GetAnimation() : nullptr;
}

void AnimationEffect::Trace(Visitor* visitor) const {
  visitor->Trace(owner_);
  visitor->Trace(event_delegate_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
