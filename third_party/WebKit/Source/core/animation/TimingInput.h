// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TimingInput_h
#define TimingInput_h

#include "core/CoreExport.h"
#include "core/animation/Timing.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class Document;
class ExceptionState;
class KeyframeAnimationOptions;
class KeyframeEffectOptions;
class UnrestrictedDoubleOrString;
class UnrestrictedDoubleOrKeyframeAnimationOptions;
class UnrestrictedDoubleOrKeyframeEffectOptions;

class CORE_EXPORT TimingInput {
  STATIC_ONLY(TimingInput);

 public:
  static bool Convert(const UnrestrictedDoubleOrKeyframeEffectOptions&,
                      Timing& timing_output,
                      Document*,
                      ExceptionState&);
  static bool Convert(const UnrestrictedDoubleOrKeyframeAnimationOptions&,
                      Timing& timing_output,
                      Document*,
                      ExceptionState&);
  static bool Convert(const KeyframeEffectOptions& timing_input,
                      Timing& timing_output,
                      Document*,
                      ExceptionState&);
  static bool Convert(const KeyframeAnimationOptions& timing_input,
                      Timing& timing_output,
                      Document*,
                      ExceptionState&);

  static bool Convert(double duration, Timing& timing_output, ExceptionState&);

  static void SetStartDelay(Timing&, double start_delay);
  static void SetEndDelay(Timing&, double end_delay);
  static void SetFillMode(Timing&, const String& fill_mode);
  static bool SetIterationStart(Timing&,
                                double iteration_start,
                                ExceptionState&);
  static bool SetIterationCount(Timing&,
                                double iteration_count,
                                ExceptionState&);
  static bool SetIterationDuration(Timing&,
                                   const UnrestrictedDoubleOrString&,
                                   ExceptionState&);
  static void SetPlaybackRate(Timing&, double playback_rate);
  static void SetPlaybackDirection(Timing&, const String& direction);
  static bool SetTimingFunction(Timing&,
                                const String& timing_function_string,
                                Document*,
                                ExceptionState&);
};

}  // namespace blink

#endif
