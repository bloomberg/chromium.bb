// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_handle_impl.h"

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace content {

namespace {

// Default timeout for the READY_TO_COMMIT -> COMMIT transition.  Chosen
// initially based on the Navigation.ReadyToCommitUntilCommit UMA, and then
// refined based on feedback based on CrashExitCodes.Renderer/RESULT_CODE_HUNG.
constexpr base::TimeDelta kDefaultCommitTimeout =
    base::TimeDelta::FromSeconds(30);

// Timeout for the READY_TO_COMMIT -> COMMIT transition.
// Overrideable via SetCommitTimeoutForTesting.
base::TimeDelta g_commit_timeout = kDefaultCommitTimeout;

// Corresponds to the "NavigationURLScheme" histogram enumeration type in
// src/tools/metrics/histograms/enums.xml.
//
// DO NOT REORDER OR CHANGE THE MEANING OF THESE VALUES.
enum class NavigationURLScheme {
  UNKNOWN = 0,
  ABOUT = 1,
  BLOB = 2,
  CONTENT = 3,
  CONTENT_ID = 4,
  DATA = 5,
  FILE = 6,
  FILE_SYSTEM = 7,
  FTP = 8,
  HTTP = 9,
  HTTPS = 10,
  kMaxValue = HTTPS
};

NavigationURLScheme GetScheme(const GURL& url) {
  static const base::NoDestructor<std::map<std::string, NavigationURLScheme>>
      kSchemeMap({
          {url::kAboutScheme, NavigationURLScheme::ABOUT},
          {url::kBlobScheme, NavigationURLScheme::BLOB},
          {url::kContentScheme, NavigationURLScheme::CONTENT},
          {url::kContentIDScheme, NavigationURLScheme::CONTENT_ID},
          {url::kDataScheme, NavigationURLScheme::DATA},
          {url::kFileScheme, NavigationURLScheme::FILE},
          {url::kFileSystemScheme, NavigationURLScheme::FILE_SYSTEM},
          {url::kFtpScheme, NavigationURLScheme::FTP},
          {url::kHttpScheme, NavigationURLScheme::HTTP},
          {url::kHttpsScheme, NavigationURLScheme::HTTPS},
      });
  auto it = kSchemeMap->find(url.scheme());
  if (it != kSchemeMap->end())
    return it->second;
  return NavigationURLScheme::UNKNOWN;
}

}  // namespace

NavigationHandleImpl::NavigationHandleImpl(
    NavigationRequest* navigation_request,
    int pending_nav_entry_id,
    net::HttpRequestHeaders request_headers)
    : navigation_request_(navigation_request),
      request_headers_(std::move(request_headers)),
      pending_nav_entry_id_(pending_nav_entry_id),
      reload_type_(ReloadType::NONE),
      restore_type_(RestoreType::NONE) {
  const GURL& url = navigation_request_->common_params().url;
  TRACE_EVENT_ASYNC_BEGIN2("navigation", "NavigationHandle", this,
                           "frame_tree_node",
                           frame_tree_node()->frame_tree_node_id(), "url",
                           url.possibly_invalid_spec());
  DCHECK(!navigation_request_->common_params().navigation_start.is_null());
  DCHECK(!IsRendererDebugURL(url));

  // Try to match this with a pending NavigationEntry if possible.  Note that
  // the NavigationController itself may be gone if this is a navigation inside
  // an interstitial and the interstitial is asynchronously deleting itself due
  // to its tab closing.
  NavigationControllerImpl* nav_controller =
      static_cast<NavigationControllerImpl*>(
          frame_tree_node()->navigator()->GetController());
  if (pending_nav_entry_id_ && nav_controller) {
    NavigationEntryImpl* nav_entry =
        nav_controller->GetEntryWithUniqueID(pending_nav_entry_id_);
    if (!nav_entry &&
        nav_controller->GetPendingEntry() &&
        nav_controller->GetPendingEntry()->GetUniqueID() ==
            pending_nav_entry_id_) {
      nav_entry = nav_controller->GetPendingEntry();
    }

    if (nav_entry) {
      reload_type_ = nav_entry->reload_type();
      restore_type_ = nav_entry->restore_type();
    }
  }

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP1(
        "navigation", "Navigation StartToCommit", this,
        navigation_request_->common_params().navigation_start, "Initial URL",
        url.spec());
  }

  if (IsSameDocument()) {
    TRACE_EVENT_ASYNC_STEP_INTO0("navigation", "NavigationHandle", this,
                                 "Same document");
  }
}

