// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/time_conversion.h"

namespace ppapi {

namespace {

// Since WebKit doesn't use ticks for event times, we have to compute what
// the time ticks would be assuming the wall clock time doesn't change.
//
// This should only be used for WebKit times which we can't change the
// definition of.
double GetTimeToTimeTicksDeltaInSeconds() {
  static double time_to_ticks_delta_seconds = 0.0;
  if (time_to_ticks_delta_seconds == 0.0) {
    double wall_clock = TimeToPPTime(base::Time::Now());
    double ticks = TimeTicksToPPTimeTicks(base::TimeTicks::Now());
    time_to_ticks_delta_seconds = ticks - wall_clock;
  }
  return time_to_ticks_delta_seconds;
}

}  // namespace

PP_Time TimeToPPTime(base::Time t) {
  return t.ToDoubleT();
}

base::Time PPTimeToTime(PP_Time t) {
  return base::Time::FromDoubleT(t);
}

PP_TimeTicks TimeTicksToPPTimeTicks(base::TimeTicks t) {
  return static_cast<double>(t.ToInternalValue()) /
      base::Time::kMicrosecondsPerSecond;
}

PP_TimeTicks EventTimeToPPTimeTicks(double event_time) {
  return event_time + GetTimeToTimeTicksDeltaInSeconds();
}

double PPTimeTicksToEventTime(PP_TimeTicks t) {
  return t - GetTimeToTimeTicksDeltaInSeconds();
}

}  // namespace ppapi
