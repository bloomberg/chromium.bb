// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_NODE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node_blame_context.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_type.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"

#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class NavigationRequest;
class RenderFrameHostImpl;
class NavigationEntryImpl;

// When a page contains iframes, its renderer process maintains a tree structure
// of those frames. We are mirroring this tree in the browser process. This
// class represents a node in this tree and is a wrapper for all objects that
// are frame-specific (as opposed to page-specific).
//
// Each FrameTreeNode has a current RenderFrameHost, which can change over
// time as the frame is navigated. Any immediate subframes of the current
// document are tracked using FrameTreeNodes owned by the current
// RenderFrameHost, rather than as children of FrameTreeNode itself. This
// allows subframe FrameTreeNodes to stay alive while a RenderFrameHost is
// still alive - for example while pending deletion, after a new current
// RenderFrameHost has replaced it.
class CONTENT_EXPORT FrameTreeNode {
 public:
  class Observer {
   public:
    // Invoked when a FrameTreeNode is being destroyed.
    virtual void OnFrameTreeNodeDestroyed(FrameTreeNode* node) {}

    // Invoked when a FrameTreeNode becomes focused.
    virtual void OnFrameTreeNodeFocused(FrameTreeNode* node) {}

    virtual ~Observer() = default;
  };

  static const int kFrameTreeNodeInvalidId;

  // Returns the FrameTreeNode with the given global |frame_tree_node_id|,
  // regardless of which FrameTree it is in.
  static FrameTreeNode* GloballyFindByID(int frame_tree_node_id);

  // Returns the FrameTreeNode for the given |rfh|. Same as
  // rfh->frame_tree_node(), but also supports nullptrs.
  static FrameTreeNode* From(RenderFrameHost* rfh);

  // Callers are are expected to initialize sandbox flags separately after
  // calling the constructor.
  FrameTreeNode(
      FrameTree* frame_tree,
      RenderFrameHostImpl* parent,
      blink::mojom::TreeScopeType tree_scope_type,
      const std::string& name,
      const std::string& unique_name,
      bool is_created_by_script,
      const base::UnguessableToken& devtools_frame_token,
      const blink::mojom::FrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType owner_type,
      const blink::FramePolicy& frame_owner);

  FrameTreeNode(const FrameTreeNode&) = delete;
  FrameTreeNode& operator=(const FrameTreeNode&) = delete;

  ~FrameTreeNode();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsMainFrame() const;

  // Clears any state in this node which was set by the document itself (CSP &
  // UserActivationState) and notifies proxies as appropriate. Invoked after
  // committing navigation to a new document (since the new document comes with
  // a fresh set of CSP).
  // TODO(arthursonzogni): Remove this function. The frame/document must not be
  // left temporarily with lax state.
  void ResetForNavigation();

  FrameTree* frame_tree() const { return frame_tree_; }
  Navigator& navigator() { return frame_tree()->navigator(); }

  RenderFrameHostManager* render_manager() { return &render_manager_; }
  int frame_tree_node_id() const { return frame_tree_node_id_; }
  const std::string& frame_name() const {
    return render_manager_.current_replication_state().name;
  }

  const std::string& unique_name() const {
    return render_manager_.current_replication_state().unique_name;
  }

  // See comment on the member declaration.
  const base::UnguessableToken& devtools_frame_token() const {
    return devtools_frame_token_;
  }

  size_t child_count() const { return current_frame_host()->child_count(); }

  RenderFrameHostImpl* parent() const { return parent_; }

  // See `RenderFrameHost::GetParentOrOuterDocument()` for
  // documentation.
  RenderFrameHostImpl* GetParentOrOuterDocument();

  // See `RenderFrameHostImpl::GetParentOrOuterDocumentOrEmbedder()` for
  // documentation.
  RenderFrameHostImpl* GetParentOrOuterDocumentOrEmbedder();

  FrameTreeNode* opener() const { return opener_; }

