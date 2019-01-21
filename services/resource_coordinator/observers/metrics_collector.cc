// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/observers/metrics_collector.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"

namespace resource_coordinator {

// Delay the metrics report from GRC to UMA/UKM for 5 minutes from when the main
// frame navigation is committed.
const base::TimeDelta kMetricsReportDelayTimeout =
    base::TimeDelta::FromMinutes(5);

const char kTabFromBackgroundedToFirstFaviconUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstFaviconUpdated";
const char kTabFromBackgroundedToFirstTitleUpdatedUMA[] =
    "TabManager.Heuristics.FromBackgroundedToFirstTitleUpdated";
const char kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA[] =
    "TabManager.Heuristics."
    "FromBackgroundedToFirstNonPersistentNotificationCreated";

const int kDefaultFrequencyUkmEQTReported = 5u;

// Gets the number of tabs that are co-resident in all of the render processes
// associated with a |CoordinationUnitType::kPage| coordination unit.
size_t GetNumCoresidentTabs(const PageCoordinationUnitImpl* page_cu) {
  std::set<CoordinationUnitBase*> coresident_tabs;
  for (auto* process_cu : page_cu->GetAssociatedProcessCoordinationUnits()) {
    for (auto* associated_page_cu :
         process_cu->GetAssociatedPageCoordinationUnits()) {
      coresident_tabs.insert(associated_page_cu);
    }
  }
  // A tab cannot be co-resident with itself.
  return coresident_tabs.size() - 1;
}

MetricsCollector::MetricsCollector() {
  UpdateWithFieldTrialParams();
}

MetricsCollector::~MetricsCollector() = default;

bool MetricsCollector::ShouldObserve(
    const CoordinationUnitBase* coordination_unit) {
  return coordination_unit->id().type == CoordinationUnitType::kFrame ||
         coordination_unit->id().type == CoordinationUnitType::kPage ||
         coordination_unit->id().type == CoordinationUnitType::kProcess;
}

void MetricsCollector::OnCoordinationUnitCreated(
    const CoordinationUnitBase* coordination_unit) {
  if (coordination_unit->id().type == CoordinationUnitType::kPage) {
    metrics_report_record_map_.emplace(coordination_unit->id(),
                                       MetricsReportRecord());
  }
}

void MetricsCollector::OnBeforeCoordinationUnitDestroyed(
    const CoordinationUnitBase* coordination_unit) {
  if (coordination_unit->id().type == CoordinationUnitType::kPage) {
    metrics_report_record_map_.erase(coordination_unit->id());
    ukm_collection_state_map_.erase(coordination_unit->id());
  }
}

void MetricsCollector::OnPagePropertyChanged(
    const PageCoordinationUnitImpl* page_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  const auto page_cu_id = page_cu->id();
  if (property_type == mojom::PropertyType::kVisible) {
    if (value) {
      // The page becomes visible again, clear all records in order to
      // report metrics when page becomes invisible next time.
      ResetMetricsReportRecord(page_cu_id);
      return;
    }
  } else if (property_type == mojom::PropertyType::kUKMSourceId) {
    ukm::SourceId ukm_source_id = value;
    UpdateUkmSourceIdForPage(page_cu_id, ukm_source_id);
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu_id)->second;
    record.UpdateUKMSourceID(ukm_source_id);
  }
}

void MetricsCollector::OnProcessPropertyChanged(
    const ProcessCoordinationUnitImpl* process_cu,
    const mojom::PropertyType property_type,
    int64_t value) {
  if (property_type == mojom::PropertyType::kExpectedTaskQueueingDuration) {
    for (auto* page_cu : process_cu->GetAssociatedPageCoordinationUnits()) {
      if (IsCollectingExpectedQueueingTimeForUkm(page_cu->id())) {
        int64_t expected_queueing_time;
        if (!page_cu->GetExpectedTaskQueueingDuration(&expected_queueing_time))
          continue;

        RecordExpectedQueueingTimeForUkm(page_cu->id(), expected_queueing_time);
      }
    }
  }
}

