// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_RUNNING_AVERAGE_H_
#define REMOTING_BASE_RUNNING_AVERAGE_H_

#include <deque>

#include "base/basictypes.h"
#include "base/threading/thread_checker.h"

namespace remoting {

// Calculates the average of the most recent N recorded samples.
// This is typically used to smooth out random variation in point samples
// over bandwidth, frame rate, etc.
class RunningAverage {
 public:
  // Constructs a helper to average over the |window_size| most recent samples.
  explicit RunningAverage(int window_size);
  virtual ~RunningAverage();

  // Records a point sample.
  void Record(int64_t value);

  // Returns the average over up to |window_size| of the most recent samples.
  double Average();

 private:
  // Stores the desired window size, as size_t to avoid casting when comparing
  // with the size of |data_points_|.
  const size_t window_size_;

  // Stores the |window_size| most recently recorded samples.
  std::deque<int64_t> data_points_;

  // Holds the sum of the samples in |data_points_|.
  int64_t sum_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RunningAverage);
};

}  // namespace remoting

#endif  // REMOTING_BASE_RUNNING_AVERAGE_H_
