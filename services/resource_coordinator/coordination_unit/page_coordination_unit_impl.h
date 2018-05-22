// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PAGE_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PAGE_COORDINATION_UNIT_IMPL_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"

namespace resource_coordinator {

class FrameCoordinationUnitImpl;
class ProcessCoordinationUnitImpl;

class PageCoordinationUnitImpl
    : public CoordinationUnitInterface<PageCoordinationUnitImpl,
                                       mojom::PageCoordinationUnit,
                                       mojom::PageCoordinationUnitRequest> {
 public:
  static CoordinationUnitType Type() { return CoordinationUnitType::kPage; }

  PageCoordinationUnitImpl(
      const CoordinationUnitID& id,
      std::unique_ptr<service_manager::ServiceContextRef> service_ref);
  ~PageCoordinationUnitImpl() override;

  // mojom::PageCoordinationUnit implementation.
  void AddFrame(const CoordinationUnitID& cu_id) override;
  void RemoveFrame(const CoordinationUnitID& cu_id) override;
  void SetIsLoading(bool is_loading) override;
  void SetVisibility(bool visible) override;
  void SetUKMSourceId(int64_t ukm_source_id) override;
  void OnFaviconUpdated() override;
  void OnTitleUpdated() override;
  void OnMainFrameNavigationCommitted() override;

  // There is no direct relationship between processes and pages. However,
  // frames are accessible by both processes and frames, so we find all of the
  // processes that are reachable from the pages's accessible frames.
  std::set<ProcessCoordinationUnitImpl*> GetAssociatedProcessCoordinationUnits()
      const;
  bool IsVisible() const;
  double GetCPUUsage() const;

  // Returns false if can't get an expected task queueing duration successfully.
  bool GetExpectedTaskQueueingDuration(int64_t* duration);

  // Returns 0 if no navigation has happened, otherwise returns the time since
  // the last navigation commit.
  base::TimeDelta TimeSinceLastNavigation() const;

  // Returns the time since the last visibility change, it should always have a
  // value since we set the visibility property when we create a
  // PageCoordinationUnit.
  base::TimeDelta TimeSinceLastVisibilityChange() const;

  const std::set<FrameCoordinationUnitImpl*>& GetFrameCoordinationUnits()
      const {
    return frame_coordination_units_;
  }

  // Returns the main frame CU or nullptr if this page has no main frame.
  FrameCoordinationUnitImpl* GetMainFrameCoordinationUnit() const;

  // Accessors.
  base::TimeTicks usage_estimate_time() const { return usage_estimate_time_; }
  void set_usage_estimate_time(base::TimeTicks usage_estimate_time) {
    usage_estimate_time_ = usage_estimate_time;
  }
  base::TimeDelta cumulative_cpu_usage_estimate() const {
    return cumulative_cpu_usage_estimate_;
  }
  void set_cumulative_cpu_usage_estimate(
      base::TimeDelta cumulative_cpu_usage_estimate) {
    cumulative_cpu_usage_estimate_ = cumulative_cpu_usage_estimate;
  }
  uint64_t private_footprint_kb_estimate() const {
    return private_footprint_kb_estimate_;
  }
  void set_private_footprint_kb_estimate(
      uint64_t private_footprint_kb_estimate) {
    private_footprint_kb_estimate_ = private_footprint_kb_estimate;
  }

 private:
  friend class FrameCoordinationUnitImpl;

  // CoordinationUnitInterface implementation.
  void OnEventReceived(mojom::Event event) override;
  void OnPropertyChanged(mojom::PropertyType property_type,
                         int64_t value) override;

  bool AddFrame(FrameCoordinationUnitImpl* frame_cu);
  bool RemoveFrame(FrameCoordinationUnitImpl* frame_cu);

  std::set<FrameCoordinationUnitImpl*> frame_coordination_units_;

  base::TimeTicks visibility_change_time_;
  // Main frame navigation committed time.
  base::TimeTicks navigation_committed_time_;

  // The time the most recent resource usage estimate applies to.
  base::TimeTicks usage_estimate_time_;

  // The most current CPU usage estimate. Note that this estimate is most
  // generously described as "piecewise linear", as it attributes the CPU
  // cost incurred since the last measurement was made equally to pages
  // hosted by a process. If, e.g. a frame has come into existence and vanished
  // from a given process between measurements, the entire cost to that frame
  // will be mis-attributed to other frames hosted in that process.
  base::TimeDelta cumulative_cpu_usage_estimate_;
  // The most current memory footprint estimate.
  uint64_t private_footprint_kb_estimate_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PageCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PAGE_COORDINATION_UNIT_IMPL_H_