void MetricsCollector::OnFrameEventReceived(
    const FrameCoordinationUnitImpl* frame_cu,
    const mojom::Event event) {
  if (event == mojom::Event::kNonPersistentNotificationCreated) {
    auto* page_cu = frame_cu->GetPageCoordinationUnit();
    // Only record metrics while it is backgrounded.
    if (!page_cu || page_cu->IsVisible() || !ShouldReportMetrics(page_cu)) {
      return;
    }
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu->id())->second;
    record.first_non_persistent_notification_created.OnSignalReceived(
        frame_cu->IsMainFrame(), page_cu->TimeSinceLastVisibilityChange(),
        coordination_unit_graph().ukm_recorder());
  }
}

void MetricsCollector::OnPageEventReceived(
    const PageCoordinationUnitImpl* page_cu,
    const mojom::Event event) {
  if (event == mojom::Event::kTitleUpdated) {
    // Only record metrics while it is backgrounded.
    if (page_cu->IsVisible() || !ShouldReportMetrics(page_cu))
      return;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu->id())->second;
    record.first_title_updated.OnSignalReceived(
        true, page_cu->TimeSinceLastVisibilityChange(),
        coordination_unit_graph().ukm_recorder());
  } else if (event == mojom::Event::kFaviconUpdated) {
    // Only record metrics while it is backgrounded.
    if (page_cu->IsVisible() || !ShouldReportMetrics(page_cu))
      return;
    MetricsReportRecord& record =
        metrics_report_record_map_.find(page_cu->id())->second;
    record.first_favicon_updated.OnSignalReceived(
        true, page_cu->TimeSinceLastVisibilityChange(),
        coordination_unit_graph().ukm_recorder());
  }
}

bool MetricsCollector::ShouldReportMetrics(
    const PageCoordinationUnitImpl* page_cu) {
  return page_cu->TimeSinceLastNavigation() > kMetricsReportDelayTimeout;
}

bool MetricsCollector::IsCollectingExpectedQueueingTimeForUkm(
    const CoordinationUnitID& page_cu_id) {
  UkmCollectionState& state = ukm_collection_state_map_[page_cu_id];
  return state.ukm_source_id != ukm::kInvalidSourceId &&
         ++state.num_unreported_eqt_measurements >= frequency_ukm_eqt_reported_;
}

void MetricsCollector::RecordExpectedQueueingTimeForUkm(
    const CoordinationUnitID& page_cu_id,
    int64_t expected_queueing_time) {
  UkmCollectionState& state = ukm_collection_state_map_[page_cu_id];
  state.num_unreported_eqt_measurements = 0u;
  ukm::builders::ResponsivenessMeasurement(state.ukm_source_id)
      .SetExpectedTaskQueueingDuration(expected_queueing_time)
      .Record(coordination_unit_graph().ukm_recorder());
}

void MetricsCollector::UpdateUkmSourceIdForPage(
    const CoordinationUnitID& page_cu_id,
    ukm::SourceId ukm_source_id) {
  UkmCollectionState& state = ukm_collection_state_map_[page_cu_id];

  state.ukm_source_id = ukm_source_id;
  // Updating the |ukm_source_id| restarts usage collection.
  state.num_unreported_eqt_measurements = 0u;
}

void MetricsCollector::UpdateWithFieldTrialParams() {
  frequency_ukm_eqt_reported_ = base::GetFieldTrialParamByFeatureAsInt(
      ukm::kUkmFeature, "FrequencyUKMExpectedQueueingTime",
      kDefaultFrequencyUkmEQTReported);
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
  first_favicon_updated.SetUKMSourceID(ukm_source_id);
  first_non_persistent_notification_created.SetUKMSourceID(ukm_source_id);
  first_title_updated.SetUKMSourceID(ukm_source_id);
}

void MetricsCollector::MetricsReportRecord::Reset() {
  first_favicon_updated.Reset();
  first_non_persistent_notification_created.Reset();
  first_title_updated.Reset();
}

}  // namespace resource_coordinator
