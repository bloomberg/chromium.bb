// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_METRICS_COLLECTOR_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_METRICS_COLLECTOR_H_

#include "base/macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"

namespace resource_coordinator {

class CoordinationUnitImpl;
class FrameCoordinationUnitImpl;
class WebContentsCoordinationUnitImpl;

extern const char kTabFromBackgroundedToFirstAlertFiredUMA[];
extern const char kTabFromBackgroundedToFirstAudioStartsUMA[];
extern const char kTabFromBackgroundedToFirstTitleUpdatedUMA[];

// A MetricsCollector observes changes happened inside CoordinationUnit Graph,
// and reports UMA/UKM.
class MetricsCollector : public CoordinationUnitGraphObserver {
 public:
  MetricsCollector();
  ~MetricsCollector() override;

  // CoordinationUnitGraphObserver implementation.
  bool ShouldObserve(const CoordinationUnitImpl* coordination_unit) override;
  void OnBeforeCoordinationUnitDestroyed(
      const CoordinationUnitImpl* coordination_unit) override;
  void OnFramePropertyChanged(const FrameCoordinationUnitImpl* frame_cu,
                              const mojom::PropertyType property_type,
                              int64_t value) override;
  void OnWebContentsPropertyChanged(
      const WebContentsCoordinationUnitImpl* web_contents_cu,
      const mojom::PropertyType property_type,
      int64_t value) override;
  void OnFrameEventReceived(const FrameCoordinationUnitImpl* frame_cu,
                            const mojom::Event event) override;
  void OnWebContentsEventReceived(
      const WebContentsCoordinationUnitImpl* web_contents_cu,
      const mojom::Event event) override;

 private:
  friend class MetricsCollectorTest;

  struct MetricsReportRecord {
    MetricsReportRecord();
    void Reset();
    bool first_alert_fired_after_backgrounded_reported;
    bool first_audible_after_backgrounded_reported;
    bool first_title_updated_after_backgrounded_reported;
  };

  struct FrameData {
    base::TimeTicks last_audible_time;
  };

  struct WebContentsData {
    base::TimeTicks last_invisible_time;
  };

  struct UkmCPUUsageCollectionState {
    size_t num_cpu_usage_measurements = 0u;
    // |ukm::UkmRecorder::GetNewSourceID| monotonically increases starting at 0,
    // so -1 implies that the current |ukm_source_id| is invalid.
    ukm::SourceId ukm_source_id = -1;
  };

  bool IsCollectingCPUUsageForUkm(const CoordinationUnitID& web_contents_cu_id);
  void RecordCPUUsageForUkm(const CoordinationUnitID& web_contents_cu_id,
                            double cpu_usage,
                            size_t num_coresident_tabs);
  void UpdateUkmSourceIdForWebContents(
      const CoordinationUnitID& web_contents_cu_id,
      ukm::SourceId ukm_source_id);
  void UpdateWithFieldTrialParams();
  void ResetMetricsReportRecord(CoordinationUnitID cu_id);

  // Note: |clock_| is always |&default_tick_clock_|, except during unit
  // testing.
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* const clock_;
  std::map<CoordinationUnitID, FrameData> frame_data_map_;
  std::map<CoordinationUnitID, WebContentsData> web_contents_data_map_;
  // The metrics_report_record_map_ is used to record whether a metric was
  // already reported to avoid reporting multiple metrics.
  std::map<CoordinationUnitID, MetricsReportRecord> metrics_report_record_map_;
  std::map<CoordinationUnitID, UkmCPUUsageCollectionState>
      ukm_cpu_usage_collection_state_map_;
  size_t max_ukm_cpu_usage_measurements_ = 0u;
  DISALLOW_COPY_AND_ASSIGN(MetricsCollector);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_METRICS_COLLECTOR_H_
