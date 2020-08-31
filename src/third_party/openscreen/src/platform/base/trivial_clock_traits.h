// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TRIVIAL_CLOCK_TRAITS_H_
#define PLATFORM_BASE_TRIVIAL_CLOCK_TRAITS_H_

#include <chrono>
#include <ostream>

namespace openscreen {

// The Open Screen monotonic clock traits description, providing all the C++14
// requirements of a TrivialClock, for use with STL <chrono>.
class TrivialClockTraits {
 public:
  // TrivialClock named requirements: std::chrono templates can/may use these.
  using duration = std::chrono::microseconds;
  using rep = duration::rep;
  using period = duration::period;
  using time_point = std::chrono::time_point<TrivialClockTraits, duration>;
  static constexpr bool is_steady = true;

  // Time point values from the clock use microsecond precision, as a reasonably
  // high-resoulution clock is required. The time source must tick forward at
  // least 10000 times per second.
  using kRequiredResolution = std::ratio<1, 10000>;

  // In <chrono>, a clock type is just some type properties plus a static now()
  // function. So, there's nothing to instantiate here.
  TrivialClockTraits() = delete;
  ~TrivialClockTraits() = delete;

  // "Trivially copyable" is necessary for using the time types in
  // std::atomic<>.
  static_assert(std::is_trivially_copyable<duration>(),
                "duration is not trivially copyable");
  static_assert(std::is_trivially_copyable<time_point>(),
                "time_point is not trivially copyable");
};

// Convenience type definition, for injecting time sources into classes (e.g.,
// &Clock::now versus something else for testing).
using ClockNowFunctionPtr = TrivialClockTraits::time_point (*)();

// Logging convenience for durations. Outputs a string of the form "123µs".
std::ostream& operator<<(std::ostream& os,
                         const TrivialClockTraits::duration& d);

// Logging convenience for time points. Outputs a string of the form
// "123µs-ticks".
std::ostream& operator<<(std::ostream& os,
                         const TrivialClockTraits::time_point& tp);

// Logging (and gtest pretty-printing) for several commonly-used chrono types.
std::ostream& operator<<(std::ostream& out, const std::chrono::hours&);
std::ostream& operator<<(std::ostream& out, const std::chrono::minutes&);
std::ostream& operator<<(std::ostream& out, const std::chrono::seconds&);
std::ostream& operator<<(std::ostream& out, const std::chrono::milliseconds&);
// Note: The ostream output operator for std::chrono::microseconds is handled by
// the one for TrivialClockTraits::duration above since they are the same type.

}  // namespace openscreen

#endif  // PLATFORM_BASE_TRIVIAL_CLOCK_TRAITS_H_