  FrameTreeNode* original_opener() const { return original_opener_; }

  const absl::optional<base::UnguessableToken>& opener_devtools_frame_token() {
    return opener_devtools_frame_token_;
  }

  // Returns the type of the frame. Refer to frame_type.h for the details.
  FrameType GetFrameType() const;

  // Assigns a new opener for this node and, if |opener| is non-null, registers
  // an observer that will clear this node's opener if |opener| is ever
  // destroyed.
  void SetOpener(FrameTreeNode* opener);

  // Assigns the initial opener for this node, and if |opener| is non-null,
  // registers an observer that will clear this node's opener if |opener| is
  // ever destroyed. The value set here is the root of the tree.
  //
  // It is not possible to change the opener once it was set.
  void SetOriginalOpener(FrameTreeNode* opener);

  // Assigns an opener frame id for this node. This string id is only set once
  // and cannot be changed. It persists, even if the |opener| is destroyed. It
  // is used for attribution in the DevTools frontend.
  void SetOpenerDevtoolsFrameToken(
      base::UnguessableToken opener_devtools_frame_token);

  FrameTreeNode* child_at(size_t index) const {
    return current_frame_host()->child_at(index);
  }

  // Returns the URL of the last committed page in the current frame.
  const GURL& current_url() const {
    return current_frame_host()->GetLastCommittedURL();
  }

  // Sets the last committed URL for this frame.
  void SetCurrentURL(const GURL& url);

  // Sets `is_on_initial_empty_document_` to false.
  void SetNotOnInitialEmptyDocument() { is_on_initial_empty_document_ = false; }

  // Returns false if the frame has committed a document that is not the initial
  // empty document, or if the current document's input stream has been opened
  // with document.open(), causing the document to lose its "initial empty
  // document" status. For more details, see the definition of
  // `is_on_initial_empty_document_`.
  bool is_on_initial_empty_document() const {
    return is_on_initial_empty_document_;
  }

  // Sets `is_on_initial_empty_document_` to
  // false. Must only be called after the current document's input stream has
  // been opened with document.open().
  void DidOpenDocumentInputStream() { is_on_initial_empty_document_ = false; }

  // Returns whether the frame's owner element in the parent document is
  // collapsed, that is, removed from the layout as if it did not exist, as per
  // request by the embedder (of the content/ layer).
  bool is_collapsed() const { return is_collapsed_; }

  // Sets whether to collapse the frame's owner element in the parent document,
  // that is, to remove it from the layout as if it did not exist, as per
  // request by the embedder (of the content/ layer). Cannot be called for main
  // frames.
  //
  // This only has an effect for <iframe> owner elements, and is a no-op when
  // called on sub-frames hosted in <frame>, <object>, and <embed> elements.
  void SetCollapsed(bool collapsed);

  // Returns the origin of the last committed page in this frame.
  // WARNING: To get the last committed origin for a particular
  // RenderFrameHost, use RenderFrameHost::GetLastCommittedOrigin() instead,
  // which will behave correctly even when the RenderFrameHost is not the
  // current one for this frame (such as when it's pending deletion).
  const url::Origin& current_origin() const {
    return render_manager_.current_replication_state().origin;
  }

  // Set the current name and notify proxies about the update.
  void SetFrameName(const std::string& name, const std::string& unique_name);

  // Returns the latest frame policy (sandbox flags and container policy) for
  // this frame. This includes flags inherited from parent frames and the latest
  // flags from the <iframe> element hosting this frame. The returned policies
  // may not yet have taken effect, since "sandbox" and "allow" attribute
  // updates in an <iframe> element take effect on next navigation. To retrieve
  // the currently active policy for this frame, use effective_frame_policy().
  const blink::FramePolicy& pending_frame_policy() const {
    return pending_frame_policy_;
  }