NavigationHandleImpl::~NavigationHandleImpl() {

  GetDelegate()->DidFinishNavigation(this);

  if (IsInMainFrame()) {
    TRACE_EVENT_ASYNC_END2("navigation", "Navigation StartToCommit", this,
                           "URL",
                           navigation_request_->common_params().url.spec(),
                           "Net Error Code", GetNetErrorCode());
  }
  TRACE_EVENT_ASYNC_END0("navigation", "NavigationHandle", this);
}

NavigatorDelegate* NavigationHandleImpl::GetDelegate() const {
  return frame_tree_node()->navigator()->GetDelegate();
}

int64_t NavigationHandleImpl::GetNavigationId() {
  return navigation_request_->navigation_handle_id();
}

const GURL& NavigationHandleImpl::GetURL() {
  return navigation_request_->common_params().url;
}

SiteInstanceImpl* NavigationHandleImpl::GetStartingSiteInstance() {
  return navigation_request_->starting_site_instance();
}

bool NavigationHandleImpl::IsInMainFrame() {
  return frame_tree_node()->IsMainFrame();
}

bool NavigationHandleImpl::IsParentMainFrame() {
  if (frame_tree_node()->parent())
    return frame_tree_node()->parent()->IsMainFrame();

  return false;
}

bool NavigationHandleImpl::IsRendererInitiated() {
  return !navigation_request_->browser_initiated();
}

bool NavigationHandleImpl::WasServerRedirect() {
  return navigation_request_->was_redirected();
}

const std::vector<GURL>& NavigationHandleImpl::GetRedirectChain() {
  return navigation_request_->redirect_chain();
}

int NavigationHandleImpl::GetFrameTreeNodeId() {
  return frame_tree_node()->frame_tree_node_id();
}

RenderFrameHostImpl* NavigationHandleImpl::GetParentFrame() {
  if (IsInMainFrame())
    return nullptr;

  return frame_tree_node()->parent()->current_frame_host();
}

base::TimeTicks NavigationHandleImpl::NavigationStart() {
  return navigation_request_->common_params().navigation_start;
}

base::TimeTicks NavigationHandleImpl::NavigationInputStart() {
  return navigation_request_->common_params().input_start;
}

bool NavigationHandleImpl::IsPost() {
  return navigation_request_->common_params().method == "POST";
}

const scoped_refptr<network::ResourceRequestBody>&
NavigationHandleImpl::GetResourceRequestBody() {
  return navigation_request_->common_params().post_data;
}

const Referrer& NavigationHandleImpl::GetReferrer() {
  return navigation_request_->sanitized_referrer();
}

bool NavigationHandleImpl::HasUserGesture() {
  return navigation_request_->common_params().has_user_gesture;
}

ui::PageTransition NavigationHandleImpl::GetPageTransition() {
  return navigation_request_->common_params().transition;
}

NavigationUIData* NavigationHandleImpl::GetNavigationUIData() {
  return navigation_request_->navigation_ui_data();
}

bool NavigationHandleImpl::IsExternalProtocol() {
  return !GetContentClient()->browser()->IsHandledURL(GetURL());
}

net::Error NavigationHandleImpl::GetNetErrorCode() {
  return navigation_request_->net_error();
}

RenderFrameHostImpl* NavigationHandleImpl::GetRenderFrameHost() {
  return navigation_request_->render_frame_host();
}

