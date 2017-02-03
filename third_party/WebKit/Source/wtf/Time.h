// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_Time_h
#define WTF_Time_h

#include "base/time/time.h"
#include "wtf/CurrentTime.h"

namespace WTF {
// Provides thin wrappers around the following basic time types from
// base/time package:
//
//  - WTF::TimeDelta is an alias for base::TimeDelta and represents a duration
//    of time.
//  - WTF::TimeTicks wraps base::TimeTicks and represents a monotonic time
//    value.
//  - WTF::Time wraps base::Time and represents a wall time value.
//
// For usage guideline please see the documentation in base/time/time.h

using TimeDelta = base::TimeDelta;

namespace internal {

template <class WrappedTimeType>
class TimeWrapper {
 public:
  TimeWrapper() {}

  static TimeWrapper Now() {
    if (WTF::getTimeFunctionForTesting()) {
      double seconds = (WTF::getTimeFunctionForTesting())();
      return TimeWrapper() + TimeDelta::FromSecondsD(seconds);
    }
    return TimeWrapper(WrappedTimeType::Now());
  }

  int64_t ToInternalValueForTesting() const {
    return m_value.ToInternalValue();
  }

  // Only use this conversion when interfacing with legacy code that represents
  // time in double. Converting to double can lead to losing information for
  // large time values.
  double InSeconds() const {
    return (m_value - WrappedTimeType()).InSecondsF();
  }

  static TimeWrapper FromSeconds(double seconds) {
    return WrappedTimeType() + TimeDelta::FromSecondsD(seconds);
  }

  TimeWrapper& operator=(TimeWrapper other) {
    m_value = other.m_value;
    return *this;
  }

  TimeDelta operator-(TimeWrapper other) const {
    return m_value - other.m_value;
  }

  TimeWrapper operator+(TimeDelta delta) const {
    return TimeWrapper(m_value + delta);
  }
  TimeWrapper operator-(TimeDelta delta) const {
    return TimeWrapper(m_value - delta);
  }

  TimeWrapper& operator+=(TimeDelta delta) {
    m_value += delta;
    return *this;
  }
  TimeWrapper& operator-=(TimeDelta delta) {
    m_value -= delta;
    return *this;
  }

  bool operator==(TimeWrapper other) const { return m_value == other.m_value; }
  bool operator!=(TimeWrapper other) const { return m_value != other.m_value; }
  bool operator<(TimeWrapper other) const { return m_value < other.m_value; }
  bool operator<=(TimeWrapper other) const { return m_value <= other.m_value; }
  bool operator>(TimeWrapper other) const { return m_value > other.m_value; }
  bool operator>=(TimeWrapper other) const { return m_value >= other.m_value; }

 private:
  WrappedTimeType m_value;
  TimeWrapper(WrappedTimeType value) : m_value(value) {}
};

}  // namespace internal

using Time = internal::TimeWrapper<base::Time>;
using TimeTicks = internal::TimeWrapper<base::TimeTicks>;

}  // namespace WTF

using WTF::Time;
using WTF::TimeDelta;
using WTF::TimeTicks;

#endif  // Time_h
