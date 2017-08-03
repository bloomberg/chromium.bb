// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_TAB_METRICS_COLLECTOR_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_TAB_METRICS_COLLECTOR_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_manager.h"
#include "services/resource_coordinator/public/cpp/coordination_unit_id.h"
#include "services/resource_coordinator/public/interfaces/tab_signal.mojom.h"

namespace resource_coordinator {

class CoordinationUnitImpl;
class WebContentsCoordinationUnitImpl;

// The TabMetricsCollector is a dedicated |CoordinationUnitGraphObserver| for
// aggregating tab-related metrics for reporting purposes.
class TabMetricsCollector : public CoordinationUnitGraphObserver {
 public:
  TabMetricsCollector();
  ~TabMetricsCollector() override;

  // CoordinationUnitGraphObserver implementation.
  void OnBeforeCoordinationUnitDestroyed(
      const CoordinationUnitImpl* coordination_unit) override;
  void OnPropertyChanged(const CoordinationUnitImpl* coordination_unit,
                         const mojom::PropertyType property_type,
                         const base::Value& value) override;
  bool ShouldObserve(const CoordinationUnitImpl* coordination_unit) override;

 private:
  struct UkmCPUUsageCollectionState {
    size_t num_cpu_usage_measurements = 0u;
    // |ukm::UkmRecorder::GetNewSourceID| monotonically increases starting at 0,
    // so -1 implies that the current |ukm_source_id| is invalid.
    ukm::SourceId ukm_source_id = -1;
  };
  bool IsCollectingCPUUsageForUkm(const CoordinationUnitID& tab_cu_id);
  void OnWebContentsPropertyChanged(
      const WebContentsCoordinationUnitImpl* web_contents_coordination_unit,
      const mojom::PropertyType property_type,
      const base::Value& value);
  void RecordCPUUsageForUkm(const CoordinationUnitID& tab_cu_id,
                            double cpu_usage,
                            size_t num_coresident_tabs);
  void UpdateUkmSourceIdForTab(const CoordinationUnitID& tab_cu_id,
                               ukm::SourceId ukm_source_id);
  void UpdateWithFieldTrialParams();

  std::map<CoordinationUnitID, UkmCPUUsageCollectionState>
      ukm_cpu_usage_collection_state_map_;
  size_t max_ukm_cpu_usage_measurements_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(TabMetricsCollector);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_TAB_METRICS_COLLECTOR_H_
