// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_

#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"

namespace blink {
namespace mojom {
class BrowserInterfaceBroker;
enum class TreeScopeType;
}  // namespace mojom

struct FramePolicy;
}  // namespace blink

namespace content {

class BrowserContext;
class PageDelegate;
class PageImpl;
class RenderFrameHostDelegate;
class RenderViewHostDelegate;
class RenderViewHostImpl;
class RenderFrameHostManager;
class RenderWidgetHostDelegate;
class SiteInstance;

// Represents the frame tree for a page. With the exception of the main frame,
// all FrameTreeNodes will be created/deleted in response to frame attach and
// detach events in the DOM.
//
// The main frame's FrameTreeNode is special in that it is reused. This allows
// it to serve as an anchor for state that needs to persist across top-level
// page navigations.
//
// TODO(ajwong): Move NavigationController ownership to the main frame
// FrameTreeNode. Possibly expose access to it from here.
//
// This object is only used on the UI thread.
class CONTENT_EXPORT FrameTree {
 public:
  class NodeRange;

  class CONTENT_EXPORT NodeIterator
      : public std::iterator<std::forward_iterator_tag, FrameTreeNode*> {
   public:
    NodeIterator(const NodeIterator& other);
    ~NodeIterator();

    NodeIterator& operator++();
    // Advances the iterator and excludes the children of the current node
    NodeIterator& AdvanceSkippingChildren();

    bool operator==(const NodeIterator& rhs) const;
    bool operator!=(const NodeIterator& rhs) const { return !(*this == rhs); }

    FrameTreeNode* operator*() { return current_node_; }

   private:
    friend class FrameTreeTest;
    friend class NodeRange;

    NodeIterator(const std::vector<FrameTreeNode*>& starting_nodes,
                 const FrameTreeNode* root_of_subtree_to_skip,
                 bool should_descend_into_inner_trees);

    void AdvanceNode();

    // `current_node_` and `root_of_subtree_to_skip_` are not a raw_ptr<...> for
    // performance reasons (based on analysis of sampling profiler data and
    // tab_search:top100:2020).
    FrameTreeNode* current_node_;
    const FrameTreeNode* const root_of_subtree_to_skip_;

    const bool should_descend_into_inner_trees_;
    base::circular_deque<FrameTreeNode*> queue_;
  };

  class CONTENT_EXPORT NodeRange {
   public:
    NodeRange(const NodeRange&);
    ~NodeRange();

    NodeIterator begin();
    NodeIterator end();

   private:
    friend class FrameTree;

    NodeRange(const std::vector<FrameTreeNode*>& starting_nodes,
              const FrameTreeNode* root_of_subtree_to_skip,
              bool should_descend_into_inner_trees);

    const std::vector<FrameTreeNode*> starting_nodes_;
    const raw_ptr<const FrameTreeNode> root_of_subtree_to_skip_;
    const bool should_descend_into_inner_trees_;
  };

  class CONTENT_EXPORT Delegate {
   public:
    // A RenderFrameHost in the specified |frame_tree_node| started loading a
    // new document. This corresponds to browser UI starting to show a spinner
    // or other visual indicator for loading. This method is only invoked if the
    // FrameTree hadn't been previously loading. |should_show_loading_ui| will
    // be true unless the load is a fragment navigation, or triggered by
    // history.pushState/replaceState.
    virtual void DidStartLoading(FrameTreeNode* frame_tree_node,
                                 bool should_show_loading_ui) = 0;

    // This is called when all nodes in the FrameTree stopped loading. This
    // corresponds to the browser UI stop showing a spinner or other visual
    // indicator for loading.
    virtual void DidStopLoading() = 0;

    // The load progress was changed.
    virtual void DidChangeLoadProgress() = 0;

    // Returns true when the active RenderWidgetHostView should be hidden.
    virtual bool IsHidden() = 0;

    // Called when current Page of this frame tree changes to `page`.
    virtual void NotifyPageChanged(PageImpl& page) = 0;

    // If the FrameTree using this delegate is an inner/nested FrameTree, then
    // there may be a FrameTreeNode in the outer FrameTree that is considered
    // our outer delegate FrameTreeNode. This method returns the outer delegate
    // FrameTreeNode ID if one exists. If we don't have a an outer delegate
    // FrameTreeNode, this method returns
    // FrameTreeNode::kFrameTreeNodeInvalidId.
    virtual int GetOuterDelegateFrameTreeNodeId() = 0;

    // Returns if this FrameTree represents a portal.
    virtual bool IsPortal() = 0;
  };

  // Type of FrameTree instance.
  enum class Type {
    // This FrameTree is the primary frame tree for the WebContents, whose main
    // document URL is shown in the Omnibox.
    kPrimary,

    // This FrameTree is used to prerender a page in the background which is
    // invisible to the user.
    kPrerender,

