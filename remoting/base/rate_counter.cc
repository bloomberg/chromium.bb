// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/rate_counter.h"

namespace remoting {

RateCounter::RateCounter(base::TimeDelta time_window)
    : time_window_(time_window),
      sum_(0) {
}

RateCounter::~RateCounter() {
}

void RateCounter::Record(int64 value) {
  base::Time current_time = base::Time::Now();
  Evict(current_time);

  base::AutoLock auto_lock(lock_);
  sum_ += value;
  data_points_.push(std::make_pair(current_time, value));
}

double RateCounter::Rate() {
  Evict(base::Time::Now());

  base::AutoLock auto_lock(lock_);
  return static_cast<double>(base::Time::kMillisecondsPerSecond) * sum_ /
      time_window_.InMilliseconds();
}

void RateCounter::Evict(base::Time current_time) {
  base::AutoLock auto_lock(lock_);

  // Remove data points outside of the window.
  base::Time window_start = current_time - time_window_;

  while (!data_points_.empty()) {
    if (data_points_.front().first > window_start)
      break;

    sum_ -= data_points_.front().second;
    data_points_.pop();
  }
}

}  // namespace remoting
