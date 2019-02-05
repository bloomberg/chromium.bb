// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/coordination_unit_base.h"

namespace resource_coordinator {

class PageCoordinationUnitImpl;
class ProcessCoordinationUnitImpl;

// Frame Coordination Units form a tree structure, each FrameCoordinationUnit at
// most has one parent that is a FrameCoordinationUnit.
// A Frame Coordination Unit will have parents only if navigation committed.
class FrameCoordinationUnitImpl
    : public CoordinationUnitInterface<FrameCoordinationUnitImpl,
                                       mojom::FrameCoordinationUnit,
                                       mojom::FrameCoordinationUnitRequest> {
 public:
  static CoordinationUnitType Type() { return CoordinationUnitType::kFrame; }

  FrameCoordinationUnitImpl(
      const CoordinationUnitID& id,
      CoordinationUnitGraph* graph,
      std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref);
  ~FrameCoordinationUnitImpl() override;

  // FrameCoordinationUnit implementation.
  void SetProcess(const CoordinationUnitID& cu_id) override;
  void AddChildFrame(const CoordinationUnitID& cu_id) override;
  void RemoveChildFrame(const CoordinationUnitID& cu_id) override;
  void SetNetworkAlmostIdle(bool idle) override;
  void SetLifecycleState(mojom::LifecycleState state) override;
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) override;
  void SetInterventionPolicy(mojom::PolicyControlledIntervention intervention,
                             mojom::InterventionPolicy policy) override;
  void OnNonPersistentNotificationCreated() override;

  FrameCoordinationUnitImpl* GetParentFrameCoordinationUnit() const;
  PageCoordinationUnitImpl* GetPageCoordinationUnit() const;
  ProcessCoordinationUnitImpl* GetProcessCoordinationUnit() const;
  bool IsMainFrame() const;

  mojom::LifecycleState lifecycle_state() const { return lifecycle_state_; }
  bool has_nonempty_beforeunload() const { return has_nonempty_beforeunload_; }

  // Returns true if all intervention policies have been set for this frame.
  bool AreAllInterventionPoliciesSet() const;

  const std::set<FrameCoordinationUnitImpl*>&
  child_frame_coordination_units_for_testing() const {
    return child_frame_coordination_units_;
  }

  // Sets the same policy for all intervention types in this frame. Causes
  // Page::OnFrameInterventionPolicyChanged to be invoked.
  void SetAllInterventionPoliciesForTesting(mojom::InterventionPolicy policy);

 private:
  friend class PageCoordinationUnitImpl;
  friend class ProcessCoordinationUnitImpl;

  // CoordinationUnitInterface implementation.
  void OnEventReceived(mojom::Event event) override;
  void OnPropertyChanged(mojom::PropertyType property_type,
                         int64_t value) override;

  bool HasFrameCoordinationUnitInAncestors(
      FrameCoordinationUnitImpl* frame_cu) const;
  bool HasFrameCoordinationUnitInDescendants(
      FrameCoordinationUnitImpl* frame_cu) const;

  // The following methods will be called by other FrameCoordinationUnitImpl,
  // PageCoordinationUnitImpl and ProcessCoordinationUnitImpl respectively to
  // manipulate their relationship.
  void AddParentFrame(FrameCoordinationUnitImpl* parent_frame_cu);
  bool AddChildFrame(FrameCoordinationUnitImpl* child_frame_cu);
  void RemoveParentFrame(FrameCoordinationUnitImpl* parent_frame_cu);
  bool RemoveChildFrame(FrameCoordinationUnitImpl* child_frame_cu);
  void AddPageCoordinationUnit(PageCoordinationUnitImpl* page_cu);
  void AddProcessCoordinationUnit(ProcessCoordinationUnitImpl* process_cu);
  void RemovePageCoordinationUnit(PageCoordinationUnitImpl* page_cu);
  void RemoveProcessCoordinationUnit(ProcessCoordinationUnitImpl* process_cu);

  FrameCoordinationUnitImpl* parent_frame_coordination_unit_;
  PageCoordinationUnitImpl* page_coordination_unit_;
  ProcessCoordinationUnitImpl* process_coordination_unit_;
  std::set<FrameCoordinationUnitImpl*> child_frame_coordination_units_;

  mojom::LifecycleState lifecycle_state_ = mojom::LifecycleState::kRunning;
  bool has_nonempty_beforeunload_ = false;

  // Intervention policy for this frame. These are communicated from the
  // renderer process and are controlled by origin trials.
  mojom::InterventionPolicy intervention_policy_
      [static_cast<size_t>(mojom::PolicyControlledIntervention::kMaxValue) + 1];

  DISALLOW_COPY_AND_ASSIGN(FrameCoordinationUnitImpl);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_FRAME_COORDINATION_UNIT_IMPL_H_