    // This FrameTree is used to host the contents of a <fencedframe> element.
    // Even for <fencedframe>s hosted inside a prerendered page, the FrameTree
    // associated with the fenced frame will be kFencedFrame, but the
    // RenderFrameHosts inside of it will have their lifecycle state set to
    // `RenderFrameHost::LifecycleState::kActive`.
    //
    // TODO(crbug.com/1232528, crbug.com/1244274): We should either:
    //  * Fully support nested FrameTrees inside prerendered pages, in which
    //    case all of the RenderFrameHosts inside the nested trees should have
    //    their lifecycle state set to kPrerendering, or...
    //  * Ban nested FrameTrees in prerendered pages, and therefore obsolete the
    //    above paragraph about <fencedframe>s hosted in prerendered pages.
    kFencedFrame
  };

  // A set of delegates are remembered here so that we can create
  // RenderFrameHostManagers.
  FrameTree(BrowserContext* browser_context,
            Delegate* delegate,
            NavigationControllerDelegate* navigation_controller_delegate,
            NavigatorDelegate* navigator_delegate,
            RenderFrameHostDelegate* render_frame_delegate,
            RenderViewHostDelegate* render_view_delegate,
            RenderWidgetHostDelegate* render_widget_delegate,
            RenderFrameHostManager::Delegate* manager_delegate,
            PageDelegate* page_delegate,
            Type type);

  FrameTree(const FrameTree&) = delete;
  FrameTree& operator=(const FrameTree&) = delete;

  ~FrameTree();

  // Initializes the main frame for this FrameTree. That is it creates the
  // initial RenderFrameHost in the root node's RenderFrameHostManager, and also
  // creates an initial NavigationEntry that potentially inherits `opener`'s
  // origin in its NavigationController. This method will call back into the
  // delegates so it should only be called once they have completed their
  // initialization.
  // TODO(carlscab): It would be great if initialization could happened in the
  // constructor so we do not leave objects in a half initialized state.
  void Init(SiteInstance* main_frame_site_instance,
            bool renderer_initiated_creation,
            const std::string& main_frame_name,
            RenderFrameHostImpl* opener);

  Type type() const { return type_; }

  FrameTreeNode* root() const { return root_; }

  bool is_prerendering() const { return type_ == FrameTree::Type::kPrerender; }

  Delegate* delegate() { return delegate_; }

  // Delegates for various objects.  These can be kept centrally on the
  // FrameTree because they are expected to be the same for all frames on a
  // given FrameTree.
  RenderFrameHostDelegate* render_frame_delegate() {
    return render_frame_delegate_;
  }
  RenderViewHostDelegate* render_view_delegate() {
    return render_view_delegate_;
  }
  RenderWidgetHostDelegate* render_widget_delegate() {
    return render_widget_delegate_;
  }
  RenderFrameHostManager::Delegate* manager_delegate() {
    return manager_delegate_;
  }
  PageDelegate* page_delegate() { return page_delegate_; }

  using RenderViewHostMapId = base::IdType32<class RenderViewHostMap>;

  // SiteInstanceGroup IDs are used to look up RenderViewHosts, since there is
  // one RenderViewHost per SiteInstanceGroup in a given FrameTree.
  using RenderViewHostMap = std::unordered_map<RenderViewHostMapId,
                                               RenderViewHostImpl*,
                                               RenderViewHostMapId::Hasher>;
  const RenderViewHostMap& render_view_hosts() const {
    return render_view_host_map_;
  }

  // Returns the FrameTreeNode with the given |frame_tree_node_id| if it is part
  // of this FrameTree.
  FrameTreeNode* FindByID(int frame_tree_node_id);

  // Returns the FrameTreeNode with the given renderer-specific |routing_id|.
  FrameTreeNode* FindByRoutingID(int process_id, int routing_id);

  // Returns the first frame in this tree with the given |name|, or the main
  // frame if |name| is empty.
  // Note that this does NOT support pseudo-names like _self, _top, and _blank,
  // nor searching other FrameTrees (unlike blink::WebView::findFrameByName).
  FrameTreeNode* FindByName(const std::string& name);

  // Returns a range to iterate over all FrameTreeNodes in the frame tree in
  // breadth-first traversal order.
  NodeRange Nodes();

  // Returns a range to iterate over all FrameTreeNodes in a subtree of the
  // frame tree, starting from |subtree_root|.
  NodeRange SubtreeNodes(FrameTreeNode* subtree_root);

  // Returns a range to iterate over all FrameTreeNodes in this frame tree, as
  // well as any FrameTreeNodes of inner frame trees. Note that this includes
  // inner frame trees of inner WebContents as well.
  NodeRange NodesIncludingInnerTreeNodes();

