// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_LATENCY_TRACKER_H_
#define UI_LATENCY_LATENCY_TRACKER_H_

#include "base/macros.h"
#include "ui/latency/latency_info.h"

namespace ui {

// Utility class for tracking the latency of events. Relies on LatencyInfo
// components logged by content::RenderWidgetHostLatencyTracker.
class LatencyTracker {
 public:
  explicit LatencyTracker(bool metric_sampling);
  ~LatencyTracker() = default;

  // Terminates latency tracking for events that triggered rendering, also
  // performing relevant UMA latency reporting.
  // Called when GPU buffers swap completes.
  void OnGpuSwapBuffersCompleted(const LatencyInfo& latency);

 protected:
  virtual void ReportRapporScrollLatency(
      const std::string& name,
      const LatencyInfo::LatencyComponent& start_component,
      const LatencyInfo::LatencyComponent& end_component);

 private:
  void ReportUkmScrollLatency(
      const std::string& event_name,
      const std::string& metric_name,
      const LatencyInfo::LatencyComponent& start_component,
      const LatencyInfo::LatencyComponent& end_component,
      const ukm::SourceId ukm_source_id);

  void ComputeEndToEndLatencyHistograms(
      const LatencyInfo::LatencyComponent& gpu_swap_begin_component,
      const LatencyInfo::LatencyComponent& gpu_swap_end_component,
      const LatencyInfo& latency);

  // Whether the sampling is needed for high volume metrics. This will be off
  // when we are in unit tests. This is a temporary field so we can come up with
  // a more permanent solution for crbug.com/739169.
  bool metric_sampling_;
  int metric_sampling_events_since_last_sample_ = -1;

  DISALLOW_COPY_AND_ASSIGN(LatencyTracker);
};

}  // namespace latency

#endif  // UI_LATENCY_LATENCY_TRACKER_H_
