// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_AVERAGE_LAG_TRACKER_H_
#define UI_LATENCY_AVERAGE_LAG_TRACKER_H_

#include <deque>

#include "base/macros.h"
#include "ui/latency/latency_info.h"

namespace ui {

// A class for reporting AverageLag metrics. See
// https://docs.google.com/document/d/1e8NuzPblIv2B9bz01oSj40rmlse7_PHq5oFS3lqz6N4/
class AverageLagTracker {
 public:
  AverageLagTracker();
  ~AverageLagTracker();
  void AddLatencyInFrame(const LatencyInfo& latency,
                         base::TimeTicks gpu_swap_begin_timestamp,
                         const std::string& scroll_name);

 private:
  typedef struct LagAreaInFrame {
    LagAreaInFrame(base::TimeTicks time, float rendered_pos = 0)
        : frame_time(time),
          rendered_accumulated_delta(rendered_pos),
          lag_area(0) {}
    base::TimeTicks frame_time;
    float rendered_accumulated_delta;
    float lag_area;
  } LagAreaInFrame;

  // Calculate lag in 1 seconds intervals and report UMA.
  void CalculateAndReportAverageLagUma(bool send_anyway = false);

  // Helper function to calculate lag area between |front_time| to
  // |back_time|.
  float LagBetween(base::TimeTicks front_time,
                   base::TimeTicks back_time,
                   const LatencyInfo& latency,
                   base::TimeTicks event_time);

  std::deque<LagAreaInFrame> frame_lag_infos_;

  // Last scroll event's timestamp in the sequence, reset on ScrollBegin.
  base::TimeTicks last_event_timestamp_;
  // Timestamp of the last frame popped from |frame_lag_infos_| queue.
  base::TimeTicks last_finished_frame_time_;

  // Accumulated scroll delta for actual scroll update events. Cumulated from
  // latency.scroll_update_delta(). Reset on ScrollBegin.
  float last_event_accumulated_delta_ = 0;
  // Accumulated scroll delta got rendered on gpu swap. Cumulated from
  // latency.predicted_scroll_update_delta(). It always has same value as
  // |last_event_accumulated_delta_| when scroll prediction is disabled.
  float last_rendered_accumulated_delta_ = 0;

  // This keeps track of the last report_time when we report to UMA, so we can
  // calculate the report's duration by current - last. Reset on ScrollBegin.
  base::TimeTicks last_reported_time_;

  // True if the first element of |frame_lag_infos_| is for ScrollBegin.
  // For ScrollBegin, we don't wait for the 1 second interval but record the
  // UMA once the frame is finished.
  bool is_begin_ = false;

  // Accumulated lag area in the 1 second intervals.
  float accumulated_lag_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AverageLagTracker);
};

}  // namespace ui

#endif  // UI_LATENCY_AVERAGE_LAG_TRACKER_H_