bool NavigationHandleImpl::IsSameDocument() {
  return navigation_request_->IsSameDocument();
}

const net::HttpRequestHeaders& NavigationHandleImpl::GetRequestHeaders() {
  return request_headers_;
}

void NavigationHandleImpl::RemoveRequestHeader(const std::string& header_name) {
  DCHECK(state() == NavigationRequest::PROCESSING_WILL_REDIRECT_REQUEST ||
         state() == NavigationRequest::WILL_REDIRECT_REQUEST);
  removed_request_headers_.push_back(header_name);
}

void NavigationHandleImpl::SetRequestHeader(const std::string& header_name,
                                            const std::string& header_value) {
  DCHECK(state() == NavigationRequest::INITIAL ||
         state() == NavigationRequest::PROCESSING_WILL_START_REQUEST ||
         state() == NavigationRequest::PROCESSING_WILL_REDIRECT_REQUEST ||
         state() == NavigationRequest::WILL_START_REQUEST ||
         state() == NavigationRequest::WILL_REDIRECT_REQUEST);
  modified_request_headers_.SetHeader(header_name, header_value);
}

const net::HttpResponseHeaders* NavigationHandleImpl::GetResponseHeaders() {
  if (response_headers_for_testing_)
    return response_headers_for_testing_.get();
  return navigation_request_->response()
             ? navigation_request_->response()->head.headers.get()
             : nullptr;
}

net::HttpResponseInfo::ConnectionInfo
NavigationHandleImpl::GetConnectionInfo() {
  return navigation_request_->response()
             ? navigation_request_->response()->head.connection_info
             : net::HttpResponseInfo::ConnectionInfo();
}

const base::Optional<net::SSLInfo>& NavigationHandleImpl::GetSSLInfo() {
  return navigation_request_->ssl_info();
}

const base::Optional<net::AuthChallengeInfo>&
NavigationHandleImpl::GetAuthChallengeInfo() {
  return navigation_request_->auth_challenge_info();
}