  // Update this frame's sandbox flags and container policy.  This is called
  // when a parent frame updates the "sandbox" attribute in the <iframe> element
  // for this frame, or any of the attributes which affect the container policy
  // ("allowfullscreen", "allowpaymentrequest", "allow", and "src".)
  // These policies won't take effect until next navigation.  If this frame's
  // parent is itself sandboxed, the parent's sandbox flags are combined with
  // those in |frame_policy|.
  // Attempting to change the container policy on the main frame will have no
  // effect.
  void SetPendingFramePolicy(blink::FramePolicy frame_policy);

  // Returns the currently active frame policy for this frame, including the
  // sandbox flags which were present at the time the document was loaded, and
  // the permissions policy container policy, which is set by the iframe's
  // allowfullscreen, allowpaymentrequest, and allow attributes, along with the
  // origin of the iframe's src attribute (which may be different from the URL
  // of the document currently loaded into the frame). This does not include
  // policy changes that have been made by updating the containing iframe
  // element attributes since the frame was last navigated; use
  // pending_frame_policy() for those.
  const blink::FramePolicy& effective_frame_policy() const {
    return render_manager_.current_replication_state().frame_policy;
  }

  const blink::mojom::FrameOwnerProperties& frame_owner_properties() {
    return frame_owner_properties_;
  }

  void set_frame_owner_properties(
      const blink::mojom::FrameOwnerProperties& frame_owner_properties) {
    frame_owner_properties_ = frame_owner_properties;
  }

  const network::mojom::ContentSecurityPolicy* csp_attribute() {
    return csp_attribute_.get();
  }

  void set_csp_attribute(
      network::mojom::ContentSecurityPolicyPtr parsed_csp_attribute) {
    csp_attribute_ = std::move(parsed_csp_attribute);
  }

  // Reflects the 'anonymous' attribute of the corresponding iframe html
  // element.
  bool anonymous() const { return anonymous_; }
  void set_anonymous(bool anonymous) { anonymous_ = anonymous; }

  bool HasSameOrigin(const FrameTreeNode& node) const {
    return render_manager_.current_replication_state().origin.IsSameOriginWith(
        node.current_replication_state().origin);
  }

  const blink::mojom::FrameReplicationState& current_replication_state() const {
    return render_manager_.current_replication_state();
  }

  RenderFrameHostImpl* current_frame_host() const {
    return render_manager_.current_frame_host();
  }

  // Returns true if this node is in a loading state.
  bool IsLoading() const;

  // Returns true if this node has a cross-document navigation in progress.
  bool HasPendingCrossDocumentNavigation() const;

  NavigationRequest* navigation_request() { return navigation_request_.get(); }

  // Transfers the ownership of the NavigationRequest to |render_frame_host|.
  // From ReadyToCommit to DidCommit, the NavigationRequest is owned by the
  // RenderFrameHost that is committing the navigation.
  void TransferNavigationRequestOwnership(
      RenderFrameHostImpl* render_frame_host);

  // Takes ownership of |navigation_request| and makes it the current
  // NavigationRequest of this frame. This corresponds to the start of a new
  // navigation. If there was an ongoing navigation request before calling this
  // function, it is canceled. |navigation_request| should not be null.
  void CreatedNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Resets the current navigation request. If |keep_state| is true, any state
  // created by the NavigationRequest (e.g. speculative RenderFrameHost,
  // loading state) will not be reset by the function.
  void ResetNavigationRequest(bool keep_state);

  // A RenderFrameHost in this node started loading.
  // |should_show_loading_ui| indicates whether this navigation should be
  // visible in the UI. True for cross-document navigations and navigations
  // intercepted by appHistory's transitionWhile().
  // |was_previously_loading| is false if the FrameTree was not loading before.
  // The caller is required to provide this boolean as the delegate should only
  // be notified if the FrameTree went from non-loading to loading state.
  // However, when it is called, the FrameTree should be in a loading state.
  void DidStartLoading(bool should_show_loading_ui,
                       bool was_previously_loading);

  // A RenderFrameHost in this node stopped loading.
  void DidStopLoading();

