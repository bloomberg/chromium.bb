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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMING_CALCULATIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMING_CALCULATIONS_H_

#include "third_party/blink/renderer/core/animation/animation_effect.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/platform/animation/animation_utilities.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static inline double MultiplyZeroAlwaysGivesZero(double x, double y) {
  DCHECK(!IsNull(x));
  DCHECK(!IsNull(y));
  return x && y ? x * y : 0;
}

static inline double MultiplyZeroAlwaysGivesZero(AnimationTimeDelta x,
                                                 double y) {
  DCHECK(!IsNull(y));
  return x.is_zero() || y == 0 ? 0 : (x * y).InSecondsF();
}

static inline bool IsWithinEpsilon(double a, double b) {
  // Permit 2-bits of quantization error. Threshold based on experimentation
  // with accuracy of fmod.
  return std::abs(a - b) <= 2.0 * std::numeric_limits<double>::epsilon();
}

// https://drafts.csswg.org/web-animations-1/#animation-effect-phases-and-states
static inline AnimationEffect::Phase CalculatePhase(
    double active_duration,
    double local_time,
    AnimationEffect::AnimationDirection direction,
    const Timing& specified) {
  DCHECK_GE(active_duration, 0);
  if (IsNull(local_time))
    return AnimationEffect::kPhaseNone;
  double end_time = std::max(
      specified.start_delay + active_duration + specified.end_delay, 0.0);
  double before_active_boundary_time =
      std::max(std::min(specified.start_delay, end_time), 0.0);
  if (local_time < before_active_boundary_time ||
      (local_time == before_active_boundary_time &&
       direction == AnimationEffect::AnimationDirection::kBackwards)) {
    return AnimationEffect::kPhaseBefore;
  }
  double active_after_boundary_time = std::max(
      std::min(specified.start_delay + active_duration, end_time), 0.0);
  if (local_time > active_after_boundary_time ||
      (local_time == active_after_boundary_time &&
       direction == AnimationEffect::AnimationDirection::kForwards)) {
    return AnimationEffect::kPhaseAfter;
  }
  return AnimationEffect::kPhaseActive;
}

static inline double CalculateActiveTime(double active_duration,
                                         Timing::FillMode fill_mode,
                                         double local_time,
                                         AnimationEffect::Phase phase,
                                         const Timing& specified) {
  DCHECK_GE(active_duration, 0);

  switch (phase) {
    case AnimationEffect::kPhaseBefore:
      if (fill_mode == Timing::FillMode::BACKWARDS ||
          fill_mode == Timing::FillMode::BOTH)
        return std::max(local_time - specified.start_delay, 0.0);
      return NullValue();
    case AnimationEffect::kPhaseActive:
      return local_time - specified.start_delay;
    case AnimationEffect::kPhaseAfter:
      if (fill_mode == Timing::FillMode::FORWARDS ||
          fill_mode == Timing::FillMode::BOTH) {
        return std::max(
            0.0, std::min(active_duration, local_time - specified.start_delay));
      }
      return NullValue();
    case AnimationEffect::kPhaseNone:
      DCHECK(IsNull(local_time));
      return NullValue();
    default:
      NOTREACHED();
      return NullValue();
  }
}

static inline double CalculateOffsetActiveTime(double active_duration,
                                               double active_time,
                                               double start_offset) {
  DCHECK_GE(active_duration, 0);
  DCHECK_GE(start_offset, 0);

  if (IsNull(active_time))
    return NullValue();

  DCHECK(active_time >= 0 && active_time <= active_duration);

  if (!std::isfinite(active_time))
    return std::numeric_limits<double>::infinity();

  return active_time + start_offset;
}

static inline bool EndsOnIterationBoundary(double iteration_count,
                                           double iteration_start) {
  DCHECK(std::isfinite(iteration_count));
  return !fmod(iteration_count + iteration_start, 1);
}

// TODO(crbug.com/630915): Align this function with current Web Animations spec
// text.
static inline double CalculateIterationTime(double iteration_duration,
                                            double repeated_duration,
                                            double offset_active_time,
                                            double start_offset,
                                            AnimationEffect::Phase phase,
                                            const Timing& specified) {
  DCHECK_GT(iteration_duration, 0);
  DCHECK_EQ(repeated_duration,
            MultiplyZeroAlwaysGivesZero(iteration_duration,
                                        specified.iteration_count));

  if (IsNull(offset_active_time))
    return NullValue();

  DCHECK_GE(offset_active_time, 0);
  DCHECK_LE(offset_active_time, repeated_duration + start_offset);

  if (!std::isfinite(offset_active_time) ||
      (offset_active_time - start_offset == repeated_duration &&
       specified.iteration_count &&
       EndsOnIterationBoundary(specified.iteration_count,
                               specified.iteration_start)))
    return iteration_duration;

  DCHECK(std::isfinite(offset_active_time));
  double iteration_time = fmod(offset_active_time, iteration_duration);

  // This implements step 3 of
  // https://drafts.csswg.org/web-animations/#calculating-the-simple-iteration-progress
  if (iteration_time == 0 && phase == AnimationEffect::kPhaseAfter &&
      repeated_duration != 0 && offset_active_time != 0)
    return iteration_duration;

  return iteration_time;
}

