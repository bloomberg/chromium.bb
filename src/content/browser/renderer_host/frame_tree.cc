// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_tree.h"

#include <stddef.h>

#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/optional_trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_factory.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/content_switches_internal.h"
#include "content/common/debug_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/loader/loader_constants.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"

namespace content {

namespace {

using perfetto::protos::pbzero::ChromeTrackEvent;

// Helper function to collect SiteInstances involved in rendering a single
// FrameTree (which is a subset of SiteInstances in main frame's proxy_hosts_
// because of openers).
std::set<SiteInstance*> CollectSiteInstances(FrameTree* tree) {
  std::set<SiteInstance*> instances;
  for (FrameTreeNode* node : tree->Nodes())
    instances.insert(node->current_frame_host()->GetSiteInstance());
  return instances;
}

// If |node| is the placeholder FrameTreeNode for an embedded frame tree,
// returns the inner tree's main frame's FrameTreeNode. Otherwise, returns null.
FrameTreeNode* GetInnerTreeMainFrameNode(FrameTreeNode* node) {
  FrameTreeNode* inner_main_frame_tree_node = FrameTreeNode::GloballyFindByID(
      node->current_frame_host()->inner_tree_main_frame_tree_node_id());
  RenderFrameHostImpl* inner_tree_main_frame =
      inner_main_frame_tree_node
          ? inner_main_frame_tree_node->current_frame_host()
          : nullptr;

  if (inner_tree_main_frame) {
    DCHECK_NE(node->frame_tree(), inner_tree_main_frame->frame_tree());
    DCHECK(inner_tree_main_frame->frame_tree_node());
  }

  return inner_tree_main_frame ? inner_tree_main_frame->frame_tree_node()
                               : nullptr;
}

void PrintCrashKeysForBug1250218(FrameTreeNode* ftn,
                                 SiteInstanceImpl* focused_site_instance,
                                 SiteInstanceImpl* proxy_site_instance) {
  // We tried to call SetFocusedFrame on a non-existent RenderFrameProxyHost.
  // This shouldn't happen but it does. Log crash keys to figure out what's
  // going on for https://crbug.com/1250218.

  // Log info about the RenderFrameHost that got the focus, and its main frame.
  RenderFrameHostImpl* rfh = ftn->current_frame_host();
  SCOPED_CRASH_KEY_BOOL("NoProxy", "focused_is_main_frame", ftn->IsMainFrame());
  SCOPED_CRASH_KEY_BOOL("NoProxy", "focused_is_render_frame_live",
                        rfh->IsRenderFrameLive());
  SCOPED_CRASH_KEY_STRING32(
      "NoProxy", "focused_lifecycle_state",
      RenderFrameHostImpl::LifecycleStateImplToString(rfh->lifecycle_state()));
  SCOPED_CRASH_KEY_BOOL(
      "NoProxy", "focused_was_bfcache_restored",
      rfh->was_restored_from_back_forward_cache_for_debugging());
  SCOPED_CRASH_KEY_STRING32("NoProxy", "main_rfh_lifecycle_state",
                            RenderFrameHostImpl::LifecycleStateImplToString(
                                rfh->GetMainFrame()->lifecycle_state()));
  SCOPED_CRASH_KEY_BOOL(
      "NoProxy", "main_rfh_was_bfcache_restored",
      rfh->GetMainFrame()
          ->was_restored_from_back_forward_cache_for_debugging());

  // Log info about the SiteInstance of the RenderFrameHost that got the focus.
  SCOPED_CRASH_KEY_NUMBER("NoProxy", "focused_site_instance",
                          focused_site_instance->GetId().value());
  SCOPED_CRASH_KEY_NUMBER(
      "NoProxy", "focused_browsing_instance",
      focused_site_instance->GetBrowsingInstanceId().value());
  SCOPED_CRASH_KEY_BOOL("NoProxy", "focused_site_instance_default",
                        focused_site_instance->IsDefaultSiteInstance());
  SCOPED_CRASH_KEY_STRING256(
      "NoProxy", "focused_site_info",
      focused_site_instance->GetSiteInfo().GetDebugString());
  SCOPED_CRASH_KEY_BOOL(
      "NoProxy", "focused_si_has_rvh",
      !!ftn->frame_tree()->GetRenderViewHost(focused_site_instance));

  // Log info about the problematic proxy's SiteInstance.
  SCOPED_CRASH_KEY_NUMBER("NoProxy", "proxy_site_instance",
                          proxy_site_instance->GetId().value());
  SCOPED_CRASH_KEY_NUMBER("NoProxy", "proxy_browsing_instance",
                          proxy_site_instance->GetBrowsingInstanceId().value());
  SCOPED_CRASH_KEY_BOOL("NoProxy", "proxy_site_instance_default",
                        proxy_site_instance->IsDefaultSiteInstance());
  SCOPED_CRASH_KEY_STRING256(
      "NoProxy", "proxy_site_info",
      proxy_site_instance->GetSiteInfo().GetDebugString());
  SCOPED_CRASH_KEY_BOOL(
      "NoProxy", "proxy_si_has_rvh",
      !!ftn->frame_tree()->GetRenderViewHost(proxy_site_instance));

  // Log info about BFCache's relation wih the focused RenderFrameHost and the
  // problematic proxy.
  BackForwardCacheImpl& back_forward_cache =
      ftn->navigator().controller().GetBackForwardCache();
  SCOPED_CRASH_KEY_NUMBER("NoProxy", "bfcache_entries_size",
                          back_forward_cache.GetEntries().size());
  SCOPED_CRASH_KEY_BOOL(
      "NoProxy", "focused_bi_in_bfcache",
      back_forward_cache.IsBrowsingInstanceInBackForwardCacheForDebugging(
          focused_site_instance->GetBrowsingInstanceId()));
  SCOPED_CRASH_KEY_BOOL(
      "NoProxy", "proxy_bi_in_bfcache",
      back_forward_cache.IsBrowsingInstanceInBackForwardCacheForDebugging(
          proxy_site_instance->GetBrowsingInstanceId()));

  base::debug::DumpWithoutCrashing();

  TRACE_EVENT_INSTANT("navigation", "FrameTree::PrintCrashKeysForBug1250218",
                      ChromeTrackEvent::kFrameTreeNodeInfo, *ftn,
                      ChromeTrackEvent::kSiteInstance, *proxy_site_instance);
  CaptureTraceForNavigationDebugScenario(
      DebugScenario::kDebugNoRenderFrameProxyHostOnSetFocusedFrame);
}

}  // namespace

FrameTree::NodeIterator::NodeIterator(const NodeIterator& other) = default;

FrameTree::NodeIterator::~NodeIterator() = default;

FrameTree::NodeIterator& FrameTree::NodeIterator::operator++() {
  if (current_node_ != root_of_subtree_to_skip_) {
    // Reserve enough space in the queue to accommodate the nodes we're
    // going to add, to avoid repeated resize calls.
    queue_.reserve(queue_.size() + current_node_->child_count());

    for (size_t i = 0; i < current_node_->child_count(); ++i) {
      FrameTreeNode* child = current_node_->child_at(i);
      FrameTreeNode* inner_tree_main_ftn = GetInnerTreeMainFrameNode(child);
      queue_.push_back((should_descend_into_inner_trees_ && inner_tree_main_ftn)
                           ? inner_tree_main_ftn
                           : child);
    }

    if (should_descend_into_inner_trees_) {
      auto unattached_nodes =
          current_node_->current_frame_host()
              ->delegate()
              ->GetUnattachedOwnedNodes(current_node_->current_frame_host());

      // Reserve enough space in the queue to accommodate the nodes we're
      // going to add.
      queue_.reserve(queue_.size() + unattached_nodes.size());

      for (auto* unattached_node : unattached_nodes) {
        queue_.push_back(unattached_node);
      }
    }
  }

  AdvanceNode();
  return *this;
}

FrameTree::NodeIterator& FrameTree::NodeIterator::AdvanceSkippingChildren() {
  AdvanceNode();
  return *this;
}

bool FrameTree::NodeIterator::operator==(const NodeIterator& rhs) const {
  return current_node_ == rhs.current_node_;
}

void FrameTree::NodeIterator::AdvanceNode() {
  if (!queue_.empty()) {
    current_node_ = queue_.front();
    queue_.pop_front();
  } else {
    current_node_ = nullptr;
  }
}

FrameTree::NodeIterator::NodeIterator(
    const std::vector<FrameTreeNode*>& starting_nodes,
    const FrameTreeNode* root_of_subtree_to_skip,
    bool should_descend_into_inner_trees)
    : current_node_(nullptr),
      root_of_subtree_to_skip_(root_of_subtree_to_skip),
      should_descend_into_inner_trees_(should_descend_into_inner_trees),
      queue_(starting_nodes.begin(), starting_nodes.end()) {
  AdvanceNode();
}

FrameTree::NodeIterator FrameTree::NodeRange::begin() {
  // We shouldn't be attempting a frame tree traversal while the tree is
  // being constructed or destructed.
  DCHECK(std::all_of(
      starting_nodes_.begin(), starting_nodes_.end(),
      [](FrameTreeNode* ftn) { return ftn->current_frame_host(); }));

  return NodeIterator(starting_nodes_, root_of_subtree_to_skip_,
                      should_descend_into_inner_trees_);
}

FrameTree::NodeIterator FrameTree::NodeRange::end() {
  return NodeIterator({}, nullptr, should_descend_into_inner_trees_);
}

FrameTree::NodeRange::NodeRange(
    const std::vector<FrameTreeNode*>& starting_nodes,
    const FrameTreeNode* root_of_subtree_to_skip,
    bool should_descend_into_inner_trees)
    : starting_nodes_(starting_nodes),
      root_of_subtree_to_skip_(root_of_subtree_to_skip),
      should_descend_into_inner_trees_(should_descend_into_inner_trees) {}

FrameTree::NodeRange::NodeRange(const NodeRange&) = default;
FrameTree::NodeRange::~NodeRange() = default;

FrameTree::FrameTree(
    BrowserContext* browser_context,
    Delegate* delegate,
    NavigationControllerDelegate* navigation_controller_delegate,
    NavigatorDelegate* navigator_delegate,
    RenderFrameHostDelegate* render_frame_delegate,
    RenderViewHostDelegate* render_view_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    RenderFrameHostManager::Delegate* manager_delegate,
    PageDelegate* page_delegate,
    Type type)
    : delegate_(delegate),
      render_frame_delegate_(render_frame_delegate),
      render_view_delegate_(render_view_delegate),
      render_widget_delegate_(render_widget_delegate),
      manager_delegate_(manager_delegate),
      page_delegate_(page_delegate),
      navigator_(browser_context,
                 *this,
                 navigator_delegate,
                 navigation_controller_delegate),
      root_(new FrameTreeNode(this,
                              nullptr,
                              // The top-level frame must always be in a
                              // document scope.
                              blink::mojom::TreeScopeType::kDocument,
                              std::string(),
                              std::string(),
                              false,
                              base::UnguessableToken::Create(),
                              blink::mojom::FrameOwnerProperties(),
                              blink::FrameOwnerElementType::kNone,
                              blink::FramePolicy())),
      focused_frame_tree_node_id_(FrameTreeNode::kFrameTreeNodeInvalidId),
      load_progress_(0.0),
      type_(type) {}

FrameTree::~FrameTree() {
  is_being_destroyed_ = true;
#if DCHECK_IS_ON()
  DCHECK(was_shut_down_);
#endif
  delete root_;
  root_ = nullptr;
}

FrameTreeNode* FrameTree::FindByID(int frame_tree_node_id) {
  for (FrameTreeNode* node : Nodes()) {
    if (node->frame_tree_node_id() == frame_tree_node_id)
      return node;
  }
  return nullptr;
}

FrameTreeNode* FrameTree::FindByRoutingID(int process_id, int routing_id) {
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, routing_id);
  if (render_frame_host) {
    FrameTreeNode* result = render_frame_host->frame_tree_node();
    if (this == result->frame_tree())
      return result;
  }

