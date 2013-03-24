// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/running_average.h"

#include "base/logging.h"

namespace remoting {

RunningAverage::RunningAverage(int window_size)
    : window_size_(window_size),
      sum_(0) {
  DCHECK_GT(window_size, 0);
}

RunningAverage::~RunningAverage() {
}

void RunningAverage::Record(int64 value) {
  DCHECK(CalledOnValidThread());

  data_points_.push_back(value);
  sum_ += value;

  if (data_points_.size() > window_size_) {
    sum_ -= data_points_[0];
    data_points_.pop_front();
  }
}

double RunningAverage::Average() const {
  DCHECK(CalledOnValidThread());

  if (data_points_.empty())
    return 0;
  return static_cast<double>(sum_) / data_points_.size();
}

}  // namespace remoting
