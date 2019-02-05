// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"

#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/observers/coordination_unit_graph_observer.h"
#include "services/resource_coordinator/resource_coordinator_clock.h"

namespace resource_coordinator {

FrameCoordinationUnitImpl::FrameCoordinationUnitImpl(
    const CoordinationUnitID& id,
    CoordinationUnitGraph* graph,
    std::unique_ptr<service_manager::ServiceKeepaliveRef> keepalive_ref)
    : CoordinationUnitInterface(id, graph, std::move(keepalive_ref)),
      parent_frame_coordination_unit_(nullptr),
      page_coordination_unit_(nullptr),
      process_coordination_unit_(nullptr) {
  for (size_t i = 0; i < base::size(intervention_policy_); ++i)
    intervention_policy_[i] = mojom::InterventionPolicy::kUnknown;
}

FrameCoordinationUnitImpl::~FrameCoordinationUnitImpl() {
  if (parent_frame_coordination_unit_)
    parent_frame_coordination_unit_->RemoveChildFrame(this);
  if (page_coordination_unit_)
    page_coordination_unit_->RemoveFrame(this);
  if (process_coordination_unit_)
    process_coordination_unit_->RemoveFrame(this);
  for (auto* child_frame : child_frame_coordination_units_)
    child_frame->RemoveParentFrame(this);
}

void FrameCoordinationUnitImpl::SetProcess(const CoordinationUnitID& cu_id) {
  ProcessCoordinationUnitImpl* process_cu =
      ProcessCoordinationUnitImpl::GetCoordinationUnitByID(graph_, cu_id);
  if (!process_cu)
    return;
  DCHECK(!process_coordination_unit_);
  process_coordination_unit_ = process_cu;
  process_cu->AddFrame(this);
}

void FrameCoordinationUnitImpl::AddChildFrame(const CoordinationUnitID& cu_id) {
  DCHECK(cu_id != id());
  FrameCoordinationUnitImpl* frame_cu =
      FrameCoordinationUnitImpl::GetCoordinationUnitByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (HasFrameCoordinationUnitInAncestors(frame_cu) ||
      frame_cu->HasFrameCoordinationUnitInDescendants(this)) {
    DCHECK(false) << "Cyclic reference in frame coordination units detected!";
    return;
  }
  if (AddChildFrame(frame_cu)) {
    frame_cu->AddParentFrame(this);
  }
}

void FrameCoordinationUnitImpl::RemoveChildFrame(
    const CoordinationUnitID& cu_id) {
  DCHECK(cu_id != id());
  FrameCoordinationUnitImpl* frame_cu =
      FrameCoordinationUnitImpl::GetCoordinationUnitByID(graph_, cu_id);
  if (!frame_cu)
    return;
  if (RemoveChildFrame(frame_cu)) {
    frame_cu->RemoveParentFrame(this);
  }
}

void FrameCoordinationUnitImpl::SetNetworkAlmostIdle(bool idle) {
  SetProperty(mojom::PropertyType::kNetworkAlmostIdle, idle);
}

void FrameCoordinationUnitImpl::SetLifecycleState(mojom::LifecycleState state) {
  if (state == lifecycle_state_)
    return;

  mojom::LifecycleState old_state = lifecycle_state_;
  lifecycle_state_ = state;

  // Notify parents of this change.
  if (process_coordination_unit_)
    process_coordination_unit_->OnFrameLifecycleStateChanged(this, old_state);
  if (page_coordination_unit_)
    page_coordination_unit_->OnFrameLifecycleStateChanged(this, old_state);
}

void FrameCoordinationUnitImpl::SetHasNonEmptyBeforeUnload(
    bool has_nonempty_beforeunload) {
  has_nonempty_beforeunload_ = has_nonempty_beforeunload;
}

void FrameCoordinationUnitImpl::SetInterventionPolicy(
    mojom::PolicyControlledIntervention intervention,
    mojom::InterventionPolicy policy) {
  size_t i = static_cast<size_t>(intervention);
  DCHECK_LT(i, base::size(intervention_policy_));

  // This can only be called to set a policy, but not to revert a policy to the
  // unset state.
  DCHECK_NE(mojom::InterventionPolicy::kUnknown, policy);

  // We expect intervention policies to be initially set in order, and rely on
  // that as a synchronization primitive. Ensure this is the case.
  DCHECK(i == 0 ||
         intervention_policy_[i - 1] != mojom::InterventionPolicy::kUnknown);

  if (policy == intervention_policy_[i])
    return;
  // Only notify of actual changes.
  mojom::InterventionPolicy old_policy = intervention_policy_[i];
  intervention_policy_[i] = policy;
  if (auto* page_cu = GetPageCoordinationUnit()) {
    page_cu->OnFrameInterventionPolicyChanged(this, intervention, old_policy,
                                              policy);
  }
}

void FrameCoordinationUnitImpl::OnNonPersistentNotificationCreated() {
  SendEvent(mojom::Event::kNonPersistentNotificationCreated);
}

FrameCoordinationUnitImpl*
FrameCoordinationUnitImpl::GetParentFrameCoordinationUnit() const {
  return parent_frame_coordination_unit_;
}

PageCoordinationUnitImpl* FrameCoordinationUnitImpl::GetPageCoordinationUnit()
    const {
  return page_coordination_unit_;
}

ProcessCoordinationUnitImpl*
FrameCoordinationUnitImpl::GetProcessCoordinationUnit() const {
  return process_coordination_unit_;
}

bool FrameCoordinationUnitImpl::IsMainFrame() const {
  return !parent_frame_coordination_unit_;
}

bool FrameCoordinationUnitImpl::AreAllInterventionPoliciesSet() const {
  // The convention is that policies are first set en masse, in order. So if
  // the last policy is set then they are all considered to be set. Check this
  // in DEBUG builds.
#if DCHECK_IS_ON()
  bool seen_unset_policy = false;
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    if (!seen_unset_policy) {
      seen_unset_policy =
          intervention_policy_[i] != mojom::InterventionPolicy::kUnknown;
    } else {
      // Once a first unset policy is seen, all subsequent policies must be
      // unset.
      DCHECK_NE(mojom::InterventionPolicy::kUnknown, intervention_policy_[i]);
    }
  }
#endif