  RenderFrameProxyHost* render_frame_proxy_host =
      RenderFrameProxyHost::FromID(process_id, routing_id);
  if (render_frame_proxy_host) {
    FrameTreeNode* result = render_frame_proxy_host->frame_tree_node();
    if (this == result->frame_tree())
      return result;
  }

  return nullptr;
}

FrameTreeNode* FrameTree::FindByName(const std::string& name) {
  if (name.empty())
    return root_;

  for (FrameTreeNode* node : Nodes()) {
    if (node->frame_name() == name)
      return node;
  }

  return nullptr;
}

FrameTree::NodeRange FrameTree::Nodes() {
  return NodesExceptSubtree(nullptr);
}

FrameTree::NodeRange FrameTree::SubtreeNodes(FrameTreeNode* subtree_root) {
  return NodeRange({subtree_root}, nullptr,
                   /* should_descend_into_inner_trees */ false);
}

FrameTree::NodeRange FrameTree::NodesIncludingInnerTreeNodes() {
  return NodeRange({root_}, nullptr,
                   /* should_descend_into_inner_trees */ true);
}

FrameTree::NodeRange FrameTree::SubtreeAndInnerTreeNodes(
    RenderFrameHostImpl* parent) {
  std::vector<FrameTreeNode*> starting_nodes;
  starting_nodes.reserve(parent->child_count());
  for (size_t i = 0; i < parent->child_count(); ++i) {
    FrameTreeNode* child = parent->child_at(i);
    FrameTreeNode* inner_tree_main_ftn = GetInnerTreeMainFrameNode(child);
    starting_nodes.push_back(inner_tree_main_ftn ? inner_tree_main_ftn : child);
  }
  const std::vector<FrameTreeNode*> unattached_owned_nodes =
      parent->delegate()->GetUnattachedOwnedNodes(parent);
  starting_nodes.insert(starting_nodes.end(), unattached_owned_nodes.begin(),
                        unattached_owned_nodes.end());
  return NodeRange(starting_nodes, nullptr,
                   /* should_descend_into_inner_trees */ true);
}

