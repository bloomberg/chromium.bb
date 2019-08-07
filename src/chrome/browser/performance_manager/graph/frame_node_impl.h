// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/public/graph/frame_node.h"
#include "url/gurl.h"

namespace performance_manager {

class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;

// Observer interface for FrameNodeImpl objects. This must be declared first as
// the type is referenced by members of FrameNodeImpl.
class FrameNodeImplObserver {
 public:
  FrameNodeImplObserver();
  virtual ~FrameNodeImplObserver();

  // Notifications of property changes.

  // Invoked when the |is_current| property changes.
  virtual void OnIsCurrentChanged(FrameNodeImpl* frame_node) = 0;

  // Invoked when the |network_almost_idle| property changes.
  virtual void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) = 0;

  // Invoked when the |lifecycle_state| property changes.
  virtual void OnLifecycleStateChanged(FrameNodeImpl* frame_node) = 0;

  // Invoked when the |url| property changes.
  virtual void OnURLChanged(FrameNodeImpl* frame_node) = 0;

  // Events with no property changes.

  // Invoked when a non-persistent notification has been issued by the frame.
  virtual void OnNonPersistentNotificationCreated(
      FrameNodeImpl* frame_node) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameNodeImplObserver);
};

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
    : public PublicNodeImpl<FrameNodeImpl, FrameNode>,
      public TypedNodeBase<FrameNodeImpl, FrameNodeImplObserver>,
      public resource_coordinator::mojom::DocumentCoordinationUnit {
 public:
  // A do-nothing implementation of the observer. Derive from this if you want
  // to selectively override a few methods and not have to worry about
  // continuously updating your implementation as new methods are added.
  class ObserverDefaultImpl;

  static constexpr NodeTypeEnum Type() { return NodeTypeEnum::kFrame; }

  // Construct a frame node associated with a |process_node|, a |page_node| and
  // optionally with a |parent_frame_node|. For the main frame of |page_node|
  // the |parent_frame_node| parameter should be nullptr.
  FrameNodeImpl(GraphImpl* graph,
                ProcessNodeImpl* process_node,
                PageNodeImpl* page_node,
                FrameNodeImpl* parent_frame_node,
                int frame_tree_node_id,
                const base::UnguessableToken& dev_tools_token,
                int32_t browsing_instance_id,
                int32_t site_instance_id);
  ~FrameNodeImpl() override;

  void Bind(
      resource_coordinator::mojom::DocumentCoordinationUnitRequest request);

  // resource_coordinator::mojom::DocumentCoordinationUnit implementation.
  void SetNetworkAlmostIdle() override;
  void SetLifecycleState(
      resource_coordinator::mojom::LifecycleState state) override;
  void SetHasNonEmptyBeforeUnload(bool has_nonempty_beforeunload) override;
  void SetInterventionPolicy(
      resource_coordinator::mojom::PolicyControlledIntervention intervention,
      resource_coordinator::mojom::InterventionPolicy policy) override;
  void SetIsAdFrame() override;
  void OnNonPersistentNotificationCreated() override;

  // Getters for const properties. These can be called from any thread.
  FrameNodeImpl* parent_frame_node() const;
  PageNodeImpl* page_node() const;
  ProcessNodeImpl* process_node() const;
  int frame_tree_node_id() const;
  const base::UnguessableToken& dev_tools_token() const;
  int32_t browsing_instance_id() const;
  int32_t site_instance_id() const;

  // Getters for non-const properties. These are not thread safe.
  const base::flat_set<FrameNodeImpl*>& child_frame_nodes() const;
  resource_coordinator::mojom::LifecycleState lifecycle_state() const;
  bool has_nonempty_beforeunload() const;
  const GURL& url() const;
  bool is_current() const;
  bool network_almost_idle() const;
  bool is_ad_frame() const;

  // Setters are not thread safe.
  void SetIsCurrent(bool is_current);

  // A frame is a main frame if it has no |parent_frame_node|. This can be
  // called from any thread.
  bool IsMainFrame() const;

  // Invoked when a navigation is committed in the frame.
  void OnNavigationCommitted(const GURL& url, bool same_document);

  // Returns true if all intervention policies have been set for this frame.
  bool AreAllInterventionPoliciesSet() const;

  // Sets the same policy for all intervention types in this frame. Causes
  // Page::OnFrameInterventionPolicyChanged to be invoked.
  void SetAllInterventionPoliciesForTesting(
      resource_coordinator::mojom::InterventionPolicy policy);

 private:
  friend class PageNodeImpl;
  friend class ProcessNodeImpl;

  // Properties associated with a Document, which are reset when a
  // different-document navigation is committed in the frame.
  struct DocumentProperties {
    DocumentProperties();
    ~DocumentProperties();

    void Reset(FrameNodeImpl* frame_node, const GURL& url_in);

    ObservedProperty::NotifiesOnlyOnChanges<GURL, &Observer::OnURLChanged> url;
    bool has_nonempty_beforeunload = false;

    // Network is considered almost idle when there are no more than 2 network
    // connections.
    ObservedProperty::
        NotifiesOnlyOnChanges<bool, &Observer::OnNetworkAlmostIdleChanged>
            network_almost_idle{false};
  };

  // Invoked by subframes on joining/leaving the graph.
  void AddChildFrame(FrameNodeImpl* frame_node);
  void RemoveChildFrame(FrameNodeImpl* frame_node);

  void JoinGraph() override;
  void LeaveGraph() override;

  bool HasFrameNodeInAncestors(FrameNodeImpl* frame_node) const;
  bool HasFrameNodeInDescendants(FrameNodeImpl* frame_node) const;

  mojo::Binding<resource_coordinator::mojom::DocumentCoordinationUnit> binding_;

  FrameNodeImpl* const parent_frame_node_;
  PageNodeImpl* const page_node_;
  ProcessNodeImpl* const process_node_;
  // Can be used to tie together "sibling" frames, where a navigation is ongoing
  // in a new frame that will soon replace the existing one.
  const int frame_tree_node_id_;
  // A unique identifier shared with all representations of this node across
  // content and blink. The token is only defined by the browser process and
  // is never sent back from the renderer in control calls. It should never be
  // used to look up the FrameTreeNode instance.
  const base::UnguessableToken dev_tools_token_;
  // The unique ID of the BrowsingInstance this frame belongs to. Frames in the
  // same BrowsingInstance are allowed to script each other at least
  // asynchronously (if cross-site), and sometimes synchronously (if same-site,
  // and thus same SiteInstance).
  const int32_t browsing_instance_id_;
  // The unique ID of the SiteInstance this frame belongs to. Frames in the
  // same SiteInstance may sychronously script each other. Frames with the
  // same |site_instance_id_| will also have the same |browsing_instance_id_|.
  const int32_t site_instance_id_;

  base::flat_set<FrameNodeImpl*> child_frame_nodes_;

  // Does *not* change when a navigation is committed.
  ObservedProperty::NotifiesOnlyOnChanges<
      resource_coordinator::mojom::LifecycleState,
      &Observer::OnLifecycleStateChanged>
      lifecycle_state_{resource_coordinator::mojom::LifecycleState::kRunning};

  // This is a one way switch. Once marked an ad-frame, always an ad-frame.
  bool is_ad_frame_ = false;

  ObservedProperty::NotifiesOnlyOnChanges<bool, &Observer::OnIsCurrentChanged>
      is_current_{false};

  // Intervention policy for this frame. These are communicated from the
  // renderer process and are controlled by origin trials.
  //
  // TODO(fdoray): Adapt aggregation logic in PageNodeImpl to allow moving this
  // to DocumentProperties.
  resource_coordinator::mojom::InterventionPolicy
      intervention_policy_[static_cast<size_t>(
                               resource_coordinator::mojom::
                                   PolicyControlledIntervention::kMaxValue) +
                           1];

  // Properties associated with a Document, which are reset when a
  // different-document navigation is committed in the frame.
  //
  // TODO(fdoray): Cleanup this once there is a 1:1 mapping between
  // RenderFrameHost and Document https://crbug.com/936696.
  DocumentProperties document_;

  DISALLOW_COPY_AND_ASSIGN(FrameNodeImpl);
};

// A do-nothing default implementation of a FrameNodeImpl::Observer.
class FrameNodeImpl::ObserverDefaultImpl : public FrameNodeImpl::Observer {
 public:
  ObserverDefaultImpl();
  ~ObserverDefaultImpl() override;

  // FrameNodeImplObserver implementation:
  void OnIsCurrentChanged(FrameNodeImpl* frame_node) override {}
  void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) override {}
  void OnLifecycleStateChanged(FrameNodeImpl* frame_node) override {}
  void OnNonPersistentNotificationCreated(FrameNodeImpl* frame_node) override {}
  void OnURLChanged(FrameNodeImpl* frame_node) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ObserverDefaultImpl);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_FRAME_NODE_IMPL_H_
