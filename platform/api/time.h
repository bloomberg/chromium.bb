// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TIME_H_
#define PLATFORM_API_TIME_H_

#include <cstdint>

namespace openscreen {
namespace platform {

class TimeDelta {
 public:
  constexpr static TimeDelta FromMilliseconds(int64_t msec) {
    return TimeDelta(msec * 1000);
  }
  constexpr static TimeDelta FromMicroseconds(int64_t usec) {
    return TimeDelta(usec);
  }

  constexpr int64_t AsSeconds() const { return microseconds_ / 1000000; }
  constexpr int64_t AsMilliseconds() const { return microseconds_ / 1000; }
  constexpr int64_t AsMicroseconds() const { return microseconds_; }

 private:
  constexpr explicit TimeDelta(int64_t usec) : microseconds_(usec) {}

  friend constexpr TimeDelta operator-(TimeDelta t1, TimeDelta t2) {
    return TimeDelta(t1.microseconds_ - t2.microseconds_);
  }
  friend constexpr bool operator<(TimeDelta t1, TimeDelta t2) {
    return t1.microseconds_ < t2.microseconds_;
  }
  friend constexpr bool operator<=(TimeDelta t1, TimeDelta t2) {
    return t1.microseconds_ <= t2.microseconds_;
  }
  friend constexpr bool operator>(TimeDelta t1, TimeDelta t2) {
    return t1.microseconds_ > t2.microseconds_;
  }
  friend constexpr bool operator>=(TimeDelta t1, TimeDelta t2) {
    return t1.microseconds_ >= t2.microseconds_;
  }
  friend constexpr bool operator==(TimeDelta t1, TimeDelta t2) {
    return t1.microseconds_ == t2.microseconds_;
  }
  friend constexpr bool operator!=(TimeDelta t1, TimeDelta t2) {
    return t1.microseconds_ != t2.microseconds_;
  }

  int64_t microseconds_;
};

TimeDelta GetMonotonicTimeNow();
TimeDelta GetUTCNow();

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_API_TIME_H_
