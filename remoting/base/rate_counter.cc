// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/rate_counter.h"

#include "base/logging.h"

namespace remoting {

RateCounter::RateCounter(base::TimeDelta time_window)
    : time_window_(time_window),
      sum_(0) {
  DCHECK_GT(time_window.InMilliseconds(), 0);
}

RateCounter::~RateCounter() {
}

void RateCounter::Record(int64 value) {
  DCHECK(CalledOnValidThread());

  base::Time current_time = CurrentTime();
  EvictOldDataPoints(current_time);
  sum_ += value;
  data_points_.push(std::make_pair(current_time, value));
}

double RateCounter::Rate() {
  DCHECK(CalledOnValidThread());

  EvictOldDataPoints(CurrentTime());
  return sum_ / time_window_.InSecondsF();
}

void RateCounter::SetCurrentTimeForTest(base::Time current_time) {
  DCHECK(CalledOnValidThread());
  DCHECK(current_time >= current_time_for_test_);

  current_time_for_test_ = current_time;
}

void RateCounter::EvictOldDataPoints(base::Time current_time) {
  // Remove data points outside of the window.
  base::Time window_start = current_time - time_window_;

  while (!data_points_.empty()) {
    if (data_points_.front().first > window_start)
      break;

    sum_ -= data_points_.front().second;
    data_points_.pop();
  }
}

base::Time RateCounter::CurrentTime() const {
  if (current_time_for_test_ == base::Time())
    return base::Time::Now();
  return current_time_for_test_;
}

}  // namespace remoting
