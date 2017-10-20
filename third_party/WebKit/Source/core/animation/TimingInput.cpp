// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/TimingInput.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/unrestricted_double_or_keyframe_animation_options.h"
#include "bindings/core/v8/unrestricted_double_or_keyframe_effect_options.h"
#include "core/animation/AnimationInputHelpers.h"
#include "core/animation/KeyframeEffectOptions.h"

namespace blink {

void TimingInput::SetStartDelay(Timing& timing, double start_delay) {
  if (std::isfinite(start_delay))
    timing.start_delay = start_delay / 1000;
  else
    timing.start_delay = Timing::Defaults().start_delay;
}

void TimingInput::SetEndDelay(Timing& timing, double end_delay) {
  if (std::isfinite(end_delay))
    timing.end_delay = end_delay / 1000;
  else
    timing.end_delay = Timing::Defaults().end_delay;
}

void TimingInput::SetFillMode(Timing& timing, const String& fill_mode) {
  if (fill_mode == "none") {
    timing.fill_mode = Timing::FillMode::NONE;
  } else if (fill_mode == "backwards") {
    timing.fill_mode = Timing::FillMode::BACKWARDS;
  } else if (fill_mode == "both") {
    timing.fill_mode = Timing::FillMode::BOTH;
  } else if (fill_mode == "forwards") {
    timing.fill_mode = Timing::FillMode::FORWARDS;
  } else {
    timing.fill_mode = Timing::Defaults().fill_mode;
  }
}

bool TimingInput::SetIterationStart(Timing& timing,
                                    double iteration_start,
                                    ExceptionState& exception_state) {
  DCHECK(std::isfinite(iteration_start));
  if (std::isnan(iteration_start) || iteration_start < 0) {
    exception_state.ThrowTypeError("iterationStart must be non-negative.");
    return false;
  }
  timing.iteration_start = iteration_start;
  return true;
}

bool TimingInput::SetIterationCount(Timing& timing,
                                    double iteration_count,
                                    ExceptionState& exception_state) {
  if (std::isnan(iteration_count) || iteration_count < 0) {
    exception_state.ThrowTypeError("iterationCount must be non-negative.");
    return false;
  }
  timing.iteration_count = iteration_count;
  return true;
}

bool TimingInput::SetIterationDuration(
    Timing& timing,
    const UnrestrictedDoubleOrString& iteration_duration,
    ExceptionState& exception_state) {
  static const char* error_message = "duration must be non-negative or auto.";

  if (iteration_duration.IsUnrestrictedDouble()) {
    double duration_number = iteration_duration.GetAsUnrestrictedDouble();
    if (std::isnan(duration_number) || duration_number < 0) {
      exception_state.ThrowTypeError(error_message);
      return false;
    }
    timing.iteration_duration = duration_number / 1000;
    return true;
  }

  if (iteration_duration.GetAsString() != "auto") {
    exception_state.ThrowTypeError(error_message);
    return false;
  }

  timing.iteration_duration = Timing::Defaults().iteration_duration;
  return true;
}

void TimingInput::SetPlaybackRate(Timing& timing, double playback_rate) {
  if (std::isfinite(playback_rate))
    timing.playback_rate = playback_rate;
  else
    timing.playback_rate = Timing::Defaults().playback_rate;
}

void TimingInput::SetPlaybackDirection(Timing& timing,
                                       const String& direction) {
  if (direction == "reverse") {
    timing.direction = Timing::PlaybackDirection::REVERSE;
  } else if (direction == "alternate") {
    timing.direction = Timing::PlaybackDirection::ALTERNATE_NORMAL;
  } else if (direction == "alternate-reverse") {
    timing.direction = Timing::PlaybackDirection::ALTERNATE_REVERSE;
  } else {
    timing.direction = Timing::Defaults().direction;
  }
}

bool TimingInput::SetTimingFunction(Timing& timing,
                                    const String& timing_function_string,
                                    Document* document,
                                    ExceptionState& exception_state) {
  if (scoped_refptr<TimingFunction> timing_function =
          AnimationInputHelpers::ParseTimingFunction(
              timing_function_string, document, exception_state)) {
    timing.timing_function = timing_function;
    return true;
  }
  return false;
}

bool TimingInput::Convert(
    const UnrestrictedDoubleOrKeyframeEffectOptions& options,
    Timing& timing_output,
    Document* document,
    ExceptionState& exception_state) {
  if (options.IsKeyframeEffectOptions()) {
    return Convert(options.GetAsKeyframeEffectOptions(), timing_output,
                   document, exception_state);
  } else if (options.IsUnrestrictedDouble()) {
    return Convert(options.GetAsUnrestrictedDouble(), timing_output,
                   exception_state);
  } else if (options.IsNull()) {
    return true;
  }
  NOTREACHED();
  return false;
}

bool TimingInput::Convert(
    const UnrestrictedDoubleOrKeyframeAnimationOptions& options,
    Timing& timing_output,
    Document* document,
    ExceptionState& exception_state) {
  if (options.IsKeyframeAnimationOptions()) {
    return Convert(options.GetAsKeyframeAnimationOptions(), timing_output,
                   document, exception_state);
  } else if (options.IsUnrestrictedDouble()) {
    return Convert(options.GetAsUnrestrictedDouble(), timing_output,
                   exception_state);
  } else if (options.IsNull()) {
    return true;
  }
  NOTREACHED();
  return false;
}

bool TimingInput::Convert(const KeyframeEffectOptions& timing_input,
                          Timing& timing_output,
                          Document* document,
                          ExceptionState& exception_state) {
  SetStartDelay(timing_output, timing_input.delay());
  SetEndDelay(timing_output, timing_input.endDelay());
  SetFillMode(timing_output, timing_input.fill());

  if (!SetIterationStart(timing_output, timing_input.iterationStart(),
                         exception_state))
    return false;

  if (!SetIterationCount(timing_output, timing_input.iterations(),
                         exception_state))
    return false;

  if (!SetIterationDuration(timing_output, timing_input.duration(),
                            exception_state))
    return false;

  SetPlaybackRate(timing_output, 1.0);
  SetPlaybackDirection(timing_output, timing_input.direction());

  if (!SetTimingFunction(timing_output, timing_input.easing(), document,
                         exception_state))
    return false;

  timing_output.AssertValid();

  return true;
}

bool TimingInput::Convert(const KeyframeAnimationOptions& timing_input,
                          Timing& timing_output,
                          Document* document,
                          ExceptionState& exception_state) {
  // The "id" field isn't used, so upcast to KeyframeEffectOptions.
  const KeyframeEffectOptions* const timing_input_ptr = &timing_input;
  return Convert(*timing_input_ptr, timing_output, document, exception_state);
}

bool TimingInput::Convert(double duration,
                          Timing& timing_output,
                          ExceptionState& exception_state) {
  DCHECK(timing_output == Timing::Defaults());
  return SetIterationDuration(
      timing_output,
      UnrestrictedDoubleOrString::FromUnrestrictedDouble(duration),
      exception_state);
}

}  // namespace blink
