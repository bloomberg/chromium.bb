/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/time.h"


namespace {

// Time between resampling the un-granular clock for this API.  60 seconds.
const int kMaxMillisecondsToAvoidDrift =
    60 * NaCl::Time::kMillisecondsPerSecond;

}  // namespace

// TimeDelta ------------------------------------------------------------------

// static
NaCl::TimeDelta NaCl::TimeDelta::FromDays(int64_t days) {
  return TimeDelta(days * Time::kMicrosecondsPerDay);
}

// static
NaCl::TimeDelta NaCl::TimeDelta::FromHours(int64_t hours) {
  return TimeDelta(hours * Time::kMicrosecondsPerHour);
}

// static
NaCl::TimeDelta NaCl::TimeDelta::FromMinutes(int64_t minutes) {
  return TimeDelta(minutes * Time::kMicrosecondsPerMinute);
}

// static
NaCl::TimeDelta NaCl::TimeDelta::FromSeconds(int64_t secs) {
  return TimeDelta(secs * Time::kMicrosecondsPerSecond);
}

// static
NaCl::TimeDelta NaCl::TimeDelta::FromMilliseconds(int64_t ms) {
  return TimeDelta(ms * Time::kMicrosecondsPerMillisecond);
}

// static
NaCl::TimeDelta NaCl::TimeDelta::FromMicroseconds(int64_t us) {
  return TimeDelta(us);
}

int NaCl::TimeDelta::InDays() const {
  return static_cast<int>(delta_ / Time::kMicrosecondsPerDay);
}

double NaCl::TimeDelta::InSecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerSecond;
}

int64_t NaCl::TimeDelta::InSeconds() const {
  return delta_ / Time::kMicrosecondsPerSecond;
}

double NaCl::TimeDelta::InMillisecondsF() const {
  return static_cast<double>(delta_) / Time::kMicrosecondsPerMillisecond;
}

int64_t NaCl::TimeDelta::InMilliseconds() const {
  return delta_ / Time::kMicrosecondsPerMillisecond;
}

int64_t NaCl::TimeDelta::InMicroseconds() const {
  return delta_;
}

// Time -----------------------------------------------------------------------

int64_t NaCl::Time::initial_time_ = 0;
NaCl::TimeTicks NaCl::Time::initial_ticks_;

// static
void NaCl::Time::InitializeClock() {
    initial_ticks_ = TimeTicks::Now();
    initial_time_ = CurrentWallclockMicroseconds();
}

// static
NaCl::Time NaCl::Time::Now() {
  if (initial_time_ == 0)
    InitializeClock();

  // We implement time using the high-resolution timers so that we can get
  // timeouts which are smaller than 10-15ms.  If we just used
  // CurrentWallclockMicroseconds(), we'd have the less-granular timer.
  //
  // To make this work, we initialize the clock (initial_time) and the
  // counter (initial_ctr).  To compute the initial time, we can check
  // the number of ticks that have elapsed, and compute the delta.
  //
  // To avoid any drift, we periodically resync the counters to the system
  // clock.
  while (true) {
    TimeTicks ticks = TimeTicks::Now();

    // Calculate the time elapsed since we started our timer
    TimeDelta elapsed = ticks - initial_ticks_;

    // Check if enough time has elapsed that we need to resync the clock.
    if (elapsed.InMilliseconds() > kMaxMillisecondsToAvoidDrift) {
      InitializeClock();
      continue;
    }

    return elapsed + Time(initial_time_);
  }
}

// static
NaCl::Time NaCl::Time::FromTimeT(time_t tt) {
  if (tt == 0)
    return Time();  // Preserve 0 so we can tell it doesn't exist.
  return NaCl::Time((tt * kMicrosecondsPerSecond) + kTimeTToMicrosecondsOffset);
}

time_t NaCl::Time::ToTimeT() const {
  if (us_ == 0)
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  return (us_ - kTimeTToMicrosecondsOffset) / kMicrosecondsPerSecond;
}

double NaCl::Time::ToDoubleT() const {
  if (us_ == 0)
    return 0;  // Preserve 0 so we can tell it doesn't exist.
  return (static_cast<double>(us_ - kTimeTToMicrosecondsOffset) /
          static_cast<double>(kMicrosecondsPerSecond));
}

NaCl::Time NaCl::Time::LocalMidnight() const {
  Exploded exploded;
  LocalExplode(&exploded);
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  return FromLocalExploded(exploded);
}
