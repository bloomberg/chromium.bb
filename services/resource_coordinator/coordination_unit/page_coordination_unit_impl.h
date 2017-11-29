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
  void SetVisibility(bool visible) override;
  void SetUKMSourceId(int64_t ukm_source_id) override;
  void OnFaviconUpdated() override;
  void OnTitleUpdated() override;
  void OnMainFrameNavigationCommitted() override;

  // Returns true when the page is in almost idle state, and updates
  // |was_almost_idle_| to true if and only if the page was not in almost idle
  // state and is now in almost idle state.
  bool CheckAndUpdateAlmostIdleStateIfNeeded();

  // There is no direct relationship between processes and pages. However,
  // frames are accessible by both processes and frames, so we find all of the
  // processes that are reachable from the pages's accessible frames.
  std::set<ProcessCoordinationUnitImpl*> GetAssociatedProcessCoordinationUnits()
      const;
  // In order to not deliver PageAlmostIdle signal multiple times, recording the
  // previous state is needed. WasAlmostIdle() returns whether the page is
  // already in almost idle state, almost idle state will be reset when the main
  // frame navigation of non-same document navigation is committed. And will
  // only be set to true when CheckAndUpdateAlmostIdleStateIfNeeded() is called.
  bool WasAlmostIdle() const;
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

  const std::set<FrameCoordinationUnitImpl*>&
  frame_coordination_units_for_testing() const {
    return frame_coordination_units_;
  }

 private:
  friend class FrameCoordinationUnitImpl;

  // CoordinationUnitInterface implementation.
  void OnEventReceived(mojom::Event event) override;
  void OnPropertyChanged(mojom::PropertyType property_type,
                         int64_t value) override;

  bool AddFrame(FrameCoordinationUnitImpl* frame_cu);
  bool RemoveFrame(FrameCoordinationUnitImpl* frame_cu);

  // Returns the main frame CU or nullptr if this page has no main frame.
  FrameCoordinationUnitImpl* GetMainFrameCoordinationUnit();

  std::set<FrameCoordinationUnitImpl*> frame_coordination_units_;

  base::TimeTicks visibility_change_time_;
  // Main frame navigation committed time.
  base::TimeTicks navigation_committed_time_;

  // |was_almost_idle_| is only reset to false when main frame navigation of
  // non-same document navigation is committed. And will be set to true when
  // CheckAndUpdateAlmostIdleStateIfNeeded() is called.
  bool was_almost_idle_;

  DISALLOW_COPY_AND_ASSIGN(PageCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_PAGE_COORDINATION_UNIT_IMPL_H_