FrameTree::NodeRange FrameTree::NodesExceptSubtree(FrameTreeNode* node) {
  return NodeRange({root_}, node, /* should_descend_into_inner_trees */ false);
}

FrameTreeNode* FrameTree::AddFrame(
    RenderFrameHostImpl* parent,
    int process_id,
    int new_routing_id,
    mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker_receiver,
    blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params,
    blink::mojom::TreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    bool is_created_by_script,
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::FramePolicy& frame_policy,
    const blink::mojom::FrameOwnerProperties& frame_owner_properties,
    bool was_discarded,
    blink::FrameOwnerElementType owner_type,
    bool is_dummy_frame_for_inner_tree) {
  CHECK_NE(new_routing_id, MSG_ROUTING_NONE);
  // Normally this path is for blink adding a child local frame. But both
  // portals and fenced frames add a dummy child frame that never gets a
  // corresponding RenderFrameImpl in any renderer process, and therefore its
  // `frame_remote` is invalid. Also its RenderFrameHostImpl is exempt from
  // having `RenderFrameCreated()` called on it (see later in this method, as
  // well as `WebContentsObserverConsistencyChecker::RenderFrameHostChanged()`).
  DCHECK_NE(frame_remote.is_valid(), is_dummy_frame_for_inner_tree);
  DCHECK_NE(browser_interface_broker_receiver.is_valid(),
            is_dummy_frame_for_inner_tree);

  // A child frame always starts with an initial empty document, which means
  // it is in the same SiteInstance as the parent frame. Ensure that the process
  // which requested a child frame to be added is the same as the process of the
  // parent node.
  CHECK_EQ(parent->GetProcess()->GetID(), process_id);

  std::unique_ptr<FrameTreeNode> new_node = base::WrapUnique(new FrameTreeNode(
      this, parent, scope, frame_name, frame_unique_name, is_created_by_script,
      devtools_frame_token, frame_owner_properties, owner_type, frame_policy));

  // Set sandbox flags and container policy and make them effective immediately,
  // since initial sandbox flags and permissions policy should apply to the
  // initial empty document in the frame. This needs to happen before the call
  // to AddChild so that the effective policy is sent to any newly-created
  // RenderFrameProxy objects when the RenderFrameHost is created.
  // SetPendingFramePolicy is necessary here because next navigation on this
  // frame will need the value of pending frame policy instead of effective
  // frame policy.
  new_node->SetPendingFramePolicy(frame_policy);
  new_node->CommitFramePolicy(frame_policy);

  if (was_discarded)
    new_node->set_was_discarded();

  // Add the new node to the FrameTree, creating the RenderFrameHost.
  FrameTreeNode* added_node =
      parent->AddChild(std::move(new_node), new_routing_id,
                       std::move(frame_remote), frame_token);

  added_node->SetFencedFrameNonceIfNeeded();

  if (browser_interface_broker_receiver.is_valid()) {
    added_node->current_frame_host()->BindBrowserInterfaceBrokerReceiver(
        std::move(browser_interface_broker_receiver));
  }

  if (policy_container_bind_params) {
    added_node->current_frame_host()->policy_container_host()->Bind(
        std::move(policy_container_bind_params));
  }

  // The last committed NavigationEntry may have a FrameNavigationEntry with the
  // same |frame_unique_name|, since we don't remove FrameNavigationEntries if
  // their frames are deleted.  If there is a stale one, remove it to avoid
  // conflicts on future updates.
  NavigationEntryImpl* last_committed_entry = static_cast<NavigationEntryImpl*>(
      navigator_.controller().GetLastCommittedEntry());
  if (last_committed_entry) {
    last_committed_entry->RemoveEntryForFrame(
        added_node, /* only_if_different_position = */ true);
  }

  // Now that the new node is part of the FrameTree and has a RenderFrameHost,
  // we can announce the creation of the initial RenderFrame which already
  // exists in the renderer process.
  // For consistency with navigating to a new RenderFrameHost case, we dispatch
  // RenderFrameCreated before RenderFrameHostChanged.
  if (!is_dummy_frame_for_inner_tree) {
    // The outer dummy FrameTreeNode for both portals and fenced frames does not
    // have a live RenderFrame in the renderer process.
    added_node->current_frame_host()->RenderFrameCreated();
  }

  // Notify the delegate of the creation of the current RenderFrameHost.
  // This is only for subframes, as the main frame case is taken care of by
  // WebContentsImpl::Init.
  manager_delegate_->NotifySwappedFromRenderManager(
      nullptr, added_node->current_frame_host());
  return added_node;
}

