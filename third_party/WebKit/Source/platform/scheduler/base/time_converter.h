// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TIME_CONVERTER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TIME_CONVERTER_H_

#include "base/time/time.h"

namespace blink {
namespace scheduler {

// TODO(scheduler-dev): Remove conversions when Blink starts using
// base::TimeTicks instead of doubles for time.
static inline base::TimeTicks MonotonicTimeInSecondsToTimeTicks(
    double monotonic_time_in_seconds) {
  return base::TimeTicks() +
         base::TimeDelta::FromSecondsD(monotonic_time_in_seconds);
}

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_TIME_CONVERTER_H_
