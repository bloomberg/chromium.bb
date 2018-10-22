// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIME_CLAMPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIME_CLAMPER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"

#include <stdint.h>

namespace blink {

class CORE_EXPORT TimeClamper {
 public:
  static constexpr double kResolutionSeconds = 0.0001;

  TimeClamper();

  // Deterministically clamp the time value |time_seconds| to a 100us interval
  // to prevent timing attacks. See
  // http://www.w3.org/TR/hr-time-2/#privacy-security.
  //
  // For each clamped time interval, we compute a pseudorandom transition
  // threshold. The returned time will either be the start of that interval or
  // the next one depending on which side of the threshold |time_seconds| is.
  double ClampTimeResolution(double time_seconds) const;

 private:
  inline double ThresholdFor(double clamped_time) const;
  static inline double ToDouble(uint64_t value);
  static inline uint64_t MurmurHash3(uint64_t value);

  uint64_t secret_;

  DISALLOW_COPY_AND_ASSIGN(TimeClamper);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_TIME_CLAMPER_H_