void FrameTree::RemoveFrame(FrameTreeNode* child) {
  RenderFrameHostImpl* parent = child->parent();
  if (!parent) {
    NOTREACHED() << "Unexpected RemoveFrame call for main frame.";
    return;
  }

  parent->RemoveChild(child);
}

void FrameTree::CreateProxiesForSiteInstance(FrameTreeNode* source,
                                             SiteInstance* site_instance) {
  // Create the RenderFrameProxyHost for the new SiteInstance.
  if (!source || !source->IsMainFrame()) {
    RenderViewHostImpl* render_view_host =
        GetRenderViewHost(site_instance).get();
    if (render_view_host) {
      root()->render_manager()->EnsureRenderViewInitialized(render_view_host,
                                                            site_instance);
    } else {
      root()->render_manager()->CreateRenderFrameProxy(site_instance);
    }
  }

  // Check whether we're in an inner delegate and |site_instance| corresponds
  // to the outer delegate.  Subframe proxies aren't needed if this is the
  // case.
  bool is_site_instance_for_outer_delegate = false;
  RenderFrameProxyHost* outer_delegate_proxy =
      root()->render_manager()->GetProxyToOuterDelegate();
  if (outer_delegate_proxy) {
    is_site_instance_for_outer_delegate =
        (site_instance == outer_delegate_proxy->GetSiteInstance());
  }

  // Proxies are created in the FrameTree in response to a node navigating to a
  // new SiteInstance. Since |source|'s navigation will replace the currently
  // loaded document, the entire subtree under |source| will be removed, and
  // thus proxy creation is skipped for all nodes in that subtree.
  //
  // However, a proxy *is* needed for the |source| node itself.  This lets
  // cross-process navigations in |source| start with a proxy and follow a
  // remote-to-local transition, which avoids race conditions in cases where
  // other navigations need to reference |source| before it commits. See
  // https://crbug.com/756790 for more background.  Therefore,
  // NodesExceptSubtree(source) will include |source| in the nodes traversed
  // (see NodeIterator::operator++).
  for (FrameTreeNode* node : NodesExceptSubtree(source)) {
    // If a new frame is created in the current SiteInstance, other frames in
    // that SiteInstance don't need a proxy for the new frame.
    RenderFrameHostImpl* current_host =
        node->render_manager()->current_frame_host();
    SiteInstance* current_instance = current_host->GetSiteInstance();
    if (current_instance != site_instance) {
      if (node == source && !current_host->IsRenderFrameLive()) {
        // We don't create a proxy at |source| when the current RenderFrameHost
        // isn't live.  This is because either (1) the speculative
        // RenderFrameHost will be committed immediately, and the proxy
        // destroyed right away, in GetFrameHostForNavigation, which makes the
        // races above impossible, or (2) the early commit will be skipped due
        // to ShouldSkipEarlyCommitPendingForCrashedFrame, in which case the
        // proxy for |source| *is* needed, but it will be created later in
        // CreateProxiesForNewRenderFrameHost.
        //
        // TODO(fergal): Consider creating a proxy for |source| here rather than
        // in CreateProxiesForNewRenderFrameHost for case (2) above.
        continue;
      }

      // Do not create proxies for subframes in the outer delegate's
      // SiteInstance, since there is no need to expose these subframes to the
      // outer delegate.  See also comments in CreateProxiesForChildFrame() and
      // https://crbug.com/1013553.
      if (!node->IsMainFrame() && is_site_instance_for_outer_delegate)
        continue;

      node->render_manager()->CreateRenderFrameProxy(site_instance);
    }
  }
}

