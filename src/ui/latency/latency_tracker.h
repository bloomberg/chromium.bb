// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_LATENCY_TRACKER_H_
#define UI_LATENCY_LATENCY_TRACKER_H_

#include <deque>
#include "base/macros.h"
#include "ui/latency/latency_info.h"

namespace ui {

// Utility class for tracking the latency of events. Relies on LatencyInfo
// components logged by content::RenderWidgetHostLatencyTracker.
class LatencyTracker {
 public:
  LatencyTracker();
  ~LatencyTracker();

  // Terminates latency tracking for events that triggered rendering, also
  // performing relevant UMA latency reporting.
  // Called when GPU buffers swap completes.
  void OnGpuSwapBuffersCompleted(const std::vector<LatencyInfo>& latency_info);
  void OnGpuSwapBuffersCompleted(const LatencyInfo& latency);

  using LatencyInfoProcessor =
      base::RepeatingCallback<void(const std::vector<ui::LatencyInfo>&)>;
  static void SetLatencyInfoProcessorForTesting(
      const LatencyInfoProcessor& processor);

 private:
  enum class InputMetricEvent {
    SCROLL_BEGIN_TOUCH = 0,
    SCROLL_UPDATE_TOUCH,
    SCROLL_BEGIN_WHEEL,
    SCROLL_UPDATE_WHEEL,

    INPUT_METRIC_EVENT_MAX = SCROLL_UPDATE_WHEEL
  };

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
      const LatencyInfo& latency);

  void CalculateAverageLag(const ui::LatencyInfo& latency,
                           base::TimeTicks gpu_swap_begin_timestamp,
                           const std::string& scroll_name);

  // Used for reporting AverageLag metrics.
  typedef struct LagData {
    LagData(const std::string& name)
        : report_time(base::TimeTicks()), lag(0), scroll_name(name) {}
    // Lag report's report_time, align with |gpu_swap_begin_time|. It should has
    // one second gap between previous report. We do not set the report_time
    // before the 1 second gap is reached.
    base::TimeTicks report_time;
    float lag;
    const std::string scroll_name;
  } LagData;

  void ReportAverageLagUma(std::unique_ptr<LagData> report);

  // Last scroll event's timestamp in the sequence, reset on ScrollBegin.
  base::TimeTicks last_event_timestamp_;
  // next_report_time is always 1 second after the newest report's report_time.
  base::TimeTicks next_report_time_;
  // This keeps track the last report_time when we report to UMA, so we can
  // calculate the report's duration by current - last. Reset on ScrollBegin.
  base::TimeTicks last_reported_time_;
  // Keeps track of last gpu_swap time, so we can end the previous unfinished
  // report on the new ScrollBegin.
  base::TimeTicks last_frame_time_;
  // Lag report that already filled in the report_time, and it will be finished
  // and report once we have an event whose timestamp is later then the
  // report_time.
  std::unique_ptr<LagData> pending_finished_lag_report_;
  // The current unfinished lag report, which doesn't reach the 1 second length
  // yet. It's report_time is null and invalid now.
  std::unique_ptr<LagData> current_lag_report_;

  DISALLOW_COPY_AND_ASSIGN(LatencyTracker);
};

}  // namespace latency

#endif  // UI_LATENCY_LATENCY_TRACKER_H_
