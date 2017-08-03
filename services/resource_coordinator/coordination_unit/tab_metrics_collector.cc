// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/tab_metrics_collector.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/web_contents_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

namespace resource_coordinator {

namespace {

const size_t kDefaultMaxCPUUsageMeasurements = 30u;

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

}  // namespace

TabMetricsCollector::TabMetricsCollector()
    : max_ukm_cpu_usage_measurements_(kDefaultMaxCPUUsageMeasurements) {
  UpdateWithFieldTrialParams();
}

TabMetricsCollector::~TabMetricsCollector() = default;

void TabMetricsCollector::OnBeforeCoordinationUnitDestroyed(
    const CoordinationUnitImpl* coordination_unit) {
  if (coordination_unit->id().type == CoordinationUnitType::kWebContents) {
    ukm_cpu_usage_collection_state_map_.erase(coordination_unit->id());
  }
}

void TabMetricsCollector::OnPropertyChanged(
    const CoordinationUnitImpl* coordination_unit,
    const mojom::PropertyType property_type,
    const base::Value& value) {
  if (coordination_unit->id().type == CoordinationUnitType::kWebContents) {
    OnWebContentsPropertyChanged(
        CoordinationUnitImpl::ToWebContentsCoordinationUnit(coordination_unit),
        property_type, value);
  }
}

bool TabMetricsCollector::ShouldObserve(
    const CoordinationUnitImpl* coordination_unit) {
  return coordination_unit->id().type == CoordinationUnitType::kWebContents;
}

bool TabMetricsCollector::IsCollectingCPUUsageForUkm(
    const CoordinationUnitID& tab_cu_id) {
  UkmCPUUsageCollectionState& state =
      ukm_cpu_usage_collection_state_map_[tab_cu_id];

  return state.ukm_source_id > -1 &&
         state.num_cpu_usage_measurements < max_ukm_cpu_usage_measurements_;
}

void TabMetricsCollector::OnWebContentsPropertyChanged(
    const WebContentsCoordinationUnitImpl* web_contents_coordination_unit,
    const mojom::PropertyType property_type,
    const base::Value& value) {
  const CoordinationUnitID& web_contents_cu_id =
      web_contents_coordination_unit->id();
  if (property_type == mojom::PropertyType::kCPUUsage) {
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

    UpdateUkmSourceIdForTab(web_contents_cu_id, ukm_source_id);
  }
}

void TabMetricsCollector::RecordCPUUsageForUkm(
    const CoordinationUnitID& tab_cu_id,
    double cpu_usage,
    size_t num_coresident_tabs) {
  UkmCPUUsageCollectionState& state =
      ukm_cpu_usage_collection_state_map_[tab_cu_id];

  ukm::builders::CPUUsageMeasurement(state.ukm_source_id)
      .SetTick(state.num_cpu_usage_measurements++)
      .SetCPUUsage(cpu_usage)
      .SetNumberOfCoresidentTabs(num_coresident_tabs)
      .Record(coordination_unit_manager().ukm_recorder());
}

void TabMetricsCollector::UpdateUkmSourceIdForTab(
    const CoordinationUnitID& tab_cu_id,
    ukm::SourceId ukm_source_id) {
  UkmCPUUsageCollectionState& state =
      ukm_cpu_usage_collection_state_map_[tab_cu_id];

  state.ukm_source_id = ukm_source_id;
  // Updating the |ukm_source_id| restarts CPU usage collection.
  state.num_cpu_usage_measurements = 0u;
}

void TabMetricsCollector::UpdateWithFieldTrialParams() {
  int64_t interval_ms = GetGRCRenderProcessCPUProfilingIntervalInMs();
  int64_t duration_ms = GetGRCRenderProcessCPUProfilingDurationInMs();

  if (interval_ms > 0 && duration_ms > 0 && duration_ms >= interval_ms) {
    max_ukm_cpu_usage_measurements_ =
        static_cast<size_t>(duration_ms / interval_ms);
  }
}

}  // namespace resource_coordinator