  return intervention_policy_[base::size(intervention_policy_) - 1] !=
         mojom::InterventionPolicy::kUnknown;
}  // namespace resource_coordinator

void FrameCoordinationUnitImpl::SetAllInterventionPoliciesForTesting(
    mojom::InterventionPolicy policy) {
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    SetInterventionPolicy(static_cast<mojom::PolicyControlledIntervention>(i),
                          policy);
  }
}

void FrameCoordinationUnitImpl::OnEventReceived(mojom::Event event) {
  for (auto& observer : observers())
    observer.OnFrameEventReceived(this, event);
}

void FrameCoordinationUnitImpl::OnPropertyChanged(
    mojom::PropertyType property_type,
    int64_t value) {
  for (auto& observer : observers())
    observer.OnFramePropertyChanged(this, property_type, value);
}

bool FrameCoordinationUnitImpl::HasFrameCoordinationUnitInAncestors(
    FrameCoordinationUnitImpl* frame_cu) const {
  if (parent_frame_coordination_unit_ == frame_cu ||
      (parent_frame_coordination_unit_ &&
       parent_frame_coordination_unit_->HasFrameCoordinationUnitInAncestors(
           frame_cu))) {
    return true;
  }
  return false;
}

bool FrameCoordinationUnitImpl::HasFrameCoordinationUnitInDescendants(
    FrameCoordinationUnitImpl* frame_cu) const {
  for (FrameCoordinationUnitImpl* child : child_frame_coordination_units_) {
    if (child == frame_cu ||
        child->HasFrameCoordinationUnitInDescendants(frame_cu)) {
      return true;
    }
  }
  return false;
}

void FrameCoordinationUnitImpl::AddParentFrame(
    FrameCoordinationUnitImpl* parent_frame_cu) {
  parent_frame_coordination_unit_ = parent_frame_cu;
}

bool FrameCoordinationUnitImpl::AddChildFrame(
    FrameCoordinationUnitImpl* child_frame_cu) {
  return child_frame_coordination_units_.count(child_frame_cu)
             ? false
             : child_frame_coordination_units_.insert(child_frame_cu).second;
}

void FrameCoordinationUnitImpl::RemoveParentFrame(
    FrameCoordinationUnitImpl* parent_frame_cu) {
  DCHECK(parent_frame_coordination_unit_ == parent_frame_cu);
  parent_frame_coordination_unit_ = nullptr;
}

bool FrameCoordinationUnitImpl::RemoveChildFrame(
    FrameCoordinationUnitImpl* child_frame_cu) {
  return child_frame_coordination_units_.erase(child_frame_cu) > 0;
}

void FrameCoordinationUnitImpl::AddPageCoordinationUnit(
    PageCoordinationUnitImpl* page_coordination_unit) {
  DCHECK(!page_coordination_unit_);
  page_coordination_unit_ = page_coordination_unit;
}

void FrameCoordinationUnitImpl::RemovePageCoordinationUnit(
    PageCoordinationUnitImpl* page_coordination_unit) {
  DCHECK(page_coordination_unit == page_coordination_unit_);
  page_coordination_unit_ = nullptr;
}

void FrameCoordinationUnitImpl::RemoveProcessCoordinationUnit(
    ProcessCoordinationUnitImpl* process_coordination_unit) {
  DCHECK(process_coordination_unit == process_coordination_unit_);
  process_coordination_unit_ = nullptr;
}

}  // namespace resource_coordinator