RenderFrameHostImpl* FrameTree::GetMainFrame() const {
  return root_->current_frame_host();
}

FrameTreeNode* FrameTree::GetFocusedFrame() {
  return FindByID(focused_frame_tree_node_id_);
}

void FrameTree::SetFocusedFrame(FrameTreeNode* node, SiteInstance* source) {
  TRACE_EVENT("navigation", "FrameTree::SetFocusedFrame",
              ChromeTrackEvent::kFrameTreeNodeInfo, *node,
              [&](perfetto::EventContext ctx) {
                if (!source)
                  return;
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                static_cast<SiteInstanceImpl*>(source)->WriteIntoTrace(
                    ctx.Wrap(event->set_site_instance()));
              });
  CHECK(node->current_frame_host()->IsActive());
  if (node == GetFocusedFrame())
    return;

  std::set<SiteInstance*> frame_tree_site_instances =
      CollectSiteInstances(this);

  SiteInstance* current_instance =
      node->current_frame_host()->GetSiteInstance();

  TRACE_EVENT_INSTANT("navigation", "FrameTree::SetFocusedFrame_Current",
                      ChromeTrackEvent::kSiteInstance,
                      *static_cast<SiteInstanceImpl*>(current_instance));

  // Update the focused frame in all other SiteInstanceGroups.  If focus changes
  // to a cross-group frame, this allows the old focused frame's renderer
  // process to clear focus from that frame and fire blur events.  It also
  // ensures that the latest focused frame is available in all renderers to
  // compute document.activeElement.
  //
  // We do not notify the |source| SiteInstance because it already knows the
  // new focused frame (since it initiated the focus change), and we notify the
  // new focused frame's SiteInstance (if it differs from |source|) separately
  // below.
  // TODO(https://crbug.com/1261963, yangsharon): CollectSiteInstances needs to
  // be updated to CollectSiteInstanceGroups, otherwise in the case multiple
  // SiteInstances are in the same group, SetFocusedFrame below will be called
  // multiple times. While SiteInstances and SiteInstanceGroups are 1:1,
  // CollectSiteInstances and CollectSiteInstanceGroups are equivalent.
  for (auto* instance : frame_tree_site_instances) {
    if (instance != source && instance != current_instance) {
      RenderFrameProxyHost* proxy =
          node->render_manager()->GetRenderFrameProxyHost(
              static_cast<SiteInstanceImpl*>(instance)->group());
      if (proxy) {
        proxy->SetFocusedFrame();
      } else {
        PrintCrashKeysForBug1250218(
            node, static_cast<SiteInstanceImpl*>(current_instance),
            static_cast<SiteInstanceImpl*>(instance));
      }
    }
  }

  // If |node| was focused from a cross-process frame (i.e., via
  // window.focus()), tell its RenderFrame that it should focus.
  if (current_instance != source)
    node->current_frame_host()->SetFocusedFrame();

  focused_frame_tree_node_id_ = node->frame_tree_node_id();
  node->DidFocus();

  // The accessibility tree data for the root of the frame tree keeps
  // track of the focused frame too, so update that every time the
  // focused frame changes.
  root()
      ->current_frame_host()
      ->GetOutermostMainFrameOrEmbedder()
      ->UpdateAXTreeData();
}

