// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/metrics_collector.h"

#include "base/metrics/histogram_macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_manager.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

// Tabs can be kept in the background for a long time, metrics show 75th
// percentile of time spent in background is 2.5 hours, and the 95th is 24 hour.
// In order to guide the selection of an appropriate observation window we are
// proposing using a CUSTOM_TIMES histogram from 1s to 48h, with 100 buckets.
#define HEURISTICS_HISTOGRAM(name, sample)                                  \
  UMA_HISTOGRAM_CUSTOM_TIMES(name, sample, base::TimeDelta::FromSeconds(1), \
                             base::TimeDelta::FromHours(48), 100)

namespace resource_coordinator {

const size_t kDefaultMaxCPUUsageMeasurements = 30u;

// Audio is considered to have started playing if the tab has never
// previously played audio, or has been silent for at least one minute.
const base::TimeDelta kMaxAudioSlientTimeout = base::TimeDelta::FromMinutes(1);
// Delay the metrics report from GRC to UMA/UKM for 5 minutes from when the main
// frame navigation is committed.
const base::TimeDelta kMetricsReportDelayTimeout =
    base::TimeDelta::FromMinutes(5);

const char kTabFromBackgroundedToFirstAlertFiredUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstAlertFired";
const char kTabFromBackgroundedToFirstAudioStartsUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstAudioStarts";
const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstFaviconUpdated";
const char kTabFromBackgroundedToFirstTitleUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstTitleUpdated";
const char kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[] =
    "TabManager.Heuristics."
    "FromBackgroundedToFirstNonPersistentNotificationCreated";

// Gets the number of tabs that are co-resident in all of the render processes
// associated with a |CoordinationUnitType::kWebContents| coordination unit.
size_t GetNumCoresidentTabs(const CoordinationUnitImpl* coordination_unit) {
  DCHECK_EQ(CoordinationUnitType::kWebContents, coordination_unit->id().type);
  std::set<CoordinationUnitImpl*> coresident_tabs;
  for (auto* process_coordination_unit :
       coordination_unit->GetAssociatedCoordinationUnitsOfType(
           CoordinationUnitType::kProcess)) {
    for (auto* tab_coordination_unit :
         process_coordination_unit->GetAssociatedCoordinationUnitsOfType(
             CoordinationUnitType::kWebContents)) {
      coresident_tabs.insert(tab_coordination_unit);
    }
  }
  // A tab cannot be co-resident with itself.
  return coresident_tabs.size() - 1;
}

MetricsCollector::MetricsCollector()
    : clock_(&default_tick_clock_),
      max_ukm_cpu_usage_measurements_(kDefaultMaxCPUUsageMeasurements) {
  UpdateWithFieldTrialParams();
}

MetricsCollector::~MetricsCollector() = default;

bool MetricsCollector::ShouldObserve(
    const CoordinationUnitImpl* coordination_unit) {
  return coordination_unit->id().type == CoordinationUnitType::kFrame ||
         coordination_unit->id().type == CoordinationUnitType::kWebContents;
}

void MetricsCollector::OnCoordinationUnitCreated(
    const CoordinationUnitImpl* coordination_unit) {
  if (coordination_unit->id().type == CoordinationUnitType::kWebContents) {
    metrics_report_record_map_.emplace(coordination_unit->id(),
                                       MetricsReportRecord());
  }
}

void MetricsCollector::OnBeforeCoordinationUnitDestroyed(
    const CoordinationUnitImpl* coordination_unit) {
  if (coordination_unit->id().type == CoordinationUnitType::kFrame) {
    frame_data_map_.erase(coordination_unit->id());
  } else if (coordination_unit->id().type ==
             CoordinationUnitType::kWebContents) {
    web_contents_data_map_.erase(coordination_unit->id());
    metrics_report_record_map_.erase(coordination_unit->id());
    ukm_cpu_usage_collection_state_map_.erase(coordination_unit->id());
  }
}

void MetricsCollector::OnFramePropertyChanged(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  FrameData& frame_data = frame_data_map_[frame_cu->id()];
  if (property_type == mojom::PropertyType::kAudible) {
    bool audible = static_cast<bool>(value);
    if (!audible) {
      frame_data.last_audible_time = clock_->NowTicks();
      return;
    }
    auto* web_contents_cu = frame_cu->GetWebContentsCoordinationUnit();
    // Only record metrics while it is backgrounded.
    if (!web_contents_cu || web_contents_cu->IsVisible() ||
        !ShouldReportMetrics(web_contents_cu)) {
      return;
    }
    // Audio is considered to have started playing if the tab has never
    // previously played audio, or has been silent for at least one minute.
    auto now = clock_->NowTicks();
    if (frame_data.last_audible_time + kMaxAudioSlientTimeout < now) {
      MetricsReportRecord& record =
          metrics_report_record_map_.find(web_contents_cu->id())->second;
      auto duration =
          now -
          web_contents_data_map_[web_contents_cu->id()].last_invisible_time;
      record.first_audible.OnSignalReceived(
          frame_cu->IsMainFrame(), duration,
          coordination_unit_manager().ukm_recorder());
    }
  }
}

void MetricsCollector::OnWebContentsPropertyChanged(
    const WebContentsCoordinationUnitImpl* web_contents_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  const auto web_contents_cu_id = web_contents_cu->id();
  if (property_type == mojom::PropertyType::kVisible) {
    if (value) {
      // The web contents becomes visible again, clear all record in order to
      // report metrics when web contents becomes invisible next time.
      ResetMetricsReportRecord(web_contents_cu_id);
      return;
    }
    web_contents_data_map_[web_contents_cu_id].last_invisible_time =
        clock_->NowTicks();
  } else if (property_type == mojom::PropertyType::kCPUUsage) {
    if (IsCollectingCPUUsageForUkm(web_contents_cu_id)) {
      RecordCPUUsageForUkm(web_contents_cu_id,
                           static_cast<double>(value) / 1000,
                           GetNumCoresidentTabs(web_contents_cu));
    }
  } else if (property_type == mojom::PropertyType::kUKMSourceId) {
    ukm::SourceId ukm_source_id = value;
    UpdateUkmSourceIdForWebContents(web_contents_cu_id, ukm_source_id);
    MetricsReportRecord& record =
        metrics_report_record_map_.find(web_contents_cu_id)->second;
    record.UpdateUKMSourceID(ukm_source_id);
  }
}

void MetricsCollector::OnFrameEventReceived(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::Event event) {
  if (event == mojom::Event::kAlertFired) {
    auto* web_contents_cu = frame_cu->GetWebContentsCoordinationUnit();
    // Only record metrics while it is backgrounded.
    if (!web_contents_cu || web_contents_cu->IsVisible() ||
        !ShouldReportMetrics(web_contents_cu)) {
      return;
    }
    auto duration =
        clock_->NowTicks() -
        web_contents_data_map_[web_contents_cu->id()].last_invisible_time;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(web_contents_cu->id())->second;
    record.first_alert_fired.OnSignalReceived(
        frame_cu->IsMainFrame(), duration,
        coordination_unit_manager().ukm_recorder());
  } else if (event == mojom::Event::kNonPersistentNotificationCreated) {
    auto* web_contents_cu = frame_cu->GetWebContentsCoordinationUnit();
    // Only record metrics while it is backgrounded.
    if (!web_contents_cu || web_contents_cu->IsVisible() ||
        !ShouldReportMetrics(web_contents_cu)) {
      return;
    }
    auto duration =
        clock_->NowTicks() -
        web_contents_data_map_[web_contents_cu->id()].last_invisible_time;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(web_contents_cu->id())->second;
    record.first_non_persistent_notification_created.OnSignalReceived(
        frame_cu->IsMainFrame(), duration,
        coordination_unit_manager().ukm_recorder());
  }
}

void MetricsCollector::OnWebContentsEventReceived(
    const WebContentsCoordinationUnitImpl* web_contents_cu,
    const mojom::Event event) {
  if (event == mojom::Event::kTitleUpdated) {
    // Only record metrics while it is backgrounded.
    if (web_contents_cu->IsVisible() || !ShouldReportMetrics(web_contents_cu))
      return;
    auto duration =
        clock_->NowTicks() -
        web_contents_data_map_[web_contents_cu->id()].last_invisible_time;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(web_contents_cu->id())->second;
    record.first_title_updated.OnSignalReceived(
        true, duration, coordination_unit_manager().ukm_recorder());
  } else if (event == mojom::Event::kFaviconUpdated) {
    // Only record metrics while it is backgrounded.
    if (web_contents_cu->IsVisible() || !ShouldReportMetrics(web_contents_cu))
      return;
    auto duration =
        clock_->NowTicks() -
        web_contents_data_map_[web_contents_cu->id()].last_invisible_time;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(web_contents_cu->id())->second;
    record.first_favicon_updated.OnSignalReceived(
        true, duration, coordination_unit_manager().ukm_recorder());
  } else if (event == mojom::Event::kNavigationCommitted) {
    web_contents_data_map_[web_contents_cu->id()].navigation_finished_time =
        clock_->NowTicks();
  }
}

bool MetricsCollector::ShouldReportMetrics(
    const WebContentsCoordinationUnitImpl* web_contents_cu) {
  return clock_->NowTicks() - web_contents_data_map_[web_contents_cu->id()]
                                  .navigation_finished_time >
         kMetricsReportDelayTimeout;
}

bool MetricsCollector::IsCollectingCPUUsageForUkm(
    const CoordinationUnitID& web_contents_cu_id) {
  UkmCPUUsageCollectionState& state =
      ukm_cpu_usage_collection_state_map_[web_contents_cu_id];

  return state.ukm_source_id > -1 &&
         state.num_cpu_usage_measurements < max_ukm_cpu_usage_measurements_;
}

void MetricsCollector::RecordCPUUsageForUkm(
    const CoordinationUnitID& web_contents_cu_id,
    double cpu_usage,
    size_t num_coresident_tabs) {
  UkmCPUUsageCollectionState& state =
      ukm_cpu_usage_collection_state_map_[web_contents_cu_id];

  ukm::builders::CPUUsageMeasurement(state.ukm_source_id)
      .SetTick(state.num_cpu_usage_measurements++)
      .SetCPUUsage(cpu_usage)
      .SetNumberOfCoresidentTabs(num_coresident_tabs)
      .Record(coordination_unit_manager().ukm_recorder());
}

void MetricsCollector::UpdateUkmSourceIdForWebContents(
    const CoordinationUnitID& web_contents_cu_id,
    ukm::SourceId ukm_source_id) {
  UkmCPUUsageCollectionState& state =
      ukm_cpu_usage_collection_state_map_[web_contents_cu_id];

  state.ukm_source_id = ukm_source_id;
  // Updating the |ukm_source_id| restarts CPU usage collection.
  state.num_cpu_usage_measurements = 0u;
}

void MetricsCollector::UpdateWithFieldTrialParams() {
  int64_t interval_ms = GetGRCRenderProcessCPUProfilingIntervalInMs();
  int64_t duration_ms = GetGRCRenderProcessCPUProfilingDurationInMs();

  if (interval_ms > 0 && duration_ms > 0 && duration_ms >= interval_ms) {
    max_ukm_cpu_usage_measurements_ =
        static_cast<size_t>(duration_ms / interval_ms);
  }
}

void MetricsCollector::ResetMetricsReportRecord(CoordinationUnitID cu_id) {
  DCHECK(metrics_report_record_map_.find(cu_id) !=
         metrics_report_record_map_.end());
  metrics_report_record_map_.find(cu_id)->second.Reset();
}

MetricsCollector::MetricsReportRecord::MetricsReportRecord() = default;

MetricsCollector::MetricsReportRecord::MetricsReportRecord(
    const MetricsReportRecord& other) = default;

void MetricsCollector::MetricsReportRecord::UpdateUKMSourceID(
    int64_t ukm_source_id) {
  first_alert_fired.SetUKMSourceID(ukm_source_id);
  first_audible.SetUKMSourceID(ukm_source_id);
  first_favicon_updated.SetUKMSourceID(ukm_source_id);
  first_non_persistent_notification_created.SetUKMSourceID(ukm_source_id);
  first_title_updated.SetUKMSourceID(ukm_source_id);
}

void MetricsCollector::MetricsReportRecord::Reset() {
  first_alert_fired.Reset();
  first_audible.Reset();
  first_favicon_updated.Reset();
  first_non_persistent_notification_created.Reset();
  first_title_updated.Reset();
}

}  // namespace resource_coordinator
