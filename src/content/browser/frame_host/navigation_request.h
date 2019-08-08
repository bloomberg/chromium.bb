// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_throttle_runner.h"
#include "content/browser/initiator_csp_context.h"
#include "content/browser/loader/navigation_url_loader_delegate.h"
#include "content/common/content_export.h"
#include "content/common/frame_message_enums.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/previews_state.h"

namespace network {
class ResourceRequestBody;
struct URLLoaderCompletionStatus;
}

namespace content {

class FrameNavigationEntry;
class FrameTreeNode;
class NavigationHandleImpl;
class NavigationURLLoader;
class NavigationData;
class NavigationUIData;
class NavigatorDelegate;
class SiteInstanceImpl;
struct SubresourceLoaderParams;

// A UI thread object that owns a navigation request until it commits. It
// ensures the UI thread can start a navigation request in the
// ResourceDispatcherHost (that lives on the IO thread).
// TODO(clamy): Describe the interactions between the UI and IO thread during
// the navigation following its refactoring.
class CONTENT_EXPORT NavigationRequest : public NavigationURLLoaderDelegate,
                                         NavigationThrottleRunner::Delegate,
                                         private RenderProcessHostObserver {
 public:
  // Keeps track of the various stages of a NavigationRequest.
  enum NavigationState {
    // Initial state.
    NOT_STARTED = 0,

    // Waiting for a BeginNavigation IPC from the renderer in a
    // browser-initiated navigation. If there is no live renderer when the
    // request is created, this stage is skipped.
    WAITING_FOR_RENDERER_RESPONSE,

    // The request was sent to the IO thread.
    STARTED,

    // The response started on the IO thread and is ready to be committed. This
    // is one of the two final states for the request.
    RESPONSE_STARTED,

    // The request failed on the IO thread and an error page should be
    // displayed. This is one of the two final states for the request.
    FAILED,
  };

  // Used to track the navigation state of NavigationHandleImpl.
  // Note: the states named PROCESSING_* indicate that NavigationThrottles are
  // currently processing the corresponding event. When they are done, the
  // state will move to the next in the list.
  // TODO(zetamoo): Merge NavigationHandleState with NavigationState, and remove
  //                the duplicates. Remove the PROCESSING_* states once the
  //                NavigationThrottleRunner is owned by the NavigationRequest.
  enum NavigationHandleState {
    NOT_CREATED = 0,
    INITIAL,
    PROCESSING_WILL_START_REQUEST,
    WILL_START_REQUEST,
    PROCESSING_WILL_REDIRECT_REQUEST,
    WILL_REDIRECT_REQUEST,
    PROCESSING_WILL_FAIL_REQUEST,
    WILL_FAIL_REQUEST,
    CANCELING,
    PROCESSING_WILL_PROCESS_RESPONSE,
    WILL_PROCESS_RESPONSE,
    READY_TO_COMMIT,
    DID_COMMIT,
    DID_COMMIT_ERROR_PAGE,
  };

  // The SiteInstance currently associated with the navigation. Note that the
  // final value will only be known when the response is received, or the
  // navigation fails, as server redirects can modify the SiteInstance to use
  // for the navigation.
  enum class AssociatedSiteInstanceType {
    NONE = 0,
    CURRENT,
    SPECULATIVE,
  };

  // Creates a request for a browser-intiated navigation.
  // Note: this is sometimes called for renderer-initiated navigations going
  // through the OpenURL path. |browser_initiated| should be false in that case.
  // TODO(clamy): Rename this function and consider merging it with
  // CreateRendererInitiated.
  static std::unique_ptr<NavigationRequest> CreateBrowserInitiated(
      FrameTreeNode* frame_tree_node,
      const CommonNavigationParams& common_params,
      const CommitNavigationParams& commit_params,
      bool browser_initiated,
      const std::string& extra_headers,
      const FrameNavigationEntry& frame_entry,
      NavigationEntryImpl* entry,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      std::unique_ptr<NavigationUIData> navigation_ui_data);

  // Creates a request for a renderer-intiated navigation.
  // Note: |body| is sent to the IO thread when calling BeginNavigation, and
  // should no longer be manipulated afterwards on the UI thread.
  // TODO(clamy): see if ResourceRequestBody could be un-refcounted to avoid
  // threading subtleties.
  static std::unique_ptr<NavigationRequest> CreateRendererInitiated(
      FrameTreeNode* frame_tree_node,
      NavigationEntryImpl* entry,
      const CommonNavigationParams& common_params,
      mojom::BeginNavigationParamsPtr begin_params,
      int current_history_list_offset,
      int current_history_list_length,
      bool override_user_agent,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      mojom::NavigationClientAssociatedPtrInfo navigation_client,
      blink::mojom::NavigationInitiatorPtr navigation_initiator);

  // Creates a request at commit time. This should only be used for
  // renderer-initiated same-document navigations, and navigations whose
  // original NavigationRequest has been destroyed by race-conditions.
  // TODO(clamy): Eventually, this should only be called for same-document
  // renderer-initiated navigations.
  static std::unique_ptr<NavigationRequest> CreateForCommit(
      FrameTreeNode* frame_tree_node,
      RenderFrameHostImpl* render_frame_host,
      NavigationEntryImpl* entry,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
      bool is_renderer_initiated,
      bool is_same_document);

  ~NavigationRequest() override;

  // Called on the UI thread by the Navigator to start the navigation.
  // The NavigationRequest can be deleted while BeginNavigation() is called.
  void BeginNavigation();

  const CommonNavigationParams& common_params() const { return common_params_; }

  const mojom::BeginNavigationParams* begin_params() const {
    return begin_params_.get();
  }

  const CommitNavigationParams& commit_params() const { return commit_params_; }

  // Updates the navigation start time.
  void set_navigation_start_time(const base::TimeTicks& time) {
    common_params_.navigation_start = time;
  }

  NavigationURLLoader* loader_for_testing() const { return loader_.get(); }

  NavigationState state() const { return state_; }

  FrameTreeNode* frame_tree_node() const { return frame_tree_node_; }

  SiteInstanceImpl* source_site_instance() const {
    return source_site_instance_.get();
  }

  SiteInstanceImpl* dest_site_instance() const {
    return dest_site_instance_.get();
  }

  RestoreType restore_type() const { return restore_type_; }

  bool is_view_source() const { return is_view_source_; }

  int bindings() const { return bindings_; }

  SiteInstanceImpl* starting_site_instance() const {
    return starting_site_instance_.get();
  }

  bool browser_initiated() const { return browser_initiated_ ; }

  bool from_begin_navigation() const { return from_begin_navigation_; }

  AssociatedSiteInstanceType associated_site_instance_type() const {
    return associated_site_instance_type_;
  }
  void set_associated_site_instance_type(AssociatedSiteInstanceType type) {
    associated_site_instance_type_ = type;
  }

  void set_was_discarded() { commit_params_.was_discarded = true; }

  NavigationHandleImpl* navigation_handle() const {
    return navigation_handle_.get();
  }

  int net_error() { return net_error_; }

  const std::string& GetMimeType() {
    return response_ ? response_->head.mime_type : base::EmptyString();
  }

  // The RenderFrameHost that will commit the navigation or an error page.
  // This is computed when the response is received, or when the navigation
  // fails and error page should be displayed.
  RenderFrameHostImpl* render_frame_host() const {
    // Only allow the RenderFrameHost to be retrieved once it has been set for
    // this navigation. This will happens either at WillProcessResponse time for
    // regular navigations or at WillFailRequest time for error pages.
    CHECK_GE(handle_state_, PROCESSING_WILL_FAIL_REQUEST)
        << "This accessor should only be called after a RenderFrameHost has "
           "been picked for this navigation.";
    static_assert(
        WILL_FAIL_REQUEST < WILL_PROCESS_RESPONSE,
        "WillFailRequest state should come before WillProcessResponse");
    return render_frame_host_;
  }

  const network::ResourceResponse* response() { return response_.get(); }
  const GlobalRequestID& request_id() const { return request_id_; }
  bool is_download() const { return is_download_; }
  const base::Optional<net::SSLInfo>& ssl_info() { return ssl_info_; }

  void SetWaitingForRendererResponse();

  // Creates a NavigationHandle. This should be called after any previous
  // NavigationRequest for the FrameTreeNode has been destroyed. |is_for_commit|
  // should only be true when creating a NavigationHandle at commit time (this
  // happens for renderer-initiated same-document navigations).
  void CreateNavigationHandle(bool is_for_commit);

  void set_on_start_checks_complete_closure_for_testing(
      const base::Closure& closure) {
    on_start_checks_complete_closure_ = closure;
  }

  // Sets ID of the RenderProcessHost we expect the navigation to commit in.
  // This is used to inform the RenderProcessHost to expect a navigation to the
  // url we're navigating to.
  void SetExpectedProcess(RenderProcessHost* expected_process);

  // Updates the destination site URL for this navigation. This is called on
  // redirects. |post_redirect_process| is the renderer process that should
  // handle the navigation following the redirect if it can be handled by an
  // existing RenderProcessHost. Otherwise, it should be null.
  void UpdateSiteURL(RenderProcessHost* post_redirect_process);

  int nav_entry_id() const { return nav_entry_id_; }

  // For automation driver-initiated navigations over the devtools protocol,
  // |devtools_navigation_token_| is used to tag the navigation. This navigation
  // token is then sent into the renderer and lands on the DocumentLoader. That
  // way subsequent Blink-level frame lifecycle events can be associated with
  // the concrete navigation.
  // - The value should not be sent back to the browser.
  // - The value on DocumentLoader may be generated in the renderer in some
  // cases, and thus shouldn't be trusted.
  // TODO(crbug.com/783506): Replace devtools navigation token with the generic
  // navigation token that can be passed from renderer to the browser.
  const base::UnguessableToken& devtools_navigation_token() const {
    return devtools_navigation_token_;
  }

  // Called on same-document navigation requests that need to be restarted as
  // cross-document navigations. This happens when a same-document commit fails
  // due to another navigation committing in the meantime.
  void ResetForCrossDocumentRestart();

  void RegisterSubresourceOverride(
      mojom::TransferrableURLLoaderPtr transferrable_loader);

  // Lazily initializes and returns the mojo::NavigationClient interface used
  // for commit. Only used with PerNavigationMojoInterface enabled.
  mojom::NavigationClient* GetCommitNavigationClient();

  void SetOriginPolicy(const std::string& policy);

  void set_transition(ui::PageTransition transition) {
    common_params_.transition = transition;
  }

  void set_has_user_gesture(bool has_user_gesture) {
    common_params_.has_user_gesture = has_user_gesture;
  }

  // Ignores any interface disconnect that might happen to the
  // navigation_client used to commit.
  void IgnoreCommitInterfaceDisconnection();

  bool IsSameDocument() const;

  int navigation_entry_offset() { return navigation_entry_offset_; }

  // TODO(zetamoo): Remove once |handle_state_| is modified exclusively from
  //                NavigationRequest.
  void set_handle_state(const NavigationHandleState state) {
    handle_state_ = state;
  }

  NavigationHandleState handle_state() { return handle_state_; }

  NavigationUIData* navigation_ui_data() const {
    return navigation_ui_data_.get();
  }

  // Resume and CancelDeferredNavigation must only be called by the
  // NavigationThrottle that is currently deferring the navigation.
  // |resuming_throttle| and |cancelling_throttle| are the throttles calling
  // these methods.
  void Resume(NavigationThrottle* resuming_throttle);
  void CancelDeferredNavigation(NavigationThrottle* cancelling_throttle,
                                NavigationThrottle::ThrottleCheckResult result);

  // Simulates the navigation resuming. Most callers should just let the
  // deferring NavigationThrottle do the resuming.
  void CallResumeForTesting();

  void RegisterThrottleForTesting(
      std::unique_ptr<NavigationThrottle> navigation_throttle);

  bool IsDeferredForTesting();

  typedef base::OnceCallback<void(NavigationThrottle::ThrottleCheckResult)>
      ThrottleChecksFinishedCallback;

  NavigationThrottle* GetDeferringThrottleForTesting() const {
    return throttle_runner_->GetDeferringThrottle();
  }

 private:
  // TODO(clamy): Transform NavigationHandleImplTest into NavigationRequestTest
  // once NavigationHandleImpl has become a wrapper around NavigationRequest.
  friend class NavigationHandleImplTest;

  NavigationRequest(FrameTreeNode* frame_tree_node,
                    const CommonNavigationParams& common_params,
                    mojom::BeginNavigationParamsPtr begin_params,
                    const CommitNavigationParams& commit_params,
                    bool browser_initiated,
                    bool from_begin_navigation,
                    bool is_for_commit,
                    const FrameNavigationEntry* frame_navigation_entry,
                    NavigationEntryImpl* navitation_entry,
                    std::unique_ptr<NavigationUIData> navigation_ui_data,
                    mojom::NavigationClientAssociatedPtrInfo navigation_client,
                    blink::mojom::NavigationInitiatorPtr navigation_initiator);

  // NavigationURLLoaderDelegate implementation.
  void OnRequestRedirected(
      const net::RedirectInfo& redirect_info,
      const scoped_refptr<network::ResourceResponse>& response) override;
  void OnResponseStarted(
      const scoped_refptr<network::ResourceResponse>& response,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      std::unique_ptr<NavigationData> navigation_data,
      const GlobalRequestID& request_id,
      bool is_download,
      NavigationDownloadPolicy download_policy,
      bool is_stream,
      base::Optional<SubresourceLoaderParams> subresource_loader_params)
      override;
  void OnRequestFailed(
      const network::URLLoaderCompletionStatus& status) override;
  void OnRequestStarted(base::TimeTicks timestamp) override;

  // To be called whenever a navigation request fails. If |skip_throttles| is
  // true, the registered NavigationThrottle(s) won't get a chance to intercept
  // NavigationThrottle::WillFailRequest. It should be used when a request
  // failed due to a throttle result itself. |error_page_content| is only used
  // when |skip_throttles| is true. If |collapse_frame| is true, the associated
  // frame tree node is collapsed.
  void OnRequestFailedInternal(
      const network::URLLoaderCompletionStatus& status,
      bool skip_throttles,
      const base::Optional<std::string>& error_page_content,
      bool collapse_frame);

  // Helper to determine whether an error page for the provided error code
  // should stay in the current process.
  bool ShouldKeepErrorPageInCurrentProcess(int net_error);

  // Called when the NavigationThrottles have been checked by the
  // NavigationHandle.
  void OnStartChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnRedirectChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnFailureChecksComplete(NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseChecksComplete(
      NavigationThrottle::ThrottleCheckResult result);

  // Called either by OnFailureChecksComplete() or OnRequestFailed() directly.
  // |error_page_content| contains the content of the error page (i.e. flattened
  // HTML, JS, CSS).
  void CommitErrorPage(const base::Optional<std::string>& error_page_content);

  // Have a RenderFrameHost commit the navigation. The NavigationRequest will
  // be destroyed after this call.
  void CommitNavigation();

  // Checks if the specified CSP context's relevant CSP directive
  // allows the navigation. This is called to perform the frame-src
  // and navigate-to checks.
  bool IsAllowedByCSPDirective(CSPContext* context,
                               CSPDirective::Name directive,
                               bool has_followed_redirect,
                               bool url_upgraded_after_redirect,
                               bool is_response_check,
                               CSPContext::CheckCSPDisposition disposition);

  // Checks if CSP allows the navigation. This will check the frame-src and
  // navigate-to directives.
  // Returns net::OK if the checks pass, and net::ERR_ABORTED or
  // net::ERR_BLOCKED_BY_CLIENT depending on which checks fail.
  net::Error CheckCSPDirectives(RenderFrameHostImpl* parent,
                                bool has_followed_redirect,
                                bool url_upgraded_after_redirect,
                                bool is_response_check,
                                CSPContext::CheckCSPDisposition disposition);

  // Check whether a request should be allowed to continue or should be blocked
  // because it violates a CSP. This method can have two side effects:
  // - If a CSP is configured to send reports and the request violates the CSP,
  //   a report will be sent.
  // - The navigation request may be upgraded from HTTP to HTTPS if a CSP is
  //   configured to upgrade insecure requests.
  net::Error CheckContentSecurityPolicy(bool has_followed_redirect,
                                        bool url_upgraded_after_redirect,
                                        bool is_response_check);

  // This enum describes the result of the credentialed subresource check for
  // the request.
  enum class CredentialedSubresourceCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };

  // Chrome blocks subresource requests whose URLs contain embedded credentials
  // (e.g. `https://user:pass@example.com/page.html`). Check whether the
  // request should be allowed to continue or should be blocked.
  CredentialedSubresourceCheckResult CheckCredentialedSubresource() const;

  // This enum describes the result of the legacy protocol check for
  // the request.
  enum class LegacyProtocolInSubresourceCheckResult {
    ALLOW_REQUEST,
    BLOCK_REQUEST,
  };

  // Block subresources requests that target "legacy" protocol (like "ftp") when
  // the main document is not served from a "legacy" protocol.
  LegacyProtocolInSubresourceCheckResult CheckLegacyProtocolInSubresource()
      const;

  // Called before a commit. Updates the history index and length held in
  // CommitNavigationParams. This is used to update this shared state with the
  // renderer process.
  void UpdateCommitNavigationParamsHistory();

  // Called when an ongoing renderer-initiated navigation is aborted.
  // Only used with PerNavigationMojoInterface enabled.
  void OnRendererAbortedNavigation();

  // Binds the given error_handler to be called when an interface disconnection
  // happens on the renderer side.
  // Only used with PerNavigationMojoInterface enabled.
  void HandleInterfaceDisconnection(mojom::NavigationClientAssociatedPtr*,
                                    base::OnceClosure error_handler);

  // When called, this NavigationRequest will no longer interpret the interface
  // disconnection on the renderer side as an AbortNavigation.
  // TODO(ahemery): remove this function when NavigationRequest properly handles
  // interface disconnection in all cases.
  // Only used with PerNavigationMojoInterface enabled.
  void IgnoreInterfaceDisconnection();

  // Inform the RenderProcessHost to no longer expect a navigation.
  void ResetExpectedProcess();

  // https://crbug.com/936962 happens when a |render_frame_host_| has been
  // selected and the |loader_| calls OnRequestFailed(). This shouldn't be
  // possible, because |render_frame_host_| and |loader_| can't be non-null at
  // the same time.
  // TODO(https://crbug.com/936962): Remove this when the bug is fixed.
  void VerifyLoaderAndRenderFrameHostExpectations();

  // Compute the history offset of the new document compared to the current one.
  // See navigation_history_offset_ for more details.
  int EstimateHistoryOffset();

  // Record download related UseCounters when navigation is a download before
  // filtered by download_policy.
  void RecordDownloadUseCountersPrePolicyCheck(
      NavigationDownloadPolicy download_policy);

  // Record download related UseCounters when navigation is a download after
  // filtered by download_policy.
  void RecordDownloadUseCountersPostPolicyCheck();

  // NavigationThrottleRunner::Delegate:
  void OnNavigationEventProcessed(
      NavigationThrottleRunner::Event event,
      NavigationThrottle::ThrottleCheckResult result) override;

  void OnWillStartRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillRedirectRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillFailRequestProcessed(
      NavigationThrottle::ThrottleCheckResult result);
  void OnWillProcessResponseProcessed(
      NavigationThrottle::ThrottleCheckResult result);

  NavigatorDelegate* GetDelegate() const;

  void CancelDeferredNavigationInternal(
      NavigationThrottle::ThrottleCheckResult result);

  // TODO(zetamoo): Remove the Will* methods and fold them into their callers.

  // Called when the URLRequest will start in the network stack. |callback| will
  // be called when all throttle checks have completed. This will allow the
  // caller to cancel the navigation or let it proceed.
  void WillStartRequest(ThrottleChecksFinishedCallback callback);

  // Called when the URLRequest will be redirected in the network stack.
  // |callback| will be called when all throttles check have completed. This
  // will allow the caller to cancel the navigation or let it proceed.
  // This will also inform the delegate that the request was redirected.
  //
  // |post_redirect_process| is the renderer process we expect to use to commit
  // the navigation now that it has been redirected. It can be null if there is
  // no live process that can be used. In that case, a suitable renderer process
  // will be created at commit time.
  void WillRedirectRequest(const GURL& new_referrer_url,
                           RenderProcessHost* post_redirect_process,
                           ThrottleChecksFinishedCallback callback);

  // Called when the URLRequest will fail. |callback| will be called when all
  // throttles check have completed. This will allow the caller to explicitly
  // cancel the navigation (with a custom error code and/or custom error page
  // HTML) or let the failure proceed as normal.
  void WillFailRequest(ThrottleChecksFinishedCallback callback);

  // Called when the URLRequest has delivered response headers and metadata.
  // |callback| will be called when all throttle checks have completed,
  // allowing the caller to cancel the navigation or let it proceed.
  // NavigationHandle will not call |callback| with a result of DEFER.
  // If the result is PROCEED, then 'ReadyToCommitNavigation' will be called
  // just before calling |callback|.
  void WillProcessResponse(ThrottleChecksFinishedCallback callback);

  // Checks for attempts to navigate to a page that is already referenced more
  // than once in the frame's ancestors.  This is a helper function used by
  // WillStartRequest and WillRedirectRequest to prevent the navigation.
  bool IsSelfReferentialURL();

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  FrameTreeNode* frame_tree_node_;

  RenderFrameHostImpl* render_frame_host_ = nullptr;

  // Initialized on creation of the NavigationRequest. Sent to the renderer when
  // the navigation is ready to commit.
  // Note: When the navigation is ready to commit, the url in |common_params|
  // will be set to the final navigation url, obtained after following all
  // redirects.
  // Note: |common_params_| and |begin_params_| are not const as they can be
  // modified during redirects.
  // Note: |commit_params_| is not const because was_discarded will
  // be set in CreatedNavigationRequest.
  CommonNavigationParams common_params_;
  mojom::BeginNavigationParamsPtr begin_params_;
  CommitNavigationParams commit_params_;
  const bool browser_initiated_;

  // Stores the NavigationUIData for this navigation until the NavigationHandle
  // is created. This can be null if the embedded did not provide a
  // NavigationUIData at the beginning of the navigation.
  std::unique_ptr<NavigationUIData> navigation_ui_data_;

  // URLLoaderFactory to facilitate loading blob URLs.
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory_;

  NavigationState state_;

  // It's important to ensure |navigation_handle_| outlives |loader_|, since the
  // loader holds raw pointers to objects owned by the navigation handle
  // (namely, the AppCache and service worker handles). The destruction order
  // matters because it occurs over separate tasks on the IO thread. So, declare
  // the handle before the loader.
  std::unique_ptr<NavigationHandleImpl> navigation_handle_;
  std::unique_ptr<NavigationURLLoader> loader_;

  // These next items are used in browser-initiated navigations to store
  // information from the NavigationEntryImpl that is required after request
  // creation time.
  scoped_refptr<SiteInstanceImpl> source_site_instance_;
  scoped_refptr<SiteInstanceImpl> dest_site_instance_;
  RestoreType restore_type_;
  bool is_view_source_;
  int bindings_;
  int nav_entry_id_ = 0;

  scoped_refptr<SiteInstanceImpl> starting_site_instance_;

  // Whether the navigation should be sent to a renderer a process. This is
  // true, except for 204/205 responses and downloads.
  bool response_should_be_rendered_;

  // The type of SiteInstance associated with this navigation.
  AssociatedSiteInstanceType associated_site_instance_type_;

  // Stores the SiteInstance created on redirects to check if there is an
  // existing RenderProcessHost that can commit the navigation so that the
  // renderer process is not deleted while the navigation is ongoing. If the
  // SiteInstance was a brand new SiteInstance, it is not stored.
  scoped_refptr<SiteInstance> speculative_site_instance_;

  // Whether the NavigationRequest was created after receiving a BeginNavigation
  // IPC. When true, main frame navigations should not commit in a different
  // process (unless asked by the content/ embedder). When true, the renderer
  // process expects to be notified if the navigation is aborted.
  bool from_begin_navigation_;

  // Holds objects received from OnResponseStarted while the WillProcessResponse
  // checks are performed by the NavigationHandle. Once the checks have been
  // completed, these objects will be used to continue the navigation.
  scoped_refptr<network::ResourceResponse> response_;
  network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints_;
  base::Optional<net::SSLInfo> ssl_info_;
  bool is_download_ = false;
  bool is_stream_ = false;
  GlobalRequestID request_id_;

  // Holds information for the navigation while the WillFailRequest
  // checks are performed by the NavigationHandle.
  bool has_stale_copy_in_cache_;
  int net_error_;

  // Identifies in which RenderProcessHost this navigation is expected to
  // commit.
  int expected_render_process_host_id_;

  // The site URL of this navigation, as obtained from SiteInstance::GetSiteURL.
  GURL site_url_;

  std::unique_ptr<InitiatorCSPContext> initiator_csp_context_;

  base::Closure on_start_checks_complete_closure_;

  // Used in the network service world to pass the subressource loader params
  // to the renderer. Used by AppCache and ServiceWorker.
  base::Optional<SubresourceLoaderParams> subresource_loader_params_;

  // See comment on accessor.
  const base::UnguessableToken devtools_navigation_token_;

  base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
      subresource_overrides_;

  // The NavigationClient interface for that requested this navigation in the
  // case of a renderer initiated navigation. It is expected to be bound until
  // this navigation commits or is canceled.
  // Only valid when PerNavigationMojoInterface is enabled.
  mojom::NavigationClientAssociatedPtr request_navigation_client_;
  base::Optional<int32_t> associated_site_instance_id_;

  // The NavigationClient interface used to commit the navigation. For now, this
  // is only used for same-site renderer-initiated navigation.
  // TODO(clamy, ahemery): Extend to all types of navigation.
  // Only valid when PerNavigationMojoInterface is enabled.
  mojom::NavigationClientAssociatedPtr commit_navigation_client_;

  // If set, any redirects to HTTP for this navigation will be upgraded to
  // HTTPS. This is used only on subframe navigations, when
  // upgrade-insecure-requests is set as a CSP policy.
  bool upgrade_if_insecure_ = false;

  // The offset of the new document in the history.
  // See NavigationHandle::GetNavigationEntryOffset() for details.
  int navigation_entry_offset_ = 0;

  // TODO(zetamoo): Merge |handle_state_| with |state_|.
  NavigationHandleState handle_state_ = NOT_CREATED;

  // Owns the NavigationThrottles associated with this navigation, and is
  // responsible for notifying them about the various navigation events.
  std::unique_ptr<NavigationThrottleRunner> throttle_runner_;

  base::WeakPtrFactory<NavigationRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationRequest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_REQUEST_H_
