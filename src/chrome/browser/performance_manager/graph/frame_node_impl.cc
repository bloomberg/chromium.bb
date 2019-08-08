// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"

#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"

namespace performance_manager {

FrameNodeImpl::FrameNodeImpl(Graph* graph,
                             ProcessNodeImpl* process_node,
                             PageNodeImpl* page_node,
                             FrameNodeImpl* parent_frame_node,
                             int frame_tree_node_id)
    : CoordinationUnitInterface(graph),
      parent_frame_node_(parent_frame_node),
      page_node_(page_node),
      process_node_(process_node),
      frame_tree_node_id_(frame_tree_node_id) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(process_node);
  DCHECK(page_node);
}

FrameNodeImpl::~FrameNodeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FrameNodeImpl::SetNetworkAlmostIdle(bool network_almost_idle) {
  network_almost_idle_.SetAndMaybeNotify(this, network_almost_idle);
}

void FrameNodeImpl::SetLifecycleState(
    resource_coordinator::mojom::LifecycleState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  lifecycle_state_.SetAndMaybeNotify(this, state);
}

void FrameNodeImpl::SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_nonempty_beforeunload_ = has_nonempty_beforeunload;
}

void FrameNodeImpl::SetInterventionPolicy(
    resource_coordinator::mojom::PolicyControlledIntervention intervention,
    resource_coordinator::mojom::InterventionPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t i = static_cast<size_t>(intervention);
  DCHECK_LT(i, base::size(intervention_policy_));

  // This can only be called to set a policy, but not to revert a policy to the
  // unset state.
  DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown, policy);

  // We expect intervention policies to be initially set in order, and rely on
  // that as a synchronization primitive. Ensure this is the case.
  DCHECK(i == 0 ||
         intervention_policy_[i - 1] !=
             resource_coordinator::mojom::InterventionPolicy::kUnknown);

  if (policy == intervention_policy_[i])
    return;
  // Only notify of actual changes.
  resource_coordinator::mojom::InterventionPolicy old_policy =
      intervention_policy_[i];
  intervention_policy_[i] = policy;
  page_node_->OnFrameInterventionPolicyChanged(this, intervention, old_policy,
                                               policy);
}

void FrameNodeImpl::OnNonPersistentNotificationCreated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observers())
    observer.OnNonPersistentNotificationCreated(this);
}

FrameNodeImpl* FrameNodeImpl::parent_frame_node() const {
  return parent_frame_node_;
}

PageNodeImpl* FrameNodeImpl::page_node() const {
  return page_node_;
}

ProcessNodeImpl* FrameNodeImpl::process_node() const {
  return process_node_;
}

int FrameNodeImpl::frame_tree_node_id() const {
  return frame_tree_node_id_;
}

const base::flat_set<FrameNodeImpl*>& FrameNodeImpl::child_frame_nodes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return child_frame_nodes_;
}

resource_coordinator::mojom::LifecycleState FrameNodeImpl::lifecycle_state()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return lifecycle_state_.value();
}

bool FrameNodeImpl::has_nonempty_beforeunload() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return has_nonempty_beforeunload_;
}

const GURL& FrameNodeImpl::url() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return url_;
}

bool FrameNodeImpl::is_current() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return is_current_.value();
}

bool FrameNodeImpl::network_almost_idle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_almost_idle_.value();
}

void FrameNodeImpl::set_url(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_ = url;
}

void FrameNodeImpl::SetIsCurrent(bool is_current) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_current_.SetAndMaybeNotify(this, is_current);

#if DCHECK_IS_ON()
  // We maintain an invariant that of all sibling nodes with the same
  // |frame_tree_node_id|, at most one may be current.
  if (is_current) {
    const base::flat_set<FrameNodeImpl*>* siblings = nullptr;
    if (parent_frame_node_) {
      siblings = &parent_frame_node_->child_frame_nodes();
    } else {
      siblings = &page_node_->main_frame_nodes();
    }

    size_t current_siblings = 0;
    for (auto* frame : *siblings) {
      if (frame->frame_tree_node_id() == frame_tree_node_id_ &&
          frame->is_current()) {
        ++current_siblings;
      }
    }
    DCHECK_EQ(1u, current_siblings);
  }