scoped_refptr<RenderViewHostImpl> FrameTree::CreateRenderViewHost(
    SiteInstance* site_instance,
    int32_t main_frame_routing_id,
    bool swapped_out,
    bool renderer_initiated_creation) {
  RenderViewHostImpl* rvh =
      static_cast<RenderViewHostImpl*>(RenderViewHostFactory::Create(
          this, site_instance, render_view_delegate_, render_widget_delegate_,
          main_frame_routing_id, swapped_out, renderer_initiated_creation));
  return base::WrapRefCounted(rvh);
}

scoped_refptr<RenderViewHostImpl> FrameTree::GetRenderViewHost(
    SiteInstance* site_instance) {
  auto it = render_view_host_map_.find(GetRenderViewHostMapId(
      static_cast<SiteInstanceImpl*>(site_instance)->group()));
  if (it == render_view_host_map_.end())
    return nullptr;

  return base::WrapRefCounted(it->second);
}

FrameTree::RenderViewHostMapId FrameTree::GetRenderViewHostMapId(
    SiteInstanceGroup* site_instance_group) const {
  return RenderViewHostMapId::FromUnsafeValue(
      site_instance_group->GetId().value());
}

void FrameTree::RegisterRenderViewHost(RenderViewHostMapId id,
                                       RenderViewHostImpl* rvh) {
  TRACE_EVENT_INSTANT("navigation", "FrameTree::RegisterRenderViewHost",
                      ChromeTrackEvent::kRenderViewHost, *rvh);
  CHECK(!base::Contains(render_view_host_map_, id));
  render_view_host_map_[id] = rvh;
}

void FrameTree::UnregisterRenderViewHost(RenderViewHostMapId id,
                                         RenderViewHostImpl* rvh) {
  TRACE_EVENT_INSTANT("navigation", "FrameTree::UnregisterRenderViewHost",
                      ChromeTrackEvent::kRenderViewHost, *rvh);
  auto it = render_view_host_map_.find(id);
  CHECK(it != render_view_host_map_.end());
  CHECK_EQ(it->second, rvh);
  render_view_host_map_.erase(it);
}

void FrameTree::FrameUnloading(FrameTreeNode* frame) {
  if (frame->frame_tree_node_id() == focused_frame_tree_node_id_)
    focused_frame_tree_node_id_ = FrameTreeNode::kFrameTreeNodeInvalidId;

  // Ensure frames that are about to be deleted aren't visible from the other
  // processes anymore.
  frame->render_manager()->ResetProxyHosts();
}

void FrameTree::FrameRemoved(FrameTreeNode* frame) {
  if (frame->frame_tree_node_id() == focused_frame_tree_node_id_)
    focused_frame_tree_node_id_ = FrameTreeNode::kFrameTreeNodeInvalidId;
}

double FrameTree::GetLoadProgress() {
  if (root_->HasNavigation())
    return blink::kInitialLoadProgress;

  return root_->current_frame_host()->GetPage().load_progress();
}

bool FrameTree::IsLoading() const {
  for (const FrameTreeNode* node : const_cast<FrameTree*>(this)->Nodes()) {
    if (node->IsLoading())
      return true;
  }
  return false;
}

