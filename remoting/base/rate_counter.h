// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// RateCounter is defined to measure average rate over a given time window.
// Rate is reported as the sum of values recorded divided by the time window.
// This can be used for measuring bandwidth, bitrate, etc.

// This class is thread-safe.

#ifndef REMOTING_BASE_RATE_COUNTER_H_
#define REMOTING_BASE_RATE_COUNTER_H_

#include <queue>
#include <utility>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "base/time.h"

namespace remoting {

class RateCounter {
 public:
  // Construct a counter for a specific time window.
  RateCounter(base::TimeDelta time_window);

  virtual ~RateCounter();

  // Record the data point.
  void Record(int64 value);

  // Report the rate recorded. At the beginning of recording the numbers before
  // |time_window| is reached the reported rate will not be accurate.
  double Rate();

 private:
  // Helper function to evict old data points.
  void Evict(base::Time current_time);

  // A data point consists of a timestamp and a data value.
  typedef std::pair<base::Time, int64> DataPoint;

  // Duration of the time window.
  base::TimeDelta time_window_;

  // Protects |data_points_| and |sum_|.
  base::Lock lock_;

  // Keep the values of all the data points in a queue.
  std::queue<DataPoint> data_points_;

  // Sum of values in |data_points_|.
  int64 sum_;

  DISALLOW_COPY_AND_ASSIGN(RateCounter);
};

}  // namespace remoting

#endif  // REMOTING_BASE_RATE_COUNTER_H_