#endif
}

bool FrameNodeImpl::IsMainFrame() const {
  return !parent_frame_node_;
}

bool FrameNodeImpl::AreAllInterventionPoliciesSet() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The convention is that policies are first set en masse, in order. So if
  // the last policy is set then they are all considered to be set. Check this
  // in DEBUG builds.
#if DCHECK_IS_ON()
  bool seen_unset_policy = false;
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    if (!seen_unset_policy) {
      seen_unset_policy =
          intervention_policy_[i] !=
          resource_coordinator::mojom::InterventionPolicy::kUnknown;
    } else {
      // Once a first unset policy is seen, all subsequent policies must be
      // unset.
      DCHECK_NE(resource_coordinator::mojom::InterventionPolicy::kUnknown,
                intervention_policy_[i]);
    }
  }
#endif

  return intervention_policy_[base::size(intervention_policy_) - 1] !=
         resource_coordinator::mojom::InterventionPolicy::kUnknown;
}  // namespace performance_manager

void FrameNodeImpl::SetAllInterventionPoliciesForTesting(
    resource_coordinator::mojom::InterventionPolicy policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < base::size(intervention_policy_); ++i) {
    SetInterventionPolicy(
        static_cast<resource_coordinator::mojom::PolicyControlledIntervention>(
            i),
        policy);
  }
}

void FrameNodeImpl::AddChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->parent_frame_node());
  DCHECK_NE(this, child_frame_node);
  DCHECK(NodeInGraph(child_frame_node));
  DCHECK(!HasFrameNodeInAncestors(child_frame_node) &&
         !child_frame_node->HasFrameNodeInDescendants(this));

  bool inserted = child_frame_nodes_.insert(child_frame_node).second;
  DCHECK(inserted);
}

void FrameNodeImpl::RemoveChildFrame(FrameNodeImpl* child_frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_frame_node);
  DCHECK_EQ(this, child_frame_node->parent_frame_node());
  DCHECK_NE(this, child_frame_node);
  DCHECK(NodeInGraph(child_frame_node));

  size_t removed = child_frame_nodes_.erase(child_frame_node);
  DCHECK_EQ(1u, removed);
}

void FrameNodeImpl::JoinGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (size_t i = 0; i < base::size(intervention_policy_); ++i)
    intervention_policy_[i] =
        resource_coordinator::mojom::InterventionPolicy::kUnknown;

  // Wire this up to the other nodes in the graph.
  if (parent_frame_node_)
    parent_frame_node_->AddChildFrame(this);
  page_node_->AddFrame(this);
  process_node_->AddFrame(this);

  NodeBase::JoinGraph();
}

void FrameNodeImpl::LeaveGraph() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NodeBase::LeaveGraph();

  DCHECK(child_frame_nodes_.empty());

  // Leave the page.
  DCHECK(NodeInGraph(page_node_));
  page_node_->RemoveFrame(this);

  // Leave the frame hierarchy.
  if (parent_frame_node_) {
    DCHECK(NodeInGraph(parent_frame_node_));
    parent_frame_node_->RemoveChildFrame(this);
  }

  // And leave the process.
  DCHECK(NodeInGraph(process_node_));
  process_node_->RemoveFrame(this);
}

bool FrameNodeImpl::HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (parent_frame_node_ == frame_node ||
      (parent_frame_node_ &&
       parent_frame_node_->HasFrameNodeInAncestors(frame_node))) {
    return true;
  }
  return false;
}

bool FrameNodeImpl::HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (FrameNodeImpl* child : child_frame_nodes_) {
    if (child == frame_node || child->HasFrameNodeInDescendants(frame_node)) {
      return true;
    }
  }
  return false;
}

}  // namespace performance_manager