void FrameTree::ReplicatePageFocus(bool is_focused) {
  // Focus loss may occur while this FrameTree is being destroyed.  Don't
  // send the message in this case, as the main frame's RenderFrameHost and
  // other state has already been cleared.
  if (is_being_destroyed_)
    return;
  std::set<SiteInstance*> frame_tree_site_instances =
      CollectSiteInstances(this);

  // Send the focus update to main frame's proxies in all SiteInstances of
  // other frames in this FrameTree. Note that the main frame might also know
  // about proxies in SiteInstances for frames in a different FrameTree (e.g.,
  // for window.open), so we can't just iterate over its proxy_hosts_ in
  // RenderFrameHostManager.
  for (auto* instance : frame_tree_site_instances)
    SetPageFocus(instance, is_focused);
}

void FrameTree::SetPageFocus(SiteInstance* instance, bool is_focused) {
  RenderFrameHostManager* root_manager = root_->render_manager();

  // Portal frame tree should not get page focus.
  DCHECK(!GetMainFrame()->InsidePortal() || !is_focused);

  // This is only used to set page-level focus in cross-process subframes, and
  // requests to set focus in main frame's SiteInstance are ignored.
  if (instance != root_manager->current_frame_host()->GetSiteInstance()) {
    RenderFrameProxyHost* proxy = root_manager->GetRenderFrameProxyHost(
        static_cast<SiteInstanceImpl*>(instance)->group());
    proxy->GetAssociatedRemoteFrame()->SetPageFocus(is_focused);
  }
}

void FrameTree::RegisterExistingOriginToPreventOptInIsolation(
    const url::Origin& previously_visited_origin,
    NavigationRequest* navigation_request_to_exclude) {
  controller().RegisterExistingOriginToPreventOptInIsolation(
      previously_visited_origin);

  std::unordered_set<SiteInstance*> matching_site_instances;

  // Be sure to visit all RenderFrameHosts associated with this frame that might
  // have an origin that could script other frames. We skip RenderFrameHosts
  // that are in the bfcache, assuming there's no way for a frame to join the
  // BrowsingInstance of a bfcache RFH while it's in the cache.
  for (auto* frame_tree_node : SubtreeNodes(root())) {
    auto* frame_host = frame_tree_node->current_frame_host();
    if (previously_visited_origin == frame_host->GetLastCommittedOrigin())
      matching_site_instances.insert(frame_host->GetSiteInstance());

    if (frame_host->HasCommittingNavigationRequestForOrigin(
            previously_visited_origin, navigation_request_to_exclude)) {
      matching_site_instances.insert(frame_host->GetSiteInstance());
    }

    auto* spec_frame_host =
        frame_tree_node->render_manager()->speculative_frame_host();
    if (spec_frame_host &&
        spec_frame_host->HasCommittingNavigationRequestForOrigin(
            previously_visited_origin, navigation_request_to_exclude)) {
      matching_site_instances.insert(spec_frame_host->GetSiteInstance());
    }

    auto* navigation_request = frame_tree_node->navigation_request();
    if (navigation_request &&
        navigation_request != navigation_request_to_exclude &&
        navigation_request->HasCommittingOrigin(previously_visited_origin)) {
      matching_site_instances.insert(frame_host->GetSiteInstance());
    }
  }

  // Update any SiteInstances found to contain |origin|.
  for (auto* site_instance : matching_site_instances) {
    static_cast<SiteInstanceImpl*>(site_instance)
        ->PreventOptInOriginIsolation(previously_visited_origin);
  }
}

void FrameTree::Init(SiteInstance* main_frame_site_instance,
                     bool renderer_initiated_creation,
                     const std::string& main_frame_name,
                     RenderFrameHostImpl* opener) {
  // blink::FrameTree::SetName always keeps |unique_name| empty in case of a
  // main frame - let's do the same thing here.
  std::string unique_name;
  root_->SetFrameName(main_frame_name, unique_name);
  root_->render_manager()->InitRoot(main_frame_site_instance,
                                    renderer_initiated_creation);
  root_->SetFencedFrameNonceIfNeeded();

  // The initial empty document should inherit the origin of its opener (the
  // origin may change after the first commit), except when they are in
  // different browsing context groups (`renderer_initiated_creation` is false),
  // where it should use a new opaque origin.
  // See also https://crbug.com/932067.
  //
  // Note that the origin of the new frame might depend on sandbox flags.
  // Checking sandbox flags of the new frame should be safe at this point,
  // because the flags should be already inherited when creating the root node.
  DCHECK(!renderer_initiated_creation || opener);
  root_->current_frame_host()->SetOriginDependentStateOfNewFrame(
      renderer_initiated_creation ? opener->GetLastCommittedOrigin()
                                  : url::Origin());
  controller().CreateInitialEntry();
}

