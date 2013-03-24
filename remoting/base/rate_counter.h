// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_RATE_COUNTER_H_
#define REMOTING_BASE_RATE_COUNTER_H_

#include <queue>
#include <utility>

#include "base/basictypes.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"

namespace remoting {

// Measures average rate per second of a sequence of point rate samples
// over a specified time window. This can be used to measure bandwidth, frame
// rates, etc.
class RateCounter : public base::NonThreadSafe {
 public:
  // Constructs a rate counter over the specified |time_window|.
  explicit RateCounter(base::TimeDelta time_window);
  virtual ~RateCounter();

  // Records a point event count to include in the rate.
  void Record(int64 value);

  // Returns the rate-per-second of values recorded over the time window.
  // Note that rates reported before |time_window| has elapsed are not accurate.
  double Rate();

  // Overrides the current time for testing.
  void SetCurrentTimeForTest(base::Time current_time);

 private:
  // Type used to store data points with timestamps.
  typedef std::pair<base::Time, int64> DataPoint;

  // Removes data points more than |time_window| older than |current_time|.
  void EvictOldDataPoints(base::Time current_time);

  // Returns the current time specified for test, if set, or base::Time::Now().
  base::Time CurrentTime() const;

  // Time window over which to calculate the rate.
  const base::TimeDelta time_window_;

  // Queue containing data points in the order in which they were recorded.
  std::queue<DataPoint> data_points_;

  // Sum of values in |data_points_|.
  int64 sum_;

  // If set, used to calculate the running average, in place of Now().
  base::Time current_time_for_test_;

  DISALLOW_COPY_AND_ASSIGN(RateCounter);
};

}  // namespace remoting

#endif  // REMOTING_BASE_RATE_COUNTER_H_
