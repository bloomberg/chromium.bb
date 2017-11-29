// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_Time_h
#define WTF_Time_h

#include "base/time/time.h"
#include "platform/wtf/WTFExport.h"

namespace WTF {
// Provides thin wrappers around the following basic time types from
// base/time package:
//
//  - WTF::TimeDelta is an alias for base::TimeDelta and represents a duration
//    of time.
//  - WTF::TimeTicks wraps base::TimeTicks and represents a monotonic time
//    value.
//  - WTF::Time is an alias for base::Time and represents a wall time value.
//
// For usage guideline please see the documentation in base/time/time.h

using TimeDelta = base::TimeDelta;
using Time = base::Time;

// Returns the current UTC time in seconds, counted from January 1, 1970.
// Precision varies depending on platform but is usually as good or better
// than a millisecond.
WTF_EXPORT double CurrentTime();

// Same thing, in milliseconds.
inline double CurrentTimeMS() {
  return CurrentTime() * 1000.0;
}

// Provides a monotonically increasing time in seconds since an arbitrary point
// in the past.  On unsupported platforms, this function only guarantees the
// result will be non-decreasing.
WTF_EXPORT double MonotonicallyIncreasingTime();

// Same thing, in milliseconds.
inline double MonotonicallyIncreasingTimeMS() {
  return MonotonicallyIncreasingTime() * 1000.0;
}

using TimeFunction = double (*)();

// Make all the time functions (currentTime(), monotonicallyIncreasingTime(),
// systemTraceTime()) return the result of the supplied function. Returns the
// pointer to the old time function. For both setting and getting, nullptr
// means using the default timing function returning the actual time.
WTF_EXPORT TimeFunction SetTimeFunctionsForTesting(TimeFunction);

// Allows wtf/Time.h to use the same mock time function
WTF_EXPORT TimeFunction GetTimeFunctionForTesting();

class TimeTicks {
 public:
  TimeTicks() {}
  TimeTicks(base::TimeTicks value) : value_(value) {}

  static TimeTicks Now() {
    if (WTF::GetTimeFunctionForTesting()) {
      double seconds = (WTF::GetTimeFunctionForTesting())();
      return TimeTicks() + TimeDelta::FromSecondsD(seconds);
    }
    return TimeTicks(base::TimeTicks::Now());
  }

  static TimeTicks UnixEpoch() {
    return TimeTicks(base::TimeTicks::UnixEpoch());
  }

  int64_t ToInternalValueForTesting() const { return value_.ToInternalValue(); }

  // Only use this conversion when interfacing with legacy code that represents
  // time in double. Converting to double can lead to losing information for
  // large time values.
  double InSeconds() const { return (value_ - base::TimeTicks()).InSecondsF(); }

  static TimeTicks FromSeconds(double seconds) {
    return TimeTicks() + TimeDelta::FromSecondsD(seconds);
  }

  bool is_null() const { return value_.is_null(); }

  operator base::TimeTicks() const { return value_; }

  TimeTicks& operator=(TimeTicks other) {
    value_ = other.value_;
    return *this;
  }

  TimeDelta operator-(TimeTicks other) const { return value_ - other.value_; }

  TimeTicks operator+(TimeDelta delta) const {
    return TimeTicks(value_ + delta);
  }
  TimeTicks operator-(TimeDelta delta) const {
    return TimeTicks(value_ - delta);
  }

  TimeTicks& operator+=(TimeDelta delta) {
    value_ += delta;
    return *this;
  }
  TimeTicks& operator-=(TimeDelta delta) {
    value_ -= delta;
    return *this;
  }

  bool operator==(TimeTicks other) const { return value_ == other.value_; }
  bool operator!=(TimeTicks other) const { return value_ != other.value_; }
  bool operator<(TimeTicks other) const { return value_ < other.value_; }
  bool operator<=(TimeTicks other) const { return value_ <= other.value_; }
  bool operator>(TimeTicks other) const { return value_ > other.value_; }
  bool operator>=(TimeTicks other) const { return value_ >= other.value_; }

 private:
  base::TimeTicks value_;
};

}  // namespace WTF

using WTF::CurrentTime;
using WTF::CurrentTimeMS;
using WTF::MonotonicallyIncreasingTime;
using WTF::MonotonicallyIncreasingTimeMS;
using WTF::SetTimeFunctionsForTesting;
using WTF::Time;
using WTF::TimeDelta;
using WTF::TimeFunction;
using WTF::TimeTicks;

#endif  // Time_h