  // Returns a range to iterate over all FrameTreeNodes in a subtree, starting
  // from, but not including |parent|, as well as any FrameTreeNodes of inner
  // frame trees. Note that this includes inner frame trees of inner WebContents
  // as well.
  static NodeRange SubtreeAndInnerTreeNodes(RenderFrameHostImpl* parent);

  // Adds a new child frame to the frame tree. |process_id| is required to
  // disambiguate |new_routing_id|, and it must match the process of the
  // |parent| node. Otherwise no child is added and this method returns nullptr.
  // |interface_provider_receiver| is the receiver end of the InterfaceProvider
  // interface through which the child RenderFrame can access Mojo services
  // exposed by the corresponding RenderFrameHost. The caller takes care of
  // sending the client end of the interface down to the
  // RenderFrame. |policy_container_bind_params|, if not null, is used for
  // binding Blink's policy container to the new RenderFrameHost's
  // PolicyContainerHost. This is only needed if this frame is the result of the
  // CreateChildFrame mojo call, which also delivers the
  // |policy_container_bind_params|. |is_dummy_frame_for_inner_tree| is true if
  // the added frame is only to serve as a placeholder for an inner frame tree
  // (e.g. fenced frames, portals) and will not have a live RenderFrame of its
  // own.
  FrameTreeNode* AddFrame(
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
      bool is_dummy_frame_for_inner_tree);

  // Removes a frame from the frame tree. |child|, its children, and objects
  // owned by their RenderFrameHostManagers are immediately deleted. The root
  // node cannot be removed this way.
  void RemoveFrame(FrameTreeNode* child);

  // This method walks the entire frame tree and creates a RenderFrameProxyHost
  // for the given |site_instance| in each node except the |source| one --
  // the source will have a RenderFrameHost.  |source| may be null if there is
  // no node navigating in this frame tree (such as when this is called
  // for an opener's frame tree), in which case no nodes are skipped for
  // RenderFrameProxyHost creation.
  void CreateProxiesForSiteInstance(FrameTreeNode* source,
                                    SiteInstance* site_instance);

  // Convenience accessor for the main frame's RenderFrameHostImpl.
  RenderFrameHostImpl* GetMainFrame() const;

  // Returns the focused frame.
  FrameTreeNode* GetFocusedFrame();

  // Sets the focused frame to |node|.  |source| identifies the SiteInstance
  // that initiated this focus change.  If this FrameTree has SiteInstances
  // other than |source|, those SiteInstances will be notified about the new
  // focused frame.   Note that |source| may differ from |node|'s current
  // SiteInstance (e.g., this happens for cross-process window.focus() calls).
  void SetFocusedFrame(FrameTreeNode* node, SiteInstance* source);

  // Creates a RenderViewHostImpl for a given |site_instance| in the tree.
  //
  // The RenderFrameHostImpls and the RenderFrameProxyHosts will share ownership
  // of this object.
  scoped_refptr<RenderViewHostImpl> CreateRenderViewHost(
      SiteInstance* site_instance,
      int32_t main_frame_routing_id,
      bool swapped_out,
      bool renderer_initiated_creation);

  // Returns the existing RenderViewHost for a new RenderFrameHost.
  // There should always be such a RenderViewHost, because the main frame
  // RenderFrameHost for each SiteInstance should be created before subframes.
  scoped_refptr<RenderViewHostImpl> GetRenderViewHost(
      SiteInstance* site_instance);

  // Returns the ID used for the RenderViewHost associated with
  // |site_instance_group|.
  RenderViewHostMapId GetRenderViewHostMapId(
      SiteInstanceGroup* site_instance_group) const;

  // Registers a RenderViewHost so that it can be reused by other frames
  // whose SiteInstance maps to the same RenderViewHostMapId.
  //
  // This method does not take ownership of|rvh|.
  //
  // NOTE: This method CHECK fails if a RenderViewHost is already registered for
  // |rvh|'s SiteInstance.
  //
  // ALSO NOTE: After calling RegisterRenderViewHost, UnregisterRenderViewHost
  // *must* be called for |rvh| when it is destroyed or put into the
  // BackForwardCache, to prevent FrameTree::CreateRenderViewHost from trying to
  // reuse it.
  void RegisterRenderViewHost(RenderViewHostMapId id, RenderViewHostImpl* rvh);

  // Unregisters the RenderViewHostImpl that's available for reuse for a
  // particular RenderViewHostMapId. NOTE: This method CHECK fails if it is
  // called for a |render_view_host| that is not currently set for reuse.
  void UnregisterRenderViewHost(RenderViewHostMapId id,
                                RenderViewHostImpl* render_view_host);

  // This is called when the frame is about to be removed and started to run
  // unload handlers.
  void FrameUnloading(FrameTreeNode* frame);

  // This is only meant to be called by FrameTreeNode. Triggers calling
  // the listener installed by SetFrameRemoveListener.
  void FrameRemoved(FrameTreeNode* frame);

