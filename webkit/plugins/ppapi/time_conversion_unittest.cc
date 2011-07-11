// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/plugins/ppapi/time_conversion.h"

namespace webkit {
namespace ppapi {

// Slop we'll allow in two Time "internal values" to consider them equal.
// Double conversion can introduce rounding errors. The internal values are in
// microseconds, so an error here is very small.
static const int kTimeInternalValueSlop = 2;

// Same as above in double-precision seconds units.
static const double kTimeSecondsSlop =
    static_cast<double>(kTimeInternalValueSlop) /
    base::Time::kMicrosecondsPerSecond;

// Fails on linux. http://crbug.com/88971
#if defined(OS_LINUX)
#define MAYBE_Time FAILS_Time
#else
#define MAYBE_TIME Time
#endif
TEST(TimeConversion, MAYBE_Time) {
  // Should be able to round-trip.
  base::Time now = base::Time::Now();
  base::Time converted = PPTimeToTime(TimeToPPTime(now));
  EXPECT_GE(kTimeInternalValueSlop,
            abs(static_cast<int>((converted - now).ToInternalValue())));

  // Units should be in seconds.
  base::Time one_second_from_now = now + base::TimeDelta::FromSeconds(1);
  EXPECT_EQ(1.0, TimeToPPTime(one_second_from_now) - TimeToPPTime(now));
}

TEST(TimeConversion, EventTime) {
  // Should be able to round-trip.
  base::Time now = base::Time::Now();
  double event_now = now.ToDoubleT();
  double converted = EventTimeToPPTimeTicks(PPTimeTicksToEventTime(event_now));
  EXPECT_GE(kTimeSecondsSlop, fabs(converted - event_now));

  // Units should be in seconds.
  base::Time one_second_from_now = now + base::TimeDelta::FromSeconds(1);
  double event_one_second_from_now = one_second_from_now.ToDoubleT();
  EXPECT_GE(kTimeSecondsSlop,
            1.0 - EventTimeToPPTimeTicks(event_one_second_from_now) -
                EventTimeToPPTimeTicks(event_now));
}

}  // namespace ppapi
}  // namespace webkit