bool NavigationHandleImpl::HasCommitted() {
  return state() == NavigationRequest::DID_COMMIT ||
         state() == NavigationRequest::DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::IsErrorPage() {
  return state() == NavigationRequest::DID_COMMIT_ERROR_PAGE;
}

bool NavigationHandleImpl::HasSubframeNavigationEntryCommitted() {
  return navigation_request_->subframe_entry_committed();
}

bool NavigationHandleImpl::DidReplaceEntry() {
  return navigation_request_->did_replace_entry();
}

bool NavigationHandleImpl::ShouldUpdateHistory() {
  return navigation_request_->should_update_history();
}

const GURL& NavigationHandleImpl::GetPreviousURL() {
  return navigation_request_->previous_url();
}

net::IPEndPoint NavigationHandleImpl::GetSocketAddress() {
  // This is CANCELING because although the data comes in after
  // WILL_PROCESS_RESPONSE, it's possible for the navigation to be cancelled
  // after and the caller might want this value.
  DCHECK_GE(state(), NavigationRequest::CANCELING);
  return navigation_request_->response()
             ? navigation_request_->response()->head.remote_endpoint
             : net::IPEndPoint();
}

void NavigationHandleImpl::RegisterThrottleForTesting(
    std::unique_ptr<NavigationThrottle> navigation_throttle) {
  navigation_request_->RegisterThrottleForTesting(
      std::move(navigation_throttle));
}

bool NavigationHandleImpl::IsDeferredForTesting() {
  return navigation_request_->IsDeferredForTesting();
}

bool NavigationHandleImpl::WasStartedFromContextMenu() {
  return navigation_request_->common_params().started_from_context_menu;
}

const GURL& NavigationHandleImpl::GetSearchableFormURL() {
  return navigation_request_->begin_params()->searchable_form_url;
}

const std::string& NavigationHandleImpl::GetSearchableFormEncoding() {
  return navigation_request_->begin_params()->searchable_form_encoding;
}

ReloadType NavigationHandleImpl::GetReloadType() {
  return reload_type_;
}

RestoreType NavigationHandleImpl::GetRestoreType() {
  return restore_type_;
}

const GURL& NavigationHandleImpl::GetBaseURLForDataURL() {
  return navigation_request_->common_params().base_url_for_data_url;
}

void NavigationHandleImpl::RegisterSubresourceOverride(
    mojom::TransferrableURLLoaderPtr transferrable_loader) {
  if (!transferrable_loader)
    return;

  navigation_request_->RegisterSubresourceOverride(
      std::move(transferrable_loader));
}

const GlobalRequestID& NavigationHandleImpl::GetGlobalRequestID() {
  DCHECK_GE(state(), NavigationRequest::PROCESSING_WILL_PROCESS_RESPONSE);
  return navigation_request_->request_id();
}

bool NavigationHandleImpl::IsDownload() {
  return navigation_request_->is_download();
}

bool NavigationHandleImpl::IsFormSubmission() {
  return navigation_request_->begin_params()->is_form_submission;
}

bool NavigationHandleImpl::WasInitiatedByLinkClick() {
  return navigation_request_->begin_params()->was_initiated_by_link_click;
}

const std::string& NavigationHandleImpl::GetHrefTranslate() {
  return navigation_request_->common_params().href_translate;
}

void NavigationHandleImpl::CallResumeForTesting() {
  navigation_request_->CallResumeForTesting();
}

const base::Optional<url::Origin>& NavigationHandleImpl::GetInitiatorOrigin() {
  return navigation_request_->common_params().initiator_origin;
}

bool NavigationHandleImpl::IsSameProcess() {
  return navigation_request_->is_same_process();
}

int NavigationHandleImpl::GetNavigationEntryOffset() {
  return navigation_request_->navigation_entry_offset();
}

bool NavigationHandleImpl::FromDownloadCrossOriginRedirect() {
  return navigation_request_->from_download_cross_origin_redirect();
}

bool NavigationHandleImpl::IsSignedExchangeInnerResponse() {
  return navigation_request_->response()
             ? navigation_request_->response()
                   ->head.is_signed_exchange_inner_response
             : false;
}

bool NavigationHandleImpl::HasPrefetchedAlternativeSubresourceSignedExchange() {
  return !navigation_request_->commit_params()
              .prefetched_signed_exchanges.empty();
}

bool NavigationHandleImpl::WasResponseCached() {
  return navigation_request_->response()
             ? navigation_request_->response()->head.was_fetched_via_cache
             : false;
}

const net::ProxyServer& NavigationHandleImpl::GetProxyServer() {
  return navigation_request_->proxy_server();
}

void NavigationHandleImpl::InitServiceWorkerHandle(
    ServiceWorkerContextWrapper* service_worker_context) {
  service_worker_handle_.reset(
      new ServiceWorkerNavigationHandle(service_worker_context));
}

void NavigationHandleImpl::RenderProcessBlockedStateChanged(bool blocked) {
  AddNetworkServiceDebugEvent(std::string("B") + (blocked ? "1" : "0"));
  if (blocked)
    StopCommitTimeout();
  else
    RestartCommitTimeout();
}

void NavigationHandleImpl::StopCommitTimeout() {
  commit_timeout_timer_.Stop();
  render_process_blocked_state_changed_subscription_.reset();
  GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsResponsive();
}

void NavigationHandleImpl::RestartCommitTimeout() {
  commit_timeout_timer_.Stop();
  if (state() >= NavigationRequest::DID_COMMIT)
    return;

  RenderProcessHost* renderer_host =
      GetRenderFrameHost()->GetRenderWidgetHost()->GetProcess();
  if (!render_process_blocked_state_changed_subscription_) {
    render_process_blocked_state_changed_subscription_ =
        renderer_host->RegisterBlockStateChangedCallback(base::BindRepeating(
            &NavigationHandleImpl::RenderProcessBlockedStateChanged,
            base::Unretained(this)));
  }
  if (!renderer_host->IsBlocked())
    commit_timeout_timer_.Start(
        FROM_HERE, g_commit_timeout,
        base::BindRepeating(&NavigationHandleImpl::OnCommitTimeout,
                            weak_factory_.GetWeakPtr()));
}

void NavigationHandleImpl::OnCommitTimeout() {
  DCHECK_EQ(NavigationRequest::READY_TO_COMMIT, state());
  AddNetworkServiceDebugEvent("T");
#if defined(OS_ANDROID)
  // Rate limit the number of stack dumps so we don't overwhelm our crash
  // reports.
  // TODO(http://crbug.com/934317): Remove this once done debugging renderer
  // hangs.
  if (base::RandDouble() < 0.1) {
    static base::debug::CrashKeyString* url_key =
        base::debug::AllocateCrashKeyString("commit_timeout_url",
                                            base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_url(
        url_key, GetURL().possibly_invalid_spec());

    static base::debug::CrashKeyString* last_crash_key =
        base::debug::AllocateCrashKeyString("ns_last_crash_ms",
                                            base::debug::CrashKeySize::Size32);
    base::debug::ScopedCrashKeyString scoped_last_crash(
        last_crash_key,
        base::NumberToString(
            GetTimeSinceLastNetworkServiceCrash().InMilliseconds()));

    static base::debug::CrashKeyString* memory_key =
        base::debug::AllocateCrashKeyString("physical_memory_mb",
                                            base::debug::CrashKeySize::Size32);
    base::debug::ScopedCrashKeyString scoped_memory(
        memory_key,
        base::NumberToString(base::SysInfo::AmountOfPhysicalMemoryMB()));

    static base::debug::CrashKeyString* debug_string_key =
        base::debug::AllocateCrashKeyString("ns_debug_events",
                                            base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_debug_string(
        debug_string_key, GetNetworkServiceDebugEventsString());
    base::debug::DumpWithoutCrashing();

    if (IsOutOfProcessNetworkService())
      GetNetworkService()->DumpWithoutCrashing(base::Time::Now());
  }
#endif

  PingNetworkService(base::BindOnce(
      [](base::Time start_time) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Navigation.CommitTimeout.NetworkServicePingTime",
            base::Time::Now() - start_time);
      },
      base::Time::Now()));
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.CommitTimeout.NetworkServiceAvailability",
      GetNetworkServiceAvailability());
  base::TimeDelta last_crash_time = GetTimeSinceLastNetworkServiceCrash();
  if (!last_crash_time.is_zero()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Navigation.CommitTimeout.NetworkServiceLastCrashTime",
        last_crash_time);
  }
  UMA_HISTOGRAM_BOOLEAN("Navigation.CommitTimeout.IsRendererProcessReady",
                        GetRenderFrameHost()->GetProcess()->IsReady());
  UMA_HISTOGRAM_ENUMERATION("Navigation.CommitTimeout.Scheme",
                            GetScheme(GetURL()));
  UMA_HISTOGRAM_BOOLEAN("Navigation.CommitTimeout.IsMainFrame",
                        frame_tree_node()->IsMainFrame());
  base::UmaHistogramSparse("Navigation.CommitTimeout.ErrorCode",
                           -GetNetErrorCode());
  render_process_blocked_state_changed_subscription_.reset();
  GetRenderFrameHost()->GetRenderWidgetHost()->RendererIsUnresponsive(
      base::BindRepeating(&NavigationHandleImpl::RestartCommitTimeout,
                          weak_factory_.GetWeakPtr()));
}

// static
void NavigationHandleImpl::SetCommitTimeoutForTesting(
    const base::TimeDelta& timeout) {
  if (timeout.is_zero())
    g_commit_timeout = kDefaultCommitTimeout;
  else
    g_commit_timeout = timeout;
}

}  // namespace content