  // The load progress for a RenderFrameHost in this node was updated to
  // |load_progress|. This will notify the FrameTree which will in turn notify
  // the WebContents.
  void DidChangeLoadProgress(double load_progress);

  // Called when the user directed the page to stop loading. Stops all loads
  // happening in the FrameTreeNode. This method should be used with
  // FrameTree::ForEach to stop all loads in the entire FrameTree.
  bool StopLoading();

  // Returns the time this frame was last focused.
  base::TimeTicks last_focus_time() const { return last_focus_time_; }

  // Called when this node becomes focused.  Updates the node's last focused
  // time and notifies observers.
  void DidFocus();

  // Called when the user closed the modal dialogue for BeforeUnload and
  // cancelled the navigation. This should stop any load happening in the
  // FrameTreeNode.
  void BeforeUnloadCanceled();

  // Returns the BlameContext associated with this node.
  FrameTreeNodeBlameContext& blame_context() { return blame_context_; }

  // Updates the user activation state in the browser frame tree and in the
  // frame trees in all renderer processes except the renderer for this node
  // (which initiated the update).  Returns |false| if the update tries to
  // consume an already consumed/expired transient state, |true| otherwise.  See
  // the comment on user_activation_state_ below.
  //
  // The |notification_type| parameter is used for histograms, only for the case
  // |update_state == kNotifyActivation|.
  bool UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type);

  // Returns the sandbox flags currently in effect for this frame. This includes
  // flags inherited from parent frames, the currently active flags from the
  // <iframe> element hosting this frame, as well as any flags set from a
  // Content-Security-Policy HTTP header. This does not include flags that have
  // have been updated in an <iframe> element but have not taken effect yet; use
  // pending_frame_policy() for those. To see the flags which will take effect
  // on navigation (which does not include the CSP-set flags), use
  // effective_frame_policy().
  network::mojom::WebSandboxFlags active_sandbox_flags() const {
    return render_manager_.current_replication_state().active_sandbox_flags;
  }

  // Returns whether the frame received a user gesture on a previous navigation
  // on the same eTLD+1.
  bool has_received_user_gesture_before_nav() const {
    return render_manager_.current_replication_state()
        .has_received_user_gesture_before_nav;
  }

  // When a tab is discarded, WebContents sets was_discarded on its
  // root FrameTreeNode.
  // In addition, when a child frame is created, this bit is passed on from
  // parent to child.
  // When a navigation request is created, was_discarded is passed on to the
  // request and reset to false in FrameTreeNode.
  void set_was_discarded() { was_discarded_ = true; }
  bool was_discarded() const { return was_discarded_; }

  // Returns the sticky bit of the User Activation v2 state of the
  // |FrameTreeNode|.
  bool HasStickyUserActivation() const {
    return user_activation_state_.HasBeenActive();
  }

  // Returns the transient bit of the User Activation v2 state of the
  // |FrameTreeNode|.
  bool HasTransientUserActivation() {
    return user_activation_state_.IsActive();
  }

  // Remove history entries for all frames created by script in this frame's
  // subtree. If a frame created by a script is removed, then its history entry
  // will never be reused - this saves memory.
  void PruneChildFrameNavigationEntries(NavigationEntryImpl* entry);

  blink::FrameOwnerElementType frame_owner_element_type() const {
    return frame_owner_element_type_;
  }

  blink::mojom::TreeScopeType tree_scope_type() const {
    return tree_scope_type_;
  }

  // The initial popup URL for new window opened using:
  // `window.open(initial_popup_url)`.
  // An empty GURL otherwise.
  //
  // [WARNING] There is no guarantee the FrameTreeNode will ever host a
  // document served from this URL. The FrameTreeNode always starts hosting the
  // initial empty document and attempts a navigation toward this URL. However
  // the navigation might be delayed, redirected and even cancelled.
  void SetInitialPopupURL(const GURL& initial_popup_url);
  const GURL& initial_popup_url() const { return initial_popup_url_; }

  // The origin of the document that used window.open() to create this frame.
  // Otherwise, an opaque Origin with a nonce different from all previously
  // existing Origins.
  void SetPopupCreatorOrigin(const url::Origin& popup_creator_origin);
  const url::Origin& popup_creator_origin() const {
    return popup_creator_origin_;
  }

  // Sets the associated FrameTree for this node. The node can change FrameTrees
  // when blink::features::Prerender2 is enabled, which allows a page loaded in
  // the prerendered FrameTree to be used for a navigation in the primary frame
  // tree.
  void SetFrameTree(FrameTree& frame_tree);

  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedValue context) const;
  void WriteIntoTrace(
      perfetto::TracedProto<perfetto::protos::pbzero::FrameTreeNodeInfo> proto);

  // Returns true the node is navigating, i.e. it has an associated
  // NavigationRequest.
  bool HasNavigation();

  // Fenced frames (meta-bug crbug.com/1111084):
  // Note that these two functions cannot be invoked from a FrameTree's or
  // its root node's constructor since they require the frame tree and the
  // root node to be completely constructed.
  //
  // Returns false if fenced frames are disabled. Returns true if the feature is
  // enabled and if |this| is a fenced frame. Returns false for
  // iframes embedded in a fenced frame. To clarify: for the MPArch
  // implementation this only returns true if |this| is the actual
  // root node of the inner FrameTree and not the proxy FrameTreeNode in the
  // outer FrameTree.
  bool IsFencedFrameRoot() const;

  // Returns false if fenced frames are disabled. Returns true if the
  // feature is enabled and if |this| or any of its ancestor nodes is a
  // fenced frame.
  bool IsInFencedFrameTree() const;

  // Returns a valid nonce if `IsInFencedFrameTree()` returns true for `this`.
  // Returns nullopt otherwise. See comments on `fenced_frame_nonce_` for more
  // details.
  absl::optional<base::UnguessableToken> fenced_frame_nonce() {
    return fenced_frame_nonce_;
  }

  // If applicable, set the fenced frame nonce. See comment on
  // fenced_frame_nonce() for when it is set to a non-null value. Invoked
  // by FrameTree::Init() or FrameTree::AddFrame().
  void SetFencedFrameNonceIfNeeded();

  // Helper for GetParentOrOuterDocument/GetParentOrOuterDocumentOrEmbedder.
  // Do not use directly.
  RenderFrameHostImpl* GetParentOrOuterDocumentHelper(bool escape_guest_view);

  // Sets the unique_name and name fields on replication_state_. To be used in
  // prerender activation to make sure the FrameTreeNode replication state is
  // correct after the RenderFrameHost is moved between FrameTreeNodes. The
  // renderers should already have the correct value, so unlike
  // FrameTreeNode::SetFrameName, we do not notify them here.
  // TODO(https://crbug.com/1237091): Remove this once the BrowsingContextState
  //  is implemented to utilize the new path.
  void set_frame_name_for_activation(const std::string& unique_name,
                                     const std::string& name) {
    render_manager_.browsing_context_state()->set_frame_name(unique_name, name);
  }

  // Returns true if error page isolation is enabled.
  bool IsErrorPageIsolationEnabled() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessPermissionsPolicyBrowserTest,
                           ContainerPolicyDynamic);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessPermissionsPolicyBrowserTest,
                           ContainerPolicySandboxDynamic);

  // Called by the destructor. When `this` is an outer dummy FrameTreeNode
  // representing an inner FrameTree, this method destroys said inner FrameTree.
  void DestroyInnerFrameTreeIfExists();

  class OpenerDestroyedObserver;

  // The |notification_type| parameter is used for histograms only.
  bool NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type);

  bool ConsumeTransientUserActivation();

  bool ClearUserActivation();

  // Verify that the renderer process is allowed to set user activation on this
  // frame by checking whether this frame's RenderWidgetHost had previously seen
  // an input event that might lead to user activation. If user activation
  // should be allowed, this returns true and also clears corresponding pending
  // user activation state in the widget. Otherwise, this returns false.
  bool VerifyUserActivation();

  // The next available browser-global FrameTreeNode ID.
  static int next_frame_tree_node_id_;

  // The FrameTree that owns us.
  raw_ptr<FrameTree> frame_tree_;  // not owned.

  // A browser-global identifier for the frame in the page, which stays stable
  // even if the frame does a cross-process navigation.
  const int frame_tree_node_id_;

  // The RenderFrameHost owning this FrameTreeNode, which cannot change for the
  // life of this FrameTreeNode. |nullptr| if this node is the root.
  const raw_ptr<RenderFrameHostImpl> parent_;

  // The frame that opened this frame, if any.  Will be set to null if the
  // opener is closed, or if this frame disowns its opener by setting its
  // window.opener to null.
  raw_ptr<FrameTreeNode> opener_ = nullptr;

  // An observer that clears this node's |opener_| if the opener is destroyed.
  // This observer is added to the |opener_|'s observer list when the |opener_|
  // is set to a non-null node, and it is removed from that list when |opener_|
  // changes or when this node is destroyed.  It is also cleared if |opener_|
  // is disowned.
  std::unique_ptr<OpenerDestroyedObserver> opener_observer_;

  // The frame that opened this frame, if any. Contrary to opener_, this
  // cannot be changed unless the original opener is destroyed.
  raw_ptr<FrameTreeNode> original_opener_ = nullptr;

  // The devtools frame token of the frame which opened this frame. This is
  // not cleared even if the opener is destroyed or disowns the frame.
  absl::optional<base::UnguessableToken> opener_devtools_frame_token_;

  // An observer that clears this node's |original_opener_| if the opener is
  // destroyed.
  std::unique_ptr<OpenerDestroyedObserver> original_opener_observer_;

  // When created by an opener, the URL specified in window.open(url)
  // Please refer to {Get,Set}InitialPopupURL() documentation.
  GURL initial_popup_url_;

  // When created using window.open, the origin of the creator.
  // Please refer to {Get,Set}PopupCreatorOrigin() documentation.
  url::Origin popup_creator_origin_;

  // Whether this frame is still on the initial about:blank document or the
  // synchronously committed about:blank document committed at frame creation,
  // and its "initial empty document"-ness is still true.
  // This will be false if either of these has happened:
  // - SetCurrentUrl() was called after committing a document that is not the
  //   initial about:blank document or the synchronously committed about:blank
  //   document, per
  //   https://html.spec.whatwg.org/multipage/browsers.html#creating-browsing-contexts:is-initial-about:blank
  // - The document's input stream has been opened with document.open(), per
  //   https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#opening-the-input-stream:is-initial-about:blank
  // NOTE: we treat both the "initial about:blank document" and the
  // "synchronously committed about:blank document" as the initial empty
  // document. In the future, we plan to remove the synchronous about:blank
  // commit so that this state will only be true if the frame is on the
  // "initial about:blank document". See also:
  // - https://github.com/whatwg/html/issues/6863
  // - https://crbug.com/1215096
  bool is_on_initial_empty_document_ = true;

  // Whether the frame's owner element in the parent document is collapsed.
  bool is_collapsed_ = false;

  // The type of frame owner for this frame. This is only relevant for non-main
  // frames.
  const blink::FrameOwnerElementType frame_owner_element_type_ =
      blink::FrameOwnerElementType::kNone;

  // The tree scope type of frame owner element, i.e. whether the element is in
  // the document tree (https://dom.spec.whatwg.org/#document-trees) or the
  // shadow tree (https://dom.spec.whatwg.org/#shadow-trees). This is only
  // relevant for non-main frames.
  const blink::mojom::TreeScopeType tree_scope_type_ =
      blink::mojom::TreeScopeType::kDocument;

  // Track the pending sandbox flags and container policy for this frame. When a
  // parent frame dynamically updates 'sandbox', 'allow', 'allowfullscreen',
  // 'allowpaymentrequest' or 'src' attributes, the updated policy for the frame
  // is stored here, and transferred into
  // render_manager_.current_replication_state().frame_policy when they take
  // effect on the next frame navigation.
  blink::FramePolicy pending_frame_policy_;

  // Whether the frame was created by javascript.  This is useful to prune
  // history entries when the frame is removed (because frames created by
  // scripts are never recreated with the same unique name - see
  // https://crbug.com/500260).
  const bool is_created_by_script_;

  // Used for devtools instrumentation and trace-ability. The token is
  // propagated to Blink's LocalFrame and both Blink and content/
  // can tag calls and requests with this token in order to attribute them
  // to the context frame.
  // |devtools_frame_token_| is only defined by the browser process and is never
  // sent back from the renderer in the control calls. It should be never used
  // to look up the FrameTreeNode instance.
  const base::UnguessableToken devtools_frame_token_;

  // Tracks the scrolling and margin properties for this frame.  These
  // properties affect the child renderer but are stored on its parent's
  // frame element.  When this frame's parent dynamically updates these
  // properties, we update them here too.
  //
  // Note that dynamic updates only take effect on the next frame navigation.
  blink::mojom::FrameOwnerProperties frame_owner_properties_;

  // Contains the current parsed value of the 'csp' attribute of this frame.
  network::mojom::ContentSecurityPolicyPtr csp_attribute_;

  // Reflects the 'anonymous' attribute of the corresponding iframe html
  // element.
  bool anonymous_ = false;

  // Owns an ongoing NavigationRequest until it is ready to commit. It will then
  // be reset and a RenderFrameHost will be responsible for the navigation.
  std::unique_ptr<NavigationRequest> navigation_request_;

  // List of objects observing this FrameTreeNode.
  base::ObserverList<Observer>::Unchecked observers_;

  base::TimeTicks last_focus_time_;

  bool was_discarded_ = false;

  // The user activation state of the current frame.  See |UserActivationState|
  // for details on how this state is maintained.
  blink::UserActivationState user_activation_state_;

  // A helper for tracing the snapshots of this FrameTreeNode and attributing
  // browser process activities to this node (when possible).  It is unrelated
  // to the core logic of FrameTreeNode.
  FrameTreeNodeBlameContext blame_context_;

  // Fenced Frames:
  // Nonce used in the net::IsolationInfo and blink::StorageKey for a fenced
  // frame and any iframes nested within it. Not set if this frame is not in a
  // fenced frame's FrameTree. Note that this could be a field in FrameTree for
  // the MPArch version but for the shadow DOM version we need to keep it here
  // since the fenced frame root is not a main frame for the latter. The value
  // of the nonce will be the same for all of the the frames inside a fenced
  // frame tree. If there is a nested fenced frame it will have a different
  // nonce than its parent fenced frame. The nonce will stay the same across
  // navigations because it is always used in conjunction with other fields of
  // the keys. If the navigation is same-origin/site then the same network stack
  // partition/storage will be reused and if it's cross-origin/site then other
  // parts of the key will change and so, even with the same nonce, another
  // partition will be used.
  absl::optional<base::UnguessableToken> fenced_frame_nonce_;

  // Manages creation and swapping of RenderFrameHosts for this frame.
  //
  // This field needs to be declared last, because destruction of
  // RenderFrameHostManager may call arbitrary callbacks (e.g. via
  // WebContentsObserver::DidFinishNavigation fired after RenderFrameHostManager
  // destructs a RenderFrameHostImpl and its NavigationRequest).  Such callbacks
  // may try to use FrameTreeNode's fields above - this would be an undefined
  // behavior if the fields (even trivially-destructible ones) were destructed
  // before the RenderFrameHostManager's destructor runs.  See also
  // https://crbug.com/1157988.
  RenderFrameHostManager render_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_NODE_H_
