// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RunningAverage defined in this file is used to generate statistics for
// bandwidth, latency and other performance metrics for remoting. Usually
// this data comes in as a stream and fluctuates a lot. They are processed by
// this class to generate a more stable value by taking average within a
// window of data points.

// All classes defined are thread-safe.

#ifndef REMOTING_BASE_RUNNING_AVERAGE_H_
#define REMOTING_BASE_RUNNING_AVERAGE_H_

#include <deque>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"

namespace remoting {

class RunningAverage {
 public:
  // Construct a running average counter for a specific window size. The
  // |windows_size| most recent values are kept and the average is reported.
  explicit RunningAverage(int window_size);

  virtual ~RunningAverage();

  // Record the provided data point.
  void Record(int64 value);

  // Return the average of data points in the last window.
  double Average();

 private:
  // Size of the window. This is of type size_t to avoid casting when comparing
  // with the size of |data_points_|.
  size_t window_size_;

  // Protects |data_points_| and |sum_|.
  base::Lock lock_;

  // Keep the values of all the data points.
  std::deque<int64> data_points_;

  // Sum of values in |data_points_|.
  int64 sum_;

  DISALLOW_COPY_AND_ASSIGN(RunningAverage);
};

}  // namespace remoting

#endif  // REMOTING_BASE_RUNNING_AVERAGE_H_
