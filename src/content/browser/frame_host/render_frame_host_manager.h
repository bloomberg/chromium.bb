// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/frame_host/back_forward_cache_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/should_swap_browsing_instance.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/referrer.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace content {
class FrameTreeNode;
class NavigationControllerImpl;
class NavigationEntry;
class NavigationRequest;
class NavigatorTest;
class RenderFrameHostManagerTest;
class RenderFrameProxyHost;
class RenderViewHost;
class RenderViewHostImpl;
class RenderWidgetHostView;
class TestWebContents;
struct FrameReplicationState;

// Manages RenderFrameHosts for a FrameTreeNode. It maintains a
// current_frame_host() which is the content currently visible to the user. When
// a frame is told to navigate to a different web site (as determined by
// SiteInstance), it will replace its current RenderFrameHost with a new
// RenderFrameHost dedicated to the new SiteInstance, possibly in a new process.
//
// Cross-process navigation works like this:
//
// - RFHM::Navigate determines whether the destination is cross-site, and if so,
//   it creates a pending_render_frame_host_.
//
// - The pending RFH is created in the "navigations suspended" state, meaning no
//   navigation messages are sent to its renderer until the beforeunload handler
//   has a chance to run in the current RFH.
//
// - The current RFH runs its beforeunload handler. If it returns false, we
//   cancel all the pending logic. Otherwise we allow the pending RFH to send
//   the navigation request to its renderer.
//
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread for the
//   main resource load from the pending RFH. It creates a
//   CrossSiteResourceHandler to check whether a process transfer is needed when
//   the request is ready to commit.
//
// - When RDH receives a response, the MimeTypeResourceHandler determines
//   whether it is a navigation type that doesn't commit (e.g. download, 204 or
//   error page). If so, it sends a message to the new renderer causing it to
//   cancel the request, and the request (e.g. the download) proceeds. In this
//   case, the pending RFH will never become the current RFH, but it remains
//   until the next DidNavigate event for this WebContentsImpl.
//
// - After RDH receives a response and determines that it is safe and not a
//   download, the CrossSiteResourceHandler checks whether a transfer for a
//   redirect is needed. If so, it pauses the network response and starts an
//   identical navigation in a new pending RFH. When the identical request is
//   later received by RDH, the response is transferred and unpaused.
//
// - Otherwise, the network response commits in the pending RFH's renderer,
//   which sends a DidCommitProvisionalLoad message back to the browser process.
//
// - RFHM::CommitPending makes visible the new RFH, and initiates the unload
//   handler in the old RFH. The unload handler will complete in the background.
//
// - RenderFrameHostManager may keep the previous RFH alive as a
//   RenderFrameProxyHost, to be used (for example) if the user goes back. The
//   process only stays live if another tab is using it, but if so, the existing
//   frame relationships will be maintained.
class CONTENT_EXPORT RenderFrameHostManager
    : public SiteInstanceImpl::Observer {
 public:
  using RenderFrameProxyHostMap =
      std::unordered_map<int32_t /* SiteInstance id */,
                         std::unique_ptr<RenderFrameProxyHost>>;

  // Functions implemented by our owner that we need.
  //
  // TODO(brettw) Clean this up! These are all the functions in WebContentsImpl
  // that are required to run this class. The design should probably be better
  // such that these are more clear.
  //
  // There is additional complexity that some of the functions we need in
  // WebContentsImpl are inherited and non-virtual. These are named with
  // "RenderManager" so that the duplicate implementation of them will be clear.
  class CONTENT_EXPORT Delegate {
   public:
    // Initializes the given renderer if necessary and creates the view ID
    // corresponding to this view host. If this method is not called and the
    // process is not shared, then the WebContentsImpl will act as though the
    // renderer is not running (i.e., it will render "sad tab"). This method is
    // automatically called from LoadURL.
    virtual bool CreateRenderViewForRenderManager(
        RenderViewHost* render_view_host,
        int opener_frame_routing_id,
        int proxy_routing_id,
        const base::UnguessableToken& frame_token,
        const base::UnguessableToken& devtools_frame_token,
        const FrameReplicationState& replicated_frame_state) = 0;
    virtual void CreateRenderWidgetHostViewForRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual bool CreateRenderFrameForRenderManager(
        RenderFrameHost* render_frame_host,
        int proxy_routing_id,
        int opener_routing_id,
        int parent_routing_id,
        int previous_sibling_routing_id) = 0;
    virtual void BeforeUnloadFiredFromRenderManager(
        bool proceed,
        const base::TimeTicks& proceed_time,
        bool* proceed_to_fire_unload) = 0;
    virtual void RenderProcessGoneFromRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void CancelModalDialogsForRenderManager() = 0;
    virtual void NotifySwappedFromRenderManager(RenderFrameHost* old_frame,
                                                RenderFrameHost* new_frame,
                                                bool is_main_frame) = 0;
    // TODO(nasko): This should be removed once extensions no longer use
    // NotificationService. See https://crbug.com/462682.
    virtual void NotifyMainFrameSwappedFromRenderManager(
        RenderFrameHost* old_frame,
        RenderFrameHost* new_frame) = 0;
    virtual NavigationControllerImpl& GetControllerForRenderManager() = 0;

    // Returns true if the location bar should be focused by default rather than
    // the page contents. The view calls this function when the tab is focused
    // to see what it should do.
    virtual bool FocusLocationBarByDefault() = 0;

    // Focuses the location bar.
    virtual void SetFocusToLocationBar() = 0;

    // Returns true if views created for this delegate should be created in a
    // hidden state.
    virtual bool IsHidden() = 0;

    // If the delegate is an inner WebContents, this method returns the
    // FrameTreeNode ID of the frame in the outer WebContents which hosts
    // the inner WebContents. Returns FrameTreeNode::kFrameTreeNodeInvalidId
    // if the delegate does not have an outer WebContents.
    virtual int GetOuterDelegateFrameTreeNodeId() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // The delegate pointer must be non-NULL and is not owned by this class. It
  // must outlive this class.
  //
  // You must call one of the Init*() methods before using this class.
  RenderFrameHostManager(FrameTreeNode* frame_tree_node, Delegate* delegate);
  ~RenderFrameHostManager();

  // Initialize this frame as the root of a new FrameTree.
  void InitRoot(SiteInstance* site_instance, bool renderer_initiated_creation);

  // Initialize this frame as the child of another frame.
  void InitChild(SiteInstance* site_instance,
                 int32_t frame_routing_id,
                 const base::UnguessableToken& frame_token);

  // Returns the currently active RenderFrameHost.
  //
  // This will be non-NULL between Init() and Shutdown(). You may want to NULL
  // check it in many cases, however. Windows can send us messages during the
  // destruction process after it has been shut down.
  RenderFrameHostImpl* current_frame_host() const {
    return render_frame_host_.get();
  }

  // TODO(creis): Remove this when we no longer use RVH for navigation.
  RenderViewHostImpl* current_host() const;

  // Returns the view associated with the current RenderViewHost, or NULL if
  // there is no current one.
  RenderWidgetHostView* GetRenderWidgetHostView() const;

  // Returns whether this manager is a main frame and belongs to a FrameTreeNode
  // that belongs to an inner WebContents.
  bool IsMainFrameForInnerDelegate();

  // If this is a RenderFrameHostManager for a main frame, this method returns
  // the FrameTreeNode for the frame in the outer WebContents (if any) that
  // contains the inner WebContents.
  FrameTreeNode* GetOuterDelegateNode();

  // Return a proxy for this frame in the parent frame's SiteInstance.  Returns
  // nullptr if this is a main frame or if such a proxy does not exist (for
  // example, if this frame is same-site with its parent OR if this frame will
  // be deleted soon and we are just waiting for the frame's unload handler).
  RenderFrameProxyHost* GetProxyToParent();

  // If this is a RenderFrameHostManager for a main frame, returns the proxy to
  // inner WebContents in the outer WebContents's SiteInstance. Returns nullptr
  // if this WebContents isn't part of inner/outer relationship.
  RenderFrameProxyHost* GetProxyToOuterDelegate();

  // If this is a RenderFrameHostManager for a main frame, removes the
  // FrameTreeNode in the outer WebContents that represents this FrameTreeNode.
  // TODO(lazyboy): This does not belong to RenderFrameHostManager, move it to
  // somehwere else.
  void RemoveOuterDelegateFrame();

  // Returns the speculative RenderFrameHost, or null if there is no speculative
  // one.
  RenderFrameHostImpl* speculative_frame_host() const {
    return speculative_render_frame_host_.get();
  }

  // Instructs the various live views to stop. Called when the user directed the
  // page to stop loading.
  void Stop();

  // Notifies the regular and pending RenderViewHosts that a load is or is not
  // happening. Even though the message is only for one of them, we don't know
  // which one so we tell both.
  void SetIsLoading(bool is_loading);

  // Confirms whether we should close the page. |proceed| indicates whether the
  // user chose to proceed. |proceed_time| is the time when the request was
  // allowed to proceed. This is called in one of the two *distinct* scenarios
  // below:
  //   1- The tab/window is closed after allowing the appropriate renderer to
  //      show the beforeunload prompt.
  //   2- The FrameTreeNode is being prepared for attaching an inner Delegate,
  //      in which case beforeunload is triggered in the current frame. This
  //      only happens for child frames.
  void BeforeUnloadCompleted(bool proceed, const base::TimeTicks& proceed_time);

  // Called when a renderer's frame navigates.
  void DidNavigateFrame(RenderFrameHostImpl* render_frame_host,
                        bool was_caused_by_user_gesture,
                        bool is_same_document_navigation,
                        bool clear_proxies_on_commit,
                        const blink::FramePolicy& frame_policy);

  // Called when this frame's opener is changed to the frame specified by
  // |opener_routing_id| in |source_site_instance|'s process.  This change
  // could come from either the current RenderFrameHost or one of the
  // proxies (e.g., window.open that targets a RemoteFrame by name).  The
  // updated opener will be forwarded to any other RenderFrameProxies and
  // RenderFrames for this FrameTreeNode.
  void DidChangeOpener(int opener_routing_id,
                       SiteInstance* source_site_instance);

  // Creates and initializes a RenderFrameHost.
  std::unique_ptr<RenderFrameHostImpl> CreateSpeculativeRenderFrame(
      SiteInstance* instance);

  // Helper method to create and initialize a RenderFrameProxyHost.
  void CreateRenderFrameProxy(SiteInstance* instance);

  // Creates proxies for a new child frame at FrameTreeNode |child| in all
  // SiteInstances for which the current frame has proxies.  This method is
  // called on the parent of a new child frame before the child leaves the
  // SiteInstance.
  void CreateProxiesForChildFrame(FrameTreeNode* child);

  // Returns the RenderFrameProxyHost for the given SiteInstance, if any.
  RenderFrameProxyHost* GetRenderFrameProxyHost(SiteInstance* instance) const;

  // If |render_frame_host| is on the pending deletion list, this deletes it.
  // Returns whether it was deleted.
  bool DeleteFromPendingList(RenderFrameHostImpl* render_frame_host);

  // BackForwardCache:
  // During a history navigation, unfreezes and swaps in a document from the
  // BackForwardCache, making it active.
  void RestoreFromBackForwardCache(
      std::unique_ptr<BackForwardCacheImpl::Entry>);

  // Deletes any proxy hosts associated with this node. Used during destruction
  // of WebContentsImpl.
  void ResetProxyHosts();

  void ClearRFHsPendingShutdown();
  void ClearWebUIInstances();

  // Returns the routing id for a RenderFrameHost or RenderFrameProxyHost
  // that has the given SiteInstance and is associated with this
  // RenderFrameHostManager. Returns MSG_ROUTING_NONE if none is found.
  int GetRoutingIdForSiteInstance(SiteInstance* site_instance);

  // Notifies the RenderFrameHostManager that a new NavigationRequest has been
  // created and set in the FrameTreeNode so that it can speculatively create a
  // new RenderFrameHost (and potentially a new process) if needed.
  void DidCreateNavigationRequest(NavigationRequest* request);

  // Called (possibly several times) during a navigation to select or create an
  // appropriate RenderFrameHost for the provided URL. The returned pointer will
  // be for the current or the speculative RenderFrameHost and the instance is
  // owned by this manager.
  RenderFrameHostImpl* GetFrameHostForNavigation(NavigationRequest* request);

  // Clean up any state for any ongoing navigation.
  void CleanUpNavigation();

  // Clears the speculative members, returning the RenderFrameHost to the caller
  // for disposal.
  std::unique_ptr<RenderFrameHostImpl> UnsetSpeculativeRenderFrameHost();

  // Notification methods to tell this RenderFrameHostManager that the frame it
  // is responsible for has started or stopped loading a document.
  void OnDidStartLoading();
  void OnDidStopLoading();

  // OnDidUpdateName gets called when a frame changes its name - it gets the new
  // |name| and the recalculated |unique_name| and replicates them into all
  // frame proxies.
  void OnDidUpdateName(const std::string& name, const std::string& unique_name);

  // Sends the newly added Content Security Policy headers to all the proxies.
  void OnDidAddContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyHeaderPtr> headers);

  // Resets Content Security Policy in all the proxies.
  void OnDidResetContentSecurityPolicy();

  // Sends updated enforcement of insecure request policy to all frame proxies
  // when the frame changes its setting.
  void OnEnforceInsecureRequestPolicy(
      blink::mojom::InsecureRequestPolicy policy);

  // Sends updated enforcement of upgrade insecure navigations set to all frame
  // proxies when the frame changes its setting.
  void OnEnforceInsecureNavigationsSet(
      const std::vector<uint32_t>& insecure_navigations_set);

  // Called when the client changes whether the frame's owner element in the
  // embedder document should be collapsed, that is, remove from the layout as
  // if it did not exist. Never called for main frames. Only has an effect for
  // <iframe> owner elements.
  void OnDidChangeCollapsedState(bool collapsed);

  // Called on a frame to notify it that its out-of-process parent frame
  // changed a property (such as allowFullscreen) on its <iframe> element.
  // Sends updated FrameOwnerProperties to the RenderFrame and to all proxies,
  // skipping the parent process.
  void OnDidUpdateFrameOwnerProperties(
      const blink::mojom::FrameOwnerProperties& properties);

  // Notify the proxies that the active sandbox flags or feature policy header
  // on the frame have been changed during page load. Sandbox flags can change
  // when set by a CSP header.
  void OnDidSetFramePolicyHeaders();

  // Send updated origin to all frame proxies when the frame navigates to a new
  // origin.
  void OnDidUpdateOrigin(const url::Origin& origin,
                         bool is_potentially_trustworthy_unique_origin);

  // Send updated ad frame type to all frame proxies at ready-to-commit time
  // when the ad status gets updated.
  void OnDidSetAdFrameType(blink::mojom::AdFrameType ad_frame_type);

  void EnsureRenderViewInitialized(RenderViewHostImpl* render_view_host,
                                   SiteInstance* instance);

  // Creates RenderFrameProxies and inactive RenderViewHosts for this frame's
  // FrameTree and for its opener chain in the given SiteInstance. This allows
  // other tabs to send cross-process JavaScript calls to their opener(s) and
  // to any other frames in the opener's FrameTree (e.g., supporting calls like
  // window.opener.opener.frames[x][y]).  Does not create proxies for the
  // subtree rooted at |skip_this_node| (e.g., if a node is being navigated, it
  // can be passed here to prevent proxies from being created for it, in
  // case it is in the same FrameTree as another node on its opener chain).
  void CreateOpenerProxies(SiteInstance* instance,
                           FrameTreeNode* skip_this_node);

  // Ensure that this frame has proxies in all SiteInstances that can discover
  // this frame by name (e.g., via window.open("", "frame_name")).  See
  // https://crbug.com/511474.
  void CreateProxiesForNewNamedFrame();

  // Returns a routing ID for the current FrameTreeNode's opener node in the
  // given SiteInstance.  May return a routing ID of either a RenderFrameHost
  // (if opener's current or pending RFH has SiteInstance |instance|) or a
  // RenderFrameProxyHost.  Returns MSG_ROUTING_NONE if there is no opener, or
  // if the opener node doesn't have a proxy for |instance|.
  int GetOpenerRoutingID(SiteInstance* instance);

  // Called on the RFHM of the inner WebContents to create a
  // RenderFrameProxyHost in its outer WebContents's SiteInstance,
  // |outer_contents_site_instance|. The frame in outer WebContents that is
  // hosting the inner WebContents is |render_frame_host|, and the frame will
  // be swapped with the proxy. Note that this method must only be called for an
  // OOPIF-based inner WebContents.
  RenderFrameProxyHost* CreateOuterDelegateProxy(
      SiteInstance* outer_contents_site_instance);

  // Called on an inner WebContents that's being detached from its outer
  // WebContents. This will delete the proxy in the
  // |outer_contents_site_instance|.
  void DeleteOuterDelegateProxy(SiteInstance* outer_contents_site_instance);

  // Tells the |render_frame_host|'s renderer that its RenderFrame is being
  // swapped for a frame in another process, and that it should create a
  // RenderFrameProxy to replace it using the |proxy| RenderFrameProxyHost.
  void SwapOuterDelegateFrame(RenderFrameHostImpl* render_frame_host,
                              RenderFrameProxyHost* proxy);

  // Sets the child RenderWidgetHostView for this frame, which must be part of
  // an inner WebContents.
  void SetRWHViewForInnerContents(RenderWidgetHostView* child_rwhv);

  // Returns the number of RenderFrameProxyHosts for this frame.
  size_t GetProxyCount();

  // Sends an IPC message to every process in the FrameTree. This should only be
  // called in the top-level RenderFrameHostManager.  |instance_to_skip|, if
  // not null, specifies the SiteInstance to which the message should not be
  // sent.
  void SendPageMessage(IPC::Message* msg, SiteInstance* instance_to_skip);

  // Returns a const reference to the map of proxy hosts. The keys are
  // SiteInstance IDs, the values are RenderFrameProxyHosts.
  const RenderFrameProxyHostMap& GetAllProxyHostsForTesting() const {
    return proxy_hosts_;
  }

  // SiteInstanceImpl::Observer
  void ActiveFrameCountIsZero(SiteInstanceImpl* site_instance) override;
  void RenderProcessGone(SiteInstanceImpl* site_instance,
                         const ChildProcessTerminationInfo& info) override;

  // Cancels and destroys the pending or speculative RenderFrameHost if they
  // match the provided |render_frame_host|.
  void CancelPendingIfNecessary(RenderFrameHostImpl* render_frame_host);

  // Updates the user activation state in all proxies of this frame.  For
  // more details, see the comment on FrameTreeNode::user_activation_state_.
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type);

  // When a frame transfers user activation to another frame via postMessage,
  // this is used to inform proxies of the target frame (via IPC) about the
  // transfer, namely that |source_rfh| is transferring user activation to this
  // frame.  Note that the IPC isn't sent to |source_rfh|'s process, since it
  // already knows about the transfer, and it also isn't sent to this frame's
  // RenderFrame, since that will be handled as part of postMessage.
  void TransferUserActivationFrom(RenderFrameHostImpl* source_rfh);

  void OnSetHadStickyUserActivationBeforeNavigation(bool value);

  // Sets up the necessary state for a new RenderViewHost.  If |proxy| is not
  // null, it creates a RenderFrameProxy in the target renderer process which is
  // used to route IPC messages.  Returns early if the RenderViewHost has
  // already been initialized for another RenderFrameHost.
  bool InitRenderView(RenderViewHostImpl* render_view_host,
                      RenderFrameProxyHost* proxy);

  // Returns the SiteInstance that should be used to host the navigation handled
  // by |navigation_request|.
  // Note: the SiteInstance returned by this function may not have an
  // initialized RenderProcessHost. It will only be initialized when
  // GetProcess() is called on the SiteInstance. In particular, calling this
  // function will never lead to a process being created for the navigation.
  scoped_refptr<SiteInstance> GetSiteInstanceForNavigationRequest(
      NavigationRequest* navigation_request);

  // Helper to initialize the current RenderFrame if it's not initialized.
  // TODO(https://crbug.com/1006814): Remove this. For now debug URLs and
  // WebView JS execution are an exception to replacing all crashed frames for
  // RenderDocument. This is a no-op if the frame is already initialized.
  bool InitializeRenderFrameForImmediateUse();

  // Prepares the FrameTreeNode for attaching an inner WebContents. This step
  // may involve replacing |current_frame_host()| with a new RenderFrameHost
  // in the same SiteInstance as the parent frame. Calling this method will
  // dispatch beforeunload event if necessary.
  void PrepareForInnerDelegateAttach(
      RenderFrameHost::PrepareForInnerWebContentsAttachCallback callback);

  // When true the FrameTreeNode is preparing a RenderFrameHost for attaching an
  // inner Delegate. During this phase new navigation requests are ignored.
  bool is_attaching_inner_delegate() const {
    return attach_to_inner_delegate_state_ != AttachToInnerDelegateState::NONE;
  }

  // Called by the delegate at the end of the attaching process.
  void set_attach_complete() {
    attach_to_inner_delegate_state_ = AttachToInnerDelegateState::ATTACHED;
  }

 private:
  friend class NavigatorTest;
  friend class RenderFrameHostManagerTest;
  friend class RenderFrameHostTester;
  friend class TestWebContents;

  enum class SiteInstanceRelation {
    // A SiteInstance in a different browsing instance from the current.
    UNRELATED,
    // A SiteInstance in the same browsing instance as the current.
    RELATED,
    // A pre-existing SiteInstance that might or might not be in the same
    // browsing instance as the current. Only used when |existing_site_instance|
    // is specified.
    PREEXISTING,
  };

  enum class AttachToInnerDelegateState {
    // There is no inner delegate attached through FrameTreeNode and no
    // attaching is in progress.
    NONE,
    // A frame is being prepared for attaching.
    PREPARE_FRAME,
    // An inner delegate attached to the delegate of this manager.
    ATTACHED
  };

  // Stores information regarding a SiteInstance targeted at a specific URL to
  // allow for comparisons without having to actually create new instances. It
  // can point to an existing one or store the details needed to create a new
  // one.
  struct CONTENT_EXPORT SiteInstanceDescriptor {
    explicit SiteInstanceDescriptor(content::SiteInstance* site_instance)
        : existing_site_instance(site_instance),
          relation(SiteInstanceRelation::PREEXISTING) {}

    SiteInstanceDescriptor(GURL dest_url,
                           SiteInstanceRelation relation_to_current);

    // Set with an existing SiteInstance to be reused.
    content::SiteInstance* existing_site_instance;

    // In case |existing_site_instance| is null, specify a destination URL.
    GURL dest_url;

    // Specifies how the new site is related to the current BrowsingInstance.
    // This is PREEXISTING iff |existing_site_instance| is defined.
    SiteInstanceRelation relation;
  };

  // Create a RenderFrameProxyHost owned by this object.
  RenderFrameProxyHost* CreateRenderFrameProxyHost(
      SiteInstance* site_instance,
      scoped_refptr<RenderViewHostImpl> rvh);

  // Delete a RenderFrameProxyHost owned by this object.
  void DeleteRenderFrameProxyHost(SiteInstance* site_instance);

  // Returns true if for the navigation from |current_effective_url| to
  // |destination_url|, a new SiteInstance and BrowsingInstance should be
  // created (even if we are in a process model that doesn't usually swap).
  // This forces a process swap and severs script connections with existing
  // tabs.  Cases where this can happen include transitions between WebUI and
  // regular web pages.
  //
  // |source_instance| is the SiteInstance of the frame that initiated the
  // navigation. |current_instance| is the SiteInstance of the frame that is
  // currently navigating. |destination_instance| is a predetermined
  // SiteInstance that will be used for |destination_url| if not
  // null - we will swap BrowsingInstances if it's in a different
  // BrowsingInstance than the current one.
  //
  // If there is no current NavigationEntry, then |current_is_view_source_mode|
  // should be the same as |dest_is_view_source_mode|.
  //
  // We use the effective URL here, since that's what is used in the
  // SiteInstance's site and when we later call IsSameSite.  If there is no
  // current NavigationEntry, check the current SiteInstance's site, which might
  // already be committed to a Web UI URL (such as the NTP). Note that we don't
  // pass the effective URL for destination URL here and instead calculate the
  // destination's effective URL within the function because some methods called
  // in the function like IsCurrentlySameSite expects a non-effective URL.
  ShouldSwapBrowsingInstance ShouldSwapBrowsingInstancesForNavigation(
      const GURL& current_effective_url,
      bool current_is_view_source_mode,
      SiteInstanceImpl* source_instance,
      SiteInstanceImpl* current_instance,
      SiteInstance* destination_instance,
      const GURL& destination_url,
      bool destination_is_view_source_mode,
      ui::PageTransition transition,
      bool is_failure,
      bool is_reload,
      bool cross_origin_opener_policy_mismatch,
      bool was_server_redirect);

  // Returns the SiteInstance to use for the navigation.
  scoped_refptr<SiteInstance> GetSiteInstanceForNavigation(
      const GURL& dest_url,
      SiteInstanceImpl* source_instance,
      SiteInstanceImpl* dest_instance,
      SiteInstanceImpl* candidate_instance,
      ui::PageTransition transition,
      bool is_failure,
      bool is_reload,
      bool dest_is_restore,
      bool dest_is_view_source_mode,
      bool was_server_redirect,
      bool cross_origin_opener_policy_mismatch);

  // Returns a descriptor of the appropriate SiteInstance object for the given
  // |dest_url|, possibly reusing the current, source or destination
  // SiteInstance. The actual SiteInstance can then be obtained calling
  // ConvertToSiteInstance with the descriptor.
  //
  // |source_instance| is the SiteInstance of the frame that initiated the
  // navigation. |current_instance| is the SiteInstance of the frame that is
  // currently navigating. |dest_instance| is a predetermined SiteInstance that
  // will be used if not null.
  // For example, if you have a parent frame A, which has a child frame B, and
  // A is trying to change the src attribute of B, this will cause a navigation
  // where the source SiteInstance is A and B is the current SiteInstance.
  //
  // This is a helper function for GetSiteInstanceForNavigation.
  SiteInstanceDescriptor DetermineSiteInstanceForURL(
      const GURL& dest_url,
      SiteInstance* source_instance,
      SiteInstance* current_instance,
      SiteInstance* dest_instance,
      ui::PageTransition transition,
      bool is_failure,
      bool dest_is_restore,
      bool dest_is_view_source_mode,
      bool force_browsing_instance_swap,
      bool was_server_redirect);

  // Returns true if a navigation to |dest_url| that uses the specified
  // PageTransition in the current frame is allowed to swap BrowsingInstances.
  // DetermineSiteInstanceForURL() uses this helper to determine when it is
  // allowed to swap BrowsingInstances to avoid unneeded process sharing.  See
  // https://crbug.com/803367.
  //
  // Note that this is different from
  // ShouldSwapBrowsingInstancesForNavigation(), which identifies cases in
  // which a BrowsingInstance swap is *required* (e.g., for security). This
  // function only identifies cases where a BrowsingInstance swap *may* be
  // performed to optimize process placement.  In particular, this is true for
  // certain browser-initiated transitions for main frame navigations.
  //
  // Returning true here doesn't imply that DetermineSiteInstanceForURL() will
  // swap BrowsingInstances.  For example, this swap will not be done for
  // same-site navigations, for history navigations, or when starting from an
  // uninitialized SiteInstance.
  bool IsBrowsingInstanceSwapAllowedForPageTransition(
      ui::PageTransition transition,
      const GURL& dest_url);

  // Returns true if we can use |source_instance| for |dest_url|.
  bool CanUseSourceSiteInstance(const GURL& dest_url,
                                SiteInstance* source_instance,
                                bool was_server_redirect,
                                bool is_failure);

  // Converts a SiteInstanceDescriptor to the actual SiteInstance it describes.
  // If a |candidate_instance| is provided (is not nullptr) and it matches the
  // description, it is returned as is.
  scoped_refptr<SiteInstance> ConvertToSiteInstance(
      const SiteInstanceDescriptor& descriptor,
      SiteInstanceImpl* candidate_instance);

  // Returns true if |candidate| is currently on the same web site as dest_url.
  bool IsCurrentlySameSite(RenderFrameHostImpl* candidate,
                           const GURL& dest_url);

  // Ensure that we have created all needed proxies for a new RFH with
  // SiteInstance |new_instance|: (1) create swapped-out RVHs and proxies for
  // the new RFH's opener chain if we are staying in the same BrowsingInstance;
  // (2) Create proxies for the new RFH's SiteInstance in its own frame tree.
  void CreateProxiesForNewRenderFrameHost(SiteInstance* old_instance,
                                          SiteInstance* new_instance);

  // Traverse the opener chain and populate |opener_frame_trees| with
  // all FrameTrees accessible by following frame openers of nodes in the
  // given node's FrameTree. |opener_frame_trees| is ordered so that openers
  // of smaller-indexed entries point to larger-indexed entries (i.e., this
  // node's FrameTree is at index 0, its opener's FrameTree is at index 1,
  // etc). If the traversal encounters a node with an opener pointing to a
  // FrameTree that has already been traversed (such as when there's a cycle),
  // the node is added to |nodes_with_back_links|.
  void CollectOpenerFrameTrees(
      std::vector<FrameTree*>* opener_frame_trees,
      std::unordered_set<FrameTreeNode*>* nodes_with_back_links);

  // Create RenderFrameProxies and inactive RenderViewHosts in the given
  // SiteInstance for the current node's FrameTree.  Used as a helper function
  // in CreateOpenerProxies for creating proxies in each FrameTree on the
  // opener chain.  Don't create proxies for the subtree rooted at
  // |skip_this_node|.
  void CreateOpenerProxiesForFrameTree(SiteInstance* instance,
                                       FrameTreeNode* skip_this_node);

  // The different types of RenderFrameHost creation that can occur.
  // See CreateRenderFrameHost for how these influence creation.
  enum class CreateFrameCase {
    // Adding a child to an existing frame in the tree.
    kInitChild,
    // Creating the first frame in a frame tree.
    kInitRoot,
    // Preparing to navigate to another frame.
    kCreateSpeculative,
  };

  // Creates a RenderFrameHost. This uses an existing a RenderViewHost in the
  // same SiteInstance if it exists or creates a new one (a new one will only be
  // created if this is a root or child local root).
  // TODO(https://crbug.com/1060082): Eliminate or rename
  // renderer_initiated_creation.
  std::unique_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
      CreateFrameCase create_frame_case,
      SiteInstance* site_instance,
      int32_t frame_routing_id,
      const base::UnguessableToken& frame_token,
      bool renderer_initiated_creation);

  // Create and initialize a speculative RenderFrameHost for an ongoing
  // navigation. It might be destroyed and re-created later if the navigation
  // is redirected to a different SiteInstance.
  bool CreateSpeculativeRenderFrameHost(SiteInstance* old_instance,
                                        SiteInstance* new_instance);

  // Initialization for RenderFrameHost uses the same sequence as InitRenderView
  // above.
  bool InitRenderFrame(RenderFrameHostImpl* render_frame_host);

  // Helper to reinitialize the RenderFrame, RenderView, and the opener chain
  // for the provided |render_frame_host|.  Used when the |render_frame_host|
  // needs to be reused for a new navigation, but it is not live.
  bool ReinitializeRenderFrame(RenderFrameHostImpl* render_frame_host);

  // Sets the |pending_rfh| to be the active one. Called when the pending
  // RenderFrameHost commits.
  //
  // This function is also called when restoring an entry from BackForwardCache.
  // In that case, |pending_rfh| is the RenderFrameHost to be restored, and
  // |pending_bfcache_entry| provides additional state to be restored, such as
  // proxies.
  // |clear_proxies_on_commit| Indicates if the proxies and opener must be
  // removed during the commit. This can happen following some BrowsingInstance
  // swaps, such as those for COOP.
  void CommitPending(
      std::unique_ptr<RenderFrameHostImpl> pending_rfh,
      std::unique_ptr<BackForwardCacheImpl::Entry> pending_bfcache_entry,
      bool clear_proxies_on_commit);

  // Helper to call CommitPending() in all necessary cases.
  void CommitPendingIfNecessary(RenderFrameHostImpl* render_frame_host,
                                bool was_caused_by_user_gesture,
                                bool is_same_document_navigation,
                                bool clear_proxies_on_commit);

  // Commits given frame policy when the renderer's frame navigates.
  void CommitFramePolicy(const blink::FramePolicy& frame_policy);

  // Runs the unload handler in the old RenderFrameHost, after the new
  // RenderFrameHost has committed.  |old_render_frame_host| will either be
  // deleted or put on the pending delete list during this call.
  void UnloadOldFrame(
      std::unique_ptr<RenderFrameHostImpl> old_render_frame_host);

  // Discards a RenderFrameHost that was never made active (for active ones
  // UnloadOldFrame is used instead).
  void DiscardUnusedFrame(
      std::unique_ptr<RenderFrameHostImpl> render_frame_host);

  // Helper method to set the active RenderFrameHost. Returns the old
  // RenderFrameHost and updates counts.
  std::unique_ptr<RenderFrameHostImpl> SetRenderFrameHost(
      std::unique_ptr<RenderFrameHostImpl> render_frame_host);

  // After a renderer process crash we'd have marked the host as invisible, so
  // we need to set the visibility of the new View to the correct value here
  // after reload.
  void EnsureRenderFrameHostVisibilityConsistent();

  // Similarly to visibility, we need to ensure RenderWidgetHost and
  // RenderWidget know about page focus.
  void EnsureRenderFrameHostPageFocusConsistent();

  // When current RenderFrameHost is not in its parent SiteInstance, this method
  // will destroy the frame and replace it with a new RenderFrameHost in the
  // parent frame's SiteInstance. Either way, this will eventually invoke
  // |attach_inner_delegate_callback_| with a pointer to |render_frame_host_|
  // which is then safe for use with WebContents::AttachToOuterWebContentsFrame.
  void CreateNewFrameForInnerDelegateAttachIfNecessary();

  // Called when the result of preparing the FrameTreeNode for attaching an
  // inner delegate is known. When successful, |render_frame_host_| can be used
  // for attaching the inner Delegate.
  void NotifyPrepareForInnerDelegateAttachComplete(bool success);

  // For use in creating RenderFrameHosts.
  FrameTreeNode* frame_tree_node_;

  // Our delegate, not owned by us. Guaranteed non-NULL.
  Delegate* delegate_;

  // Our RenderFrameHost which is responsible for all communication with a child
  // RenderFrame instance.
  // For now, RenderFrameHost keeps a RenderViewHost in its SiteInstance alive.
  // Eventually, RenderViewHost will be replaced with a page context.
  std::unique_ptr<RenderFrameHostImpl> render_frame_host_;

  // Proxy hosts, indexed by site instance ID.
  RenderFrameProxyHostMap proxy_hosts_;

  // A list of RenderFrameHosts waiting to shut down after swapping out.
  using RFHPendingDeleteList = std::list<std::unique_ptr<RenderFrameHostImpl>>;
  RFHPendingDeleteList pending_delete_hosts_;

  // Stores a speculative RenderFrameHost which is created early in a navigation
  // so a renderer process can be started in parallel, if needed.
  // This is purely a performance optimization and is not required for correct
  // behavior. The speculative RenderFrameHost might be discarded later on if
  // the final URL's SiteInstance isn't compatible with the one used to create
  // it.
  std::unique_ptr<RenderFrameHostImpl> speculative_render_frame_host_;

  // After being set in RestoreFromBackForwardCache(), the bfcache entry is
  // immediately consumed in CommitPending().
  std::unique_ptr<BackForwardCacheImpl::Entry> bfcache_entry_to_restore_;

  // This callback is used when attaching an inner Delegate to |delegate_|
  // through |frame_tree_node_|.
  RenderFrameHost::PrepareForInnerWebContentsAttachCallback
      attach_inner_delegate_callback_;
  AttachToInnerDelegateState attach_to_inner_delegate_state_ =
      AttachToInnerDelegateState::NONE;

  base::WeakPtrFactory<RenderFrameHostManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_MANAGER_H_