// Calculates the overall progress, which describes the number of iterations
// that have completed (including partial iterations).
// https://drafts.csswg.org/web-animations/#calculating-the-overall-progress
static inline double CalculateOverallProgress(AnimationEffect::Phase phase,
                                              double active_time,
                                              double iteration_duration,
                                              double iteration_count,
                                              double iteration_start) {
  // 1. If the active time is unresolved, return unresolved.
  if (IsNull(active_time))
    return NullValue();

  // 2. Calculate an initial value for overall progress.
  double overall_progress = 0;
  if (!iteration_duration) {
    if (phase != AnimationEffect::kPhaseBefore)
      overall_progress = iteration_count;
  } else {
    overall_progress = active_time / iteration_duration;
  }

  return overall_progress + iteration_start;
}

// Calculates the simple iteration progress, which is a fraction of the progress
// through the current iteration that ignores transformations to the time
// introduced by the playback direction or timing functions applied to the
// effect.
// https://drafts.csswg.org/web-animations/#calculating-the-simple-iteration
// -progress
static inline double CalculateSimpleIterationProgress(
    AnimationEffect::Phase phase,
    double overall_progress,
    double iteration_start,
    double active_time,
    double iteration_duration,
    double iteration_count) {
  // 1. If the overall progress is unresolved, return unresolved.
  if (IsNull(overall_progress))
    return NullValue();

  // 2. If overall progress is infinity, let the simple iteration progress be
  // iteration start % 1.0, otherwise, let the simple iteration progress be
  // overall progress % 1.0.
  double simple_iteration_progress = std::isinf(overall_progress)
                                         ? fmod(iteration_start, 1.0)
                                         : fmod(overall_progress, 1.0);

  const double active_duration = iteration_duration * iteration_count;

  // 3. If all of the following conditions are true,
  //   * the simple iteration progress calculated above is zero, and
  //   * the animation effect is in the active phase or the after phase, and
  //   * the active time is equal to the active duration, and
  //   * the iteration count is not equal to zero.
  // let the simple iteration progress be 1.0.
  if (IsWithinEpsilon(simple_iteration_progress, 0.0) &&
      (phase == AnimationEffect::kPhaseActive ||
       phase == AnimationEffect::kPhaseAfter) &&
      IsWithinEpsilon(active_time, active_duration) &&
      !IsWithinEpsilon(iteration_count, 0.0)) {
    simple_iteration_progress = 1.0;
  }

  // 4. Return simple iteration progress.
  return simple_iteration_progress;
}

// https://drafts.csswg.org/web-animations/#calculating-the-current-iteration
static inline double CalculateCurrentIteration(AnimationEffect::Phase phase,
                                               double active_time,
                                               double iteration_duration,
                                               double iteration_count,
                                               double iteration_start) {
  // 1. If the active time is unresolved, return unresolved.
  if (IsNull(active_time))
    return NullValue();

  // 2. If the animation effect is in the after phase and the iteration count
  // is infinity, return infinity.
  if (phase == AnimationEffect::kPhaseAfter && std::isinf(iteration_count)) {
    return std::numeric_limits<double>::infinity();
  }

  const double overall_progress = CalculateOverallProgress(
      phase, active_time, iteration_duration, iteration_count, iteration_start);

  // 3. If the simple iteration progress is 1.0, return floor(overall progress)
  // - 1.
  const double simple_iteration_progress = CalculateSimpleIterationProgress(
      phase, overall_progress, iteration_start, active_time, iteration_duration,
      iteration_count);

  if (simple_iteration_progress == 1.0)
    return floor(overall_progress) - 1;

  // 4. Otherwise, return floor(overall progress).
  return floor(overall_progress);
}

static inline double CalculateDirectedTime(double current_iteration,
                                           double iteration_duration,
                                           double iteration_time,
                                           const Timing& specified) {
  DCHECK(IsNull(current_iteration) || current_iteration >= 0);
  DCHECK_GT(iteration_duration, 0);

  if (IsNull(iteration_time))
    return NullValue();

  if (IsNull(current_iteration))
    return NullValue();

  DCHECK_GE(current_iteration, 0);
  DCHECK_GE(iteration_time, 0);
  DCHECK_LE(iteration_time, iteration_duration);

  const bool current_iteration_is_odd = fmod(current_iteration, 2) >= 1;
  const bool current_direction_is_forwards =
      specified.direction == Timing::PlaybackDirection::NORMAL ||
      (specified.direction == Timing::PlaybackDirection::ALTERNATE_NORMAL &&
       !current_iteration_is_odd) ||
      (specified.direction == Timing::PlaybackDirection::ALTERNATE_REVERSE &&
       current_iteration_is_odd);

  return current_direction_is_forwards ? iteration_time
                                       : iteration_duration - iteration_time;
}

static inline base::Optional<double> CalculateTransformedTime(
    double current_iteration,
    double iteration_duration,
    double iteration_time,
    const Timing& specified) {
  DCHECK(IsNull(current_iteration) || current_iteration >= 0);
  DCHECK_GT(iteration_duration, 0);
  DCHECK(IsNull(iteration_time) ||
         (iteration_time >= 0 && iteration_time <= iteration_duration));

  double directed_time = CalculateDirectedTime(
      current_iteration, iteration_duration, iteration_time, specified);
  if (IsNull(directed_time))
    return base::nullopt;
  if (!std::isfinite(iteration_duration))
    return directed_time;
  double time_fraction = directed_time / iteration_duration;
  DCHECK(time_fraction >= 0 && time_fraction <= 1);
  return MultiplyZeroAlwaysGivesZero(
      iteration_duration,
      specified.timing_function->Evaluate(
          time_fraction, AccuracyForDuration(iteration_duration)));
}

}  // namespace blink

#endif
