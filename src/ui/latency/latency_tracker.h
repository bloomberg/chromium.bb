// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_LATENCY_TRACKER_H_
#define UI_LATENCY_LATENCY_TRACKER_H_

#include "ui/latency/latency_info.h"

namespace ui {

// Utility class for tracking the latency of events. Relies on LatencyInfo
// components logged by content::RenderWidgetHostLatencyTracker.
class LatencyTracker {
 public:
  LatencyTracker();

  LatencyTracker(const LatencyTracker&) = delete;
  LatencyTracker& operator=(const LatencyTracker&) = delete;

  ~LatencyTracker();

  // Terminates latency tracking for events that triggered rendering, also
  // performing relevant UMA latency reporting.
  // Called when GPU buffers swap completes.
  void OnGpuSwapBuffersCompleted(
      std::vector<LatencyInfo> latency_info,
      bool top_controls_visible_height_changed = false);

 private:
  enum class InputMetricEvent {
    SCROLL_BEGIN_TOUCH = 0,
    SCROLL_UPDATE_TOUCH,
    SCROLL_BEGIN_WHEEL,
    SCROLL_UPDATE_WHEEL,

    INPUT_METRIC_EVENT_MAX = SCROLL_UPDATE_WHEEL
  };

  // Data holder for all intermediate state for jank tracking.
  struct JankTrackerState {
    int total_update_events_ = 0;
    int janky_update_events_ = 0;
    bool prev_scroll_update_reported_ = false;
    base::TimeDelta prev_duration_;
    base::TimeDelta total_update_duration_;
    base::TimeDelta janky_update_duration_;
  };
  JankTrackerState jank_state_;

  void ReportUkmScrollLatency(
      const InputMetricEvent& metric_event,
      base::TimeTicks start_timestamp,
      base::TimeTicks time_to_scroll_update_swap_begin_timestamp,
      base::TimeTicks time_to_handled_timestamp,
      bool is_main_thread,
      const ukm::SourceId ukm_source_id);

  void ComputeEndToEndLatencyHistograms(
      base::TimeTicks gpu_swap_begin_timestamp,
      base::TimeTicks gpu_swap_end_timestamp,
      const LatencyInfo& latency,
      bool top_controls_visible_height_changed);

  void ReportJankyFrame(base::TimeTicks gpu_swap_begin_timestamp,
                        base::TimeTicks gpu_swap_end_timestamp,
                        const LatencyInfo& latency,
                        bool first_frame);
};

}  // namespace latency

#endif  // UI_LATENCY_LATENCY_TRACKER_H_
