// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/metrics_collector.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
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

constexpr base::TimeDelta kMaxAudioSlientTime = base::TimeDelta::FromMinutes(1);

const char kTabFromBackgroundedToFirstAudioStartsUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstAudioStarts";

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

void MetricsCollector::OnBeforeCoordinationUnitDestroyed(
    const CoordinationUnitImpl* coordination_unit) {
  if (coordination_unit->id().type == CoordinationUnitType::kFrame) {
    frame_data_map_.erase(coordination_unit->id());
  } else if (coordination_unit->id().type ==
             CoordinationUnitType::kWebContents) {
    metrics_report_record_map_.erase(coordination_unit->id());
    ukm_cpu_usage_collection_state_map_.erase(coordination_unit->id());
  }
}

void MetricsCollector::OnFramePropertyChanged(
    const FrameCoordinationUnitImpl* frame_coordination_unit,
    const mojom::PropertyType property_type,
    const base::Value& value) {
  FrameData& frame_data = frame_data_map_[frame_coordination_unit->id()];
  if (property_type == mojom::PropertyType::kAudible) {
    bool audible = value.GetBool();
    if (!audible) {
      frame_data.last_audible_time = clock_->NowTicks();
      return;
    }
    auto* web_contents_coordination_unit =
        frame_coordination_unit->GetWebContentsCoordinationUnit();
    // Only record metrics while it is backgrounded.
    if (!web_contents_coordination_unit ||
        web_contents_coordination_unit->IsVisible())
      return;
    // Audio is considered to have started playing if the tab has never
    // previously played audio, or has been silent for at least one minute.
    auto now = clock_->NowTicks();
    if (frame_data.last_audible_time + kMaxAudioSlientTime < now) {
      MetricsReportRecord& record =
          metrics_report_record_map_[web_contents_coordination_unit->id()];
      const WebContentsData web_contents_data =
          web_contents_data_map_[web_contents_coordination_unit->id()];
      if (!record.first_audible_after_backgrounded_reported) {
        HEURISTICS_HISTOGRAM(kTabFromBackgroundedToFirstAudioStartsUMA,
                             now - web_contents_data.last_invisible_time);
        record.first_audible_after_backgrounded_reported = true;
      }
    }
  }
}

void MetricsCollector::OnWebContentsPropertyChanged(
    const WebContentsCoordinationUnitImpl* web_contents_coordination_unit,
    const mojom::PropertyType property_type,
    const base::Value& value) {
  const auto web_contents_cu_id = web_contents_coordination_unit->id();
  if (property_type == mojom::PropertyType::kVisible) {
    if (value.GetBool()) {
      // The web contents becomes visible again, clear all record in order to
      // report metrics when web contents becomes invisible next time.
      ResetMetricsReportRecord(web_contents_coordination_unit->id());
      return;
    }
    web_contents_data_map_[web_contents_cu_id].last_invisible_time =
        clock_->NowTicks();
  } else if (property_type == mojom::PropertyType::kCPUUsage) {
    if (IsCollectingCPUUsageForUkm(web_contents_cu_id)) {
      RecordCPUUsageForUkm(
          web_contents_cu_id, value.GetDouble(),
          GetNumCoresidentTabs(web_contents_coordination_unit));
    }
  } else if (property_type == mojom::PropertyType::kUkmSourceId) {
    ukm::SourceId ukm_source_id;
    // |mojom::PropertyType::kUkmSourceId| is stored as a string because
    // |ukm::SourceId is not supported by the coordination unit property
    // store, so it must  be converted before being used.
    bool success = base::StringToInt64(value.GetString(), &ukm_source_id);
    DCHECK(success);

    UpdateUkmSourceIdForWebContents(web_contents_cu_id, ukm_source_id);
  }
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
  auto metrics_report_record_iter = metrics_report_record_map_.find(cu_id);
  if (metrics_report_record_iter == metrics_report_record_map_.end())
    return;
  metrics_report_record_iter->second.Reset();
}

MetricsCollector::MetricsReportRecord::MetricsReportRecord()
    : first_audible_after_backgrounded_reported(false) {}

void MetricsCollector::MetricsReportRecord::Reset() {
  first_audible_after_backgrounded_reported = false;
}

}  // namespace resource_coordinator