void FrameTree::DidAccessInitialMainDocument() {
  OPTIONAL_TRACE_EVENT0("content", "FrameTree::DidAccessInitialDocument");
  has_accessed_initial_main_document_ = true;
  controller().DidAccessInitialMainDocument();
}

void FrameTree::DidStartLoadingNode(FrameTreeNode& node,
                                    bool should_show_loading_ui,
                                    bool was_previously_loading) {
  if (was_previously_loading)
    return;

  root()->render_manager()->SetIsLoading(IsLoading());
  delegate_->DidStartLoading(&node, should_show_loading_ui);
}

void FrameTree::DidStopLoadingNode(FrameTreeNode& node) {
  if (IsLoading())
    return;

  root()->render_manager()->SetIsLoading(false);
  delegate_->DidStopLoading();
}

void FrameTree::DidCancelLoading() {
  OPTIONAL_TRACE_EVENT0("content", "FrameTree::DidCancelLoading");
  navigator_.controller().DiscardNonCommittedEntries();
}

void FrameTree::StopLoading() {
  for (FrameTreeNode* node : Nodes())
    node->StopLoading();
}

void FrameTree::Shutdown() {
  is_being_destroyed_ = true;
#if DCHECK_IS_ON()
  DCHECK(!was_shut_down_);
  was_shut_down_ = true;
#endif

  RenderFrameHostManager* root_manager = root_->render_manager();

  if (!root_manager->current_frame_host()) {
    // The page has been transferred out during an activation. There is little
    // left to do.
    // TODO(https://crbug.com/1199693): If we decide that pending delete RFHs
    // need to be moved along during activation replace this line with a DCHECK
    // that there are no pending delete instances.
    root_manager->ClearRFHsPendingShutdown();
    DCHECK_EQ(0u, root_manager->GetProxyCount());
    DCHECK(!root_->navigation_request());
    DCHECK(!root_manager->speculative_frame_host());
    manager_delegate_->OnFrameTreeNodeDestroyed(root_);
    return;
  }

  for (FrameTreeNode* node : Nodes()) {
    // Delete all RFHs pending shutdown, which will lead the corresponding RVHs
    // to be shutdown and be deleted as well.
    node->render_manager()->ClearRFHsPendingShutdown();
    // TODO(https://crbug.com/1199676): Ban WebUI instance in Prerender pages.
    node->render_manager()->ClearWebUIInstances();
  }

  // Destroy all subframes now. This notifies observers.
  root_manager->current_frame_host()->ResetChildren();
  root_manager->ResetProxyHosts();

  // Manually call the observer methods for the root FrameTreeNode. It is
  // necessary to manually delete all objects tracking navigations
  // (NavigationHandle, NavigationRequest) for observers to be properly
  // notified of these navigations stopping before the WebContents is
  // destroyed.

  root_manager->current_frame_host()->RenderFrameDeleted();
  root_manager->current_frame_host()->ResetNavigationRequests();

  // Do not update state as the FrameTree::Delegate (possibly a WebContents) is
  // being destroyed.
  root_->ResetNavigationRequest(/*keep_state=*/true);
  if (root_manager->speculative_frame_host()) {
    root_manager->DiscardSpeculativeRenderFrameHostForShutdown();
  }

  // NavigationRequests restoring the page from bfcache have a reference to the
  // RFHs stored in the cache, so the cache should be cleared after the
  // navigation request is reset.
  controller().GetBackForwardCache().Shutdown();

  manager_delegate_->OnFrameTreeNodeDestroyed(root_);
  render_view_delegate_->RenderViewDeleted(
      root_manager->current_frame_host()->render_view_host());
}

base::SafeRef<FrameTree> FrameTree::GetSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

void FrameTree::FocusOuterFrameTrees() {
  OPTIONAL_TRACE_EVENT0("content", "FrameTree::FocusOuterFrameTrees");

  FrameTree* frame_tree_to_focus = this;
  while (true) {
    FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
        frame_tree_to_focus->delegate()->GetOuterDelegateFrameTreeNodeId());
    if (!outer_node || !outer_node->current_frame_host()->IsActive()) {
      // Don't set focus on an inactive FrameTreeNode.
      return;
    }
    outer_node->frame_tree()->SetFocusedFrame(outer_node, nullptr);

    // For a browser initiated focus change, let embedding renderer know of the
    // change. Otherwise, if the currently focused element is just across a
    // process boundary in focus order, it will not be possible to move across
    // that boundary. This is because the target element will already be focused
    // (that renderer was not notified) and drop the event.
    if (auto* proxy_to_outer_delegate = frame_tree_to_focus->root()
                                            ->render_manager()
                                            ->GetProxyToOuterDelegate()) {
      proxy_to_outer_delegate->SetFocusedFrame();
    }
    frame_tree_to_focus = outer_node->frame_tree();
  }
}

}  // namespace content