  void DidStartLoadingNode(FrameTreeNode& node,
                           bool should_show_loading_ui,
                           bool was_previously_loading);
  void DidStopLoadingNode(FrameTreeNode& node);
  void DidCancelLoading();

  // Returns this FrameTree's total load progress. If the `root_` FrameTreeNode
  // is navigating returns `blink::kInitialLoadProgress`.
  double GetLoadProgress();

  // Returns true if at least one of the nodes in this FrameTree is loading.
  bool IsLoading() const;

  // Set page-level focus in all SiteInstances involved in rendering
  // this FrameTree, not including the current main frame's
  // SiteInstance. The focus update will be sent via the main frame's proxies
  // in those SiteInstances.
  void ReplicatePageFocus(bool is_focused);

  // Updates page-level focus for this FrameTree in the subframe renderer
  // identified by |instance|.
  void SetPageFocus(SiteInstance* instance, bool is_focused);

  // Walks the current frame tree and registers any origins matching |origin|,
  // either the last committed origin of a RenderFrameHost or the origin
  // associated with a NavigationRequest that has been assigned to a
  // SiteInstance, as not-opted-in for origin isolation.
  void RegisterExistingOriginToPreventOptInIsolation(
      const url::Origin& previously_visited_origin,
      NavigationRequest* navigation_request_to_exclude);

  NavigationControllerImpl& controller() { return navigator_.controller(); }
  Navigator& navigator() { return navigator_; }

  // Another page accessed the initial empty main document, which means it
  // is no longer safe to display a pending URL without risking a URL spoof.
  void DidAccessInitialMainDocument();

  bool has_accessed_initial_main_document() const {
    return has_accessed_initial_main_document_;
  }

  void ResetHasAccessedInitialMainDocument() {
    has_accessed_initial_main_document_ = false;
  }

  bool IsHidden() const { return delegate_->IsHidden(); }

  // Stops all ongoing navigations in each of the nodes of this FrameTree.
  void StopLoading();

  // Prepares this frame tree for destruction, cleaning up the internal state
  // and firing the appropriate events like FrameDeleted.
  // Must be called before FrameTree is destroyed.
  void Shutdown();

  bool IsBeingDestroyed() const { return is_being_destroyed_; }

  base::SafeRef<FrameTree> GetSafeRef();

  // Walks up the FrameTree chain and focuses the FrameTreeNode where
  // each inner FrameTree is attached.
  void FocusOuterFrameTrees();

 private:
  friend class FrameTreeTest;
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplBrowserTest, RemoveFocusedFrame);

  // Returns a range to iterate over all FrameTreeNodes in the frame tree in
  // breadth-first traversal order, skipping the subtree rooted at
  // |node|, but including |node| itself.
  NodeRange NodesExceptSubtree(FrameTreeNode* node);

  const raw_ptr<Delegate> delegate_;

  // These delegates are installed into all the RenderViewHosts and
  // RenderFrameHosts that we create.
  raw_ptr<RenderFrameHostDelegate> render_frame_delegate_;
  raw_ptr<RenderViewHostDelegate> render_view_delegate_;
  raw_ptr<RenderWidgetHostDelegate> render_widget_delegate_;
  raw_ptr<RenderFrameHostManager::Delegate> manager_delegate_;
  raw_ptr<PageDelegate> page_delegate_;

  // The Navigator object responsible for managing navigations on this frame
  // tree. Each FrameTreeNode will default to using it for navigation tasks in
  // the frame.
  Navigator navigator_;

  // Map of RenderViewHostMapId to RenderViewHost. This allows us to look up the
  // RenderViewHost for a given SiteInstance when creating RenderFrameHosts.
  // Each RenderViewHost maintains a refcount and is deleted when there are no
  // more RenderFrameHosts or RenderFrameProxyHosts using it.
  RenderViewHostMap render_view_host_map_;

  // This is an owned ptr to the root FrameTreeNode, which never changes over
  // the lifetime of the FrameTree. It is not a scoped_ptr because we need the
  // pointer to remain valid even while the FrameTreeNode is being destroyed,
  // since it's common for a node to test whether it's the root node.
  raw_ptr<FrameTreeNode> root_;

  int focused_frame_tree_node_id_;

  // Overall load progress.
  double load_progress_;

  // Whether the initial empty page has been accessed by another page, making it
  // unsafe to show the pending URL. Usually false unless another window tries
  // to modify the blank page.  Always false after the first commit.
  bool has_accessed_initial_main_document_ = false;

  // Indicates type of frame tree.
  const Type type_;

  bool is_being_destroyed_ = false;

#if DCHECK_IS_ON()
  // Whether Shutdown() was called.
  bool was_shut_down_ = false;
#endif

  base::WeakPtrFactory<FrameTree> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_H_
