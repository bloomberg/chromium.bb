// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"
#include "url/gurl.h"

namespace performance_manager {

class PageNodeImpl;
class ProcessNodeImpl;

// Frame nodes form a tree structure, each FrameNode at most has one parent that
// is a FrameNode. Conceptually, a frame corresponds to a
// content::RenderFrameHost in the browser, and a content::RenderFrameImpl /
// blink::LocalFrame in a renderer.
//
// Note that a frame in a frame tree can be replaced with another, with the
// continuity of that position represented via the |frame_tree_node_id|. It is
// possible to have multiple "sibling" nodes that share the same
// |frame_tree_node_id|. Only one of these may contribute to the content being
// rendered, and this node is designated the "current" node in content
// terminology. A swap is effectively atomic but will take place in two steps
// in the graph: the outgoing frame will first be marked as not current, and the
// incoming frame will be marked as current. As such, the graph invariant is
// that there will be 0 or 1 |is_current| frames with a given
// |frame_tree_node_id|.
//
// This occurs when a frame is navigated and the existing frame can't be reused.
// In that case a "provisional" frame is created to start the navigation. Once
// the navigation completes (which may actually involve a redirect to another
// origin meaning the frame has to be destroyed and another one created in
// another process!) and commits, the frame will be swapped with the previously
// active frame.
class FrameNodeImpl
    : public CoordinationUnitInterface<
          FrameNodeImpl,
          resource_coordinator::mojom::FrameCoordinationUnit,
          resource_coordinator::mojom::FrameCoordinationUnitRequest> {
 public:
  static constexpr resource_coordinator::CoordinationUnitType Type() {
    return resource_coordinator::CoordinationUnitType::kFrame;
  }

  // Construct a frame node associated with a |process_node|, a |page_node| and
  // optionally with a |parent_frame_node|. For the main frame of |page_node|
  // the |parent_frame_node| parameter should be nullptr.
  FrameNodeImpl(Graph* graph,
                ProcessNodeImpl* process_node,
                PageNodeImpl* page_node,
                FrameNodeImpl* parent_frame_node,
                int frame_tree_node_id);
  ~FrameNodeImpl() override;

  // FrameNode implementation.
  void SetNetworkAlmostIdle(bool idle) override;
  void SetLifecycleState(
      resource_coordinator::mojom::LifecycleState state) override;
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) override;
  void SetInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention,
      resource_coordinator::mojom::InterventionPolicy policy) override;
  void OnNonPersistentNotificationCreated() override;

  // Getters for const properties. These can be called from any thread.
  FrameNodeImpl* parent_frame_node() const;
  PageNodeImpl* page_node() const;
  ProcessNodeImpl* process_node() const;
  int frame_tree_node_id() const;

  // Getters for non-const properties. These are not thread safe.
  const base::flat_set<FrameNodeImpl*>& child_frame_nodes() const;
  resource_coordinator::mojom::LifecycleState lifecycle_state() const;
  bool has_nonempty_beforeunload() const;
  const GURL& url() const;
  bool is_current() const;
  bool network_almost_idle() const;

  // Setters are not thread safe.
  void set_url(const GURL& url);
  void SetIsCurrent(bool is_current);

  // A frame is a main frame if it has no |parent_frame_node|. This can be
  // called from any thread.
  bool IsMainFrame() const;

  // Returns true if all intervention policies have been set for this frame.
  bool AreAllInterventionPoliciesSet() const;

  // Sets the same policy for all intervention types in this frame. Causes
  // Page::OnFrameInterventionPolicyChanged to be invoked.
  void SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy policy);

 private:
  friend class PageNodeImpl;
  friend class ProcessNodeImpl;

  // Invoked by subframes on joining/leaving the graph.
  void AddChildFrame(FrameNodeImpl* frame_node);
  void RemoveChildFrame(FrameNodeImpl* frame_node);

  void JoinGraph() override;
  void LeaveGraph() override;

  bool HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const;
  bool HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const;

  FrameNodeImpl* const parent_frame_node_;
  PageNodeImpl* const page_node_;
  ProcessNodeImpl* const process_node_;
  // Can be used to tie together "sibling" frames, where a navigation is ongoing
  // in a new frame that will soon replace the existing one.
  const int frame_tree_node_id_;

  base::flat_set<FrameNodeImpl*> child_frame_nodes_;
  ObservedProperty::NotifiesOnlyOnChanges<
      resource_coordinator::mojom::LifecycleState,
      &GraphObserver::OnLifecycleStateChanged>
      lifecycle_state_{resource_coordinator::mojom::LifecycleState::kRunning};

  bool has_nonempty_beforeunload_ = false;
  GURL url_;

  ObservedProperty::NotifiesOnlyOnChanges<bool,
                                          &GraphObserver::OnIsCurrentChanged>
      is_current_{false};

  // Network is considered almost idle when there are no more than 2 network
  // connections.
  ObservedProperty::
      NotifiesOnlyOnChanges<bool, &GraphObserver::OnNetworkAlmostIdleChanged>
          network_almost_idle_{false};

  // Intervention policy for this frame. These are communicated from the
  // renderer process and are controlled by origin trials.
  resource_coordinator::mojom::InterventionPolicy
      intervention_policy_[static_cast<size_t>(
                               resource_coordinator::mojom::
                                   PolicyControlledIntervention::kMaxValue) +
                           1];

  DISALLOW_COPY_AND_ASSIGN(FrameNodeImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
