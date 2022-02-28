// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"

#include <inttypes.h>
#include <cstdint>

#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/common/debug_utils.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

std::string DescribeFeatures(BlockListedFeatures blocklisted_features) {
  std::vector<std::string> features;
  for (WebSchedulerTrackedFeature feature : blocklisted_features) {
    features.push_back(blink::scheduler::FeatureToHumanReadableString(feature));
  }
  return base::JoinString(features, ", ");
}

const char* BrowsingInstanceSwapResultToString(
    absl::optional<ShouldSwapBrowsingInstance> reason) {
  if (!reason)
    return "no BI swap result";
  switch (reason.value()) {
    case ShouldSwapBrowsingInstance::kYes_ForceSwap:
      return "forced BI swap";
    case ShouldSwapBrowsingInstance::kNo_ProactiveSwapDisabled:
      return "BI not swapped - proactive swap disabled";
    case ShouldSwapBrowsingInstance::kNo_NotMainFrame:
      return "BI not swapped - not a main frame";
    case ShouldSwapBrowsingInstance::kNo_HasRelatedActiveContents:
      return "BI not swapped - has related active contents";
    case ShouldSwapBrowsingInstance::kNo_DoesNotHaveSite:
      return "BI not swapped - current SiteInstance does not have site";
    case ShouldSwapBrowsingInstance::kNo_SourceURLSchemeIsNotHTTPOrHTTPS:
      return "BI not swapped - source URL scheme is not HTTP(S)";
    case ShouldSwapBrowsingInstance::kNo_SameSiteNavigation:
      return "BI not swapped - same site navigation";
    case ShouldSwapBrowsingInstance::kNo_AlreadyHasMatchingBrowsingInstance:
      return "BI not swapped - already has matching BrowsingInstance";
    case ShouldSwapBrowsingInstance::kNo_RendererDebugURL:
      return "BI not swapped - URL is a renderer debug URL";
    case ShouldSwapBrowsingInstance::kNo_NotNeededForBackForwardCache:
      return "BI not swapped - old page can't be stored in bfcache";
    case ShouldSwapBrowsingInstance::kYes_CrossSiteProactiveSwap:
      return "proactively swapped BI (cross-site)";
    case ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap:
      return "proactively swapped BI (same-site)";
    case ShouldSwapBrowsingInstance::kNo_SameDocumentNavigation:
      return "BI not swapped - same-document navigation";
    case ShouldSwapBrowsingInstance::kNo_SameUrlNavigation:
      return "BI not swapped - navigation to the same URL";
    case ShouldSwapBrowsingInstance::kNo_WillReplaceEntry:
      return "BI not swapped - navigation entry will be replaced";
    case ShouldSwapBrowsingInstance::kNo_Reload:
      return "BI not swapped - reloading";
    case ShouldSwapBrowsingInstance::kNo_Guest:
      return "BI not swapped - <webview> guest";
    case ShouldSwapBrowsingInstance::kNo_HasNotComittedAnyNavigation:
      return "BI not swapped - hasn't committed any navigation";
    case ShouldSwapBrowsingInstance::
        kNo_UnloadHandlerExistsOnSameSiteNavigation:
      return "BI not swapped - unload handler exists and the navigation is "
             "same-site";
  }
}

using ProtoEnum =
    perfetto::protos::pbzero::BackForwardCacheCanStoreDocumentResult;
ProtoEnum::BackForwardCacheNotRestoredReason NotRestoredReasonToTraceEnum(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;
  switch (reason) {
    case Reason::kNotMainFrame:
      return ProtoEnum::NOT_MAIN_FRAME;
    case Reason::kBackForwardCacheDisabled:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED;
    case Reason::kRelatedActiveContentsExist:
      return ProtoEnum::RELATED_ACTIVE_CONTENTS_EXIST;
    case Reason::kHTTPStatusNotOK:
      return ProtoEnum::HTTP_STATUS_NOT_OK;
    case Reason::kSchemeNotHTTPOrHTTPS:
      return ProtoEnum::SCHEME_NOT_HTTP_OR_HTTPS;
    case Reason::kLoading:
      return ProtoEnum::LOADING;
    case Reason::kWasGrantedMediaAccess:
      return ProtoEnum::WAS_GRANTED_MEDIA_ACCESS;
    case Reason::kDisableForRenderFrameHostCalled:
      return ProtoEnum::DISABLE_FOR_RENDER_FRAME_HOST_CALLED;
    case Reason::kDomainNotAllowed:
      return ProtoEnum::DOMAIN_NOT_ALLOWED;
    case Reason::kHTTPMethodNotGET:
      return ProtoEnum::HTTP_METHOD_NOT_GET;
    case Reason::kSubframeIsNavigating:
      return ProtoEnum::SUBFRAME_IS_NAVIGATING;
    case Reason::kTimeout:
      return ProtoEnum::TIMEOUT;
    case Reason::kCacheLimit:
      return ProtoEnum::CACHE_LIMIT;
    case Reason::kJavaScriptExecution:
      return ProtoEnum::JAVASCRIPT_EXECUTION;
    case Reason::kRendererProcessKilled:
      return ProtoEnum::RENDERER_PROCESS_KILLED;
    case Reason::kRendererProcessCrashed:
      return ProtoEnum::RENDERER_PROCESS_CRASHED;
    case Reason::kGrantedMediaStreamAccess:
      return ProtoEnum::GRANTED_MEDIA_STREAM_ACCESS;
    case Reason::kSchedulerTrackedFeatureUsed:
      return ProtoEnum::SCHEDULER_TRACKED_FEATURE_USED;
    case Reason::kConflictingBrowsingInstance:
      return ProtoEnum::CONFLICTING_BROWSING_INSTANCE;
    case Reason::kCacheFlushed:
      return ProtoEnum::CACHE_FLUSHED;
    case Reason::kServiceWorkerVersionActivation:
      return ProtoEnum::SERVICE_WORKER_VERSION_ACTIVATION;
    case Reason::kSessionRestored:
      return ProtoEnum::SESSION_RESTORED;
    case Reason::kServiceWorkerPostMessage:
      return ProtoEnum::SERVICE_WORKER_POST_MESSAGE;
    case Reason::kEnteredBackForwardCacheBeforeServiceWorkerHostAdded:
      return ProtoEnum::
          ENTERED_BACK_FORWARD_CACHE_BEFORE_SERVICE_WORKER_HOST_ADDED;
    case Reason::kNotMostRecentNavigationEntry:
      return ProtoEnum::NOT_MOST_RECENT_NAVIGATION_ENTRY;
    case Reason::kServiceWorkerClaim:
      return ProtoEnum::SERVICE_WORKER_CLAIM;
    case Reason::kIgnoreEventAndEvict:
      return ProtoEnum::IGNORE_EVENT_AND_EVICT;
    case Reason::kHaveInnerContents:
      return ProtoEnum::HAVE_INNER_CONTENTS;
    case Reason::kTimeoutPuttingInCache:
      return ProtoEnum::TIMEOUT_PUTTING_IN_CACHE;
    case Reason::kBackForwardCacheDisabledByLowMemory:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED_BY_LOW_MEMORY;
    case Reason::kBackForwardCacheDisabledByCommandLine:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED_BY_COMMAND_LINE;
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return ProtoEnum::NETWORK_REQUEST_DATAPIPE_DRAINED_AS_BYTES_CONSUMER;
    case Reason::kNetworkRequestRedirected:
      return ProtoEnum::NETWORK_REQUEST_REDIRECTED;
    case Reason::kNetworkRequestTimeout:
      return ProtoEnum::NETWORK_REQUEST_TIMEOUT;
    case Reason::kNetworkExceedsBufferLimit:
      return ProtoEnum::NETWORK_EXCEEDS_BUFFER_LIMIT;
    case Reason::kNavigationCancelledWhileRestoring:
      return ProtoEnum::NAVIGATION_CANCELLED_WHILE_RESTORING;
    case Reason::kBackForwardCacheDisabledForPrerender:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED_FOR_PRERENDER;
    case Reason::kUserAgentOverrideDiffers:
      return ProtoEnum::USER_AGENT_OVERRIDE_DIFFERS;
    case Reason::kForegroundCacheLimit:
      return ProtoEnum::FOREGROUND_CACHE_LIMIT;
    case Reason::kBrowsingInstanceNotSwapped:
      return ProtoEnum::BROWSING_INSTANCE_NOT_SWAPPED;
    case Reason::kBackForwardCacheDisabledForDelegate:
      return ProtoEnum::BACK_FORWARD_CACHE_DISABLED_FOR_DELEGATE;
    case Reason::kOptInUnloadHeaderNotPresent:
      return ProtoEnum::OPT_IN_UNLOAD_HEADER_NOT_PRESENT;
    case Reason::kUnloadHandlerExistsInMainFrame:
      return ProtoEnum::UNLOAD_HANDLER_EXISTS_IN_MAIN_FRAME;
    case Reason::kUnloadHandlerExistsInSubFrame:
      return ProtoEnum::UNLOAD_HANDLER_EXISTS_IN_SUBFRAME;
    case Reason::kServiceWorkerUnregistration:
      return ProtoEnum::SERVICE_WORKER_UNREGISTRATION;
    case Reason::kCacheControlNoStore:
      return ProtoEnum::CACHE_CONTROL_NO_STORE;
    case Reason::kCacheControlNoStoreCookieModified:
      return ProtoEnum::CACHE_CONTROL_NO_STORE_COOKIE_MODIFIED;
    case Reason::kCacheControlNoStoreHTTPOnlyCookieModified:
      return ProtoEnum::CACHE_CONTROL_NO_STORE_HTTP_ONLY_COOKIE_MODIFIED;
    case Reason::kNoResponseHead:
      return ProtoEnum::NO_RESPONSE_HEAD;
    case Reason::kActivationNavigationsDisallowedForBug1234857:
      return ProtoEnum::ACTIVATION_NAVIGATION_DISALLOWED_FOR_BUG_1234857;
    case Reason::kBlocklistedFeatures:
      return ProtoEnum::BLOCKLISTED_FEATURES;
    case Reason::kUnknown:
      return ProtoEnum::UNKNOWN;
  }
  NOTREACHED();
  return ProtoEnum::UNKNOWN;
}

}  // namespace

void BackForwardCacheCanStoreDocumentResult::WriteIntoTrace(
    perfetto::TracedProto<
        perfetto::protos::pbzero::BackForwardCacheCanStoreDocumentResult>
        result) const {
  for (auto reason : not_stored_reasons()) {
    result->set_back_forward_cache_not_restored_reason(
        NotRestoredReasonToTraceEnum(reason));
  }
}

bool BackForwardCacheCanStoreDocumentResult::HasNotStoredReason(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  return not_stored_reasons_.Has(reason);
}

void BackForwardCacheCanStoreDocumentResult::AddNotStoredReason(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  not_stored_reasons_.Put(reason);

  if (reason == BackForwardCacheMetrics::NotRestoredReason::kNoResponseHead ||
      reason ==
          BackForwardCacheMetrics::NotRestoredReason::kSchemeNotHTTPOrHTTPS) {
    if (not_stored_reasons_.Has(
            BackForwardCacheMetrics::NotRestoredReason::kNoResponseHead) &&
        not_stored_reasons_.Has(BackForwardCacheMetrics::NotRestoredReason::
                                    kSchemeNotHTTPOrHTTPS) &&
        !not_stored_reasons_.Has(
            BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK)) {
      CaptureTraceForNavigationDebugScenario(
          DebugScenario::kDebugNoResponseHeadForHttpOrHttps);
      base::debug::DumpWithoutCrashing();
    }
  }
}

bool BackForwardCacheCanStoreDocumentResult::CanStore() const {
  return not_stored_reasons_.Empty();
}

namespace {
std::string DisabledReasonsToString(
    const std::set<BackForwardCache::DisabledReason>& reasons) {
  std::vector<std::string> descriptions;
  for (const auto& reason : reasons) {
    descriptions.push_back(base::StringPrintf(
        "%d:%d:%s", reason.source, reason.id, reason.description.c_str()));
  }
  return base::JoinString(descriptions, ", ");
}

std::string DisallowActivationReasonsToString(
    const std::set<uint64_t>& reasons) {
  std::vector<std::string> descriptions;
  for (const uint64_t reason : reasons) {
    descriptions.push_back(base::StringPrintf("%" PRIu64, reason));
  }
  return base::JoinString(descriptions, ", ");
}
}  // namespace

std::string BackForwardCacheCanStoreDocumentResult::ToString() const {
  if (CanStore())
    return "Yes";

  std::vector<std::string> reason_strs;

  for (BackForwardCacheMetrics::NotRestoredReason reason :
       not_stored_reasons_) {
    reason_strs.push_back(NotRestoredReasonToString(reason));
  }

  return "No: " + base::JoinString(reason_strs, ", ");
}

std::string BackForwardCacheCanStoreDocumentResult::NotRestoredReasonToString(
    BackForwardCacheMetrics::NotRestoredReason reason) const {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;

  switch (reason) {
    case Reason::kNotMainFrame:
      return "not a main frame";
    case Reason::kBackForwardCacheDisabled:
      return "BackForwardCache disabled";
    case Reason::kRelatedActiveContentsExist:
      return base::StringPrintf(
          "related active contents exist: %s",
          BrowsingInstanceSwapResultToString(browsing_instance_swap_result_));
    case Reason::kHTTPStatusNotOK:
      return "HTTP status is not OK";
    case Reason::kSchemeNotHTTPOrHTTPS:
      return "scheme is not HTTP or HTTPS";
    case Reason::kLoading:
      return "frame is not fully loaded";
    case Reason::kWasGrantedMediaAccess:
      return "frame was granted microphone or camera access";
    case Reason::kBlocklistedFeatures:
      return "blocklisted features: " + DescribeFeatures(blocklisted_features_);
    case Reason::kDisableForRenderFrameHostCalled:
      return "BackForwardCache::DisableForRenderFrameHost() was called: " +
             DisabledReasonsToString(disabled_reasons_);
    case Reason::kDomainNotAllowed:
      return "This domain is not allowed to be stored in BackForwardCache";
    case Reason::kHTTPMethodNotGET:
      return "HTTP method is not GET";
    case Reason::kSubframeIsNavigating:
      return "subframe navigation is in progress";
    case Reason::kTimeout:
      return "timeout";
    case Reason::kCacheLimit:
      return "cache limit";
    case Reason::kForegroundCacheLimit:
      return "foreground cache limit";
    case Reason::kJavaScriptExecution:
      return "JavaScript execution";
    case Reason::kRendererProcessKilled:
      return "renderer process is killed";
    case Reason::kRendererProcessCrashed:
      return "renderer process crashed";
    case Reason::kGrantedMediaStreamAccess:
      return "granted media stream access";
    case Reason::kSchedulerTrackedFeatureUsed:
      return "scheduler tracked feature is used";
    case Reason::kConflictingBrowsingInstance:
      return "conflicting BrowsingInstance";
    case Reason::kCacheFlushed:
      return "cache flushed";
    case Reason::kServiceWorkerVersionActivation:
      return "service worker version is activated";
    case Reason::kSessionRestored:
      return "session restored";
    case Reason::kUnknown:
      return "unknown";
    case Reason::kServiceWorkerPostMessage:
      return "postMessage from service worker";
    case Reason::kEnteredBackForwardCacheBeforeServiceWorkerHostAdded:
      return "frame already in the cache when service worker host was added";
    case Reason::kNotMostRecentNavigationEntry:
      return "navigation entry is not the most recent one for this document";
    case Reason::kServiceWorkerClaim:
      return "service worker claim is called";
    case Reason::kIgnoreEventAndEvict:
      return "IsInactiveAndDisallowReactivation() was called for the frame in "
             "bfcache " +
             DisallowActivationReasonsToString(disallow_activation_reasons_);
    case Reason::kHaveInnerContents:
      return "RenderFrameHost has inner WebContents attached";
    case Reason::kTimeoutPuttingInCache:
      return "Timed out while waiting for page to acknowledge freezing";
    case Reason::kBackForwardCacheDisabledByLowMemory:
      return "BackForwardCache is disabled due to low memory of the device";
    case Reason::kBackForwardCacheDisabledByCommandLine:
      return "BackForwardCache is disabled through command line (may include "
             "cases where the embedder disabled it due to, e.g., enterprise "
             "policy)";
    case Reason::kNavigationCancelledWhileRestoring:
      return "Navigation request was cancelled after js eviction was disabled";
    case Reason::kNetworkRequestRedirected:
      return "Network request is redirected in bfcache";
    case Reason::kNetworkRequestTimeout:
      return "Network request is open for too long and exceeds time limit";
    case Reason::kNetworkExceedsBufferLimit:
      return "Network request reads too much data and exceeds buffer limit";
    case Reason::kBackForwardCacheDisabledForPrerender:
      return "BackForwardCache is disabled for Prerender";
    case Reason::kUserAgentOverrideDiffers:
      return "User-agent override differs";
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return "Network requests' datapipe has been passed as bytes consumer";
    case Reason::kBrowsingInstanceNotSwapped:
      return "Browsing instance is not swapped";
    case Reason::kBackForwardCacheDisabledForDelegate:
      return "BackForwardCache is not supported by delegate";
    case Reason::kOptInUnloadHeaderNotPresent:
      return "BFCache-Opt-In header not present, or does not include `unload` "
             "token, and an experimental config which requires it is active.";
    case Reason::kUnloadHandlerExistsInMainFrame:
      return "Unload handler exists in the main frame, and the current "
             "experimental config doesn't permit it to be BFCached.";
    case Reason::kUnloadHandlerExistsInSubFrame:
      return "Unload handler exists in a sub frame, and the current "
             "experimental config doesn't permit it to be BFCached.";
    case Reason::kServiceWorkerUnregistration:
      return "ServiceWorker is unregistered while the controllee page is in "
             "bfcache.";
    case Reason::kCacheControlNoStore:
      return "Pages with cache-control:no-store went into bfcache temporarily "
             "because of the flag, and there was no cookie change.";
    case Reason::kCacheControlNoStoreCookieModified:
      return "Pages with cache-control:no-store went into bfcache temporarily "
             "because of the flag, and while in bfcache the cookie was "
             "modified or deleted and thus evicted.";
    case Reason::kCacheControlNoStoreHTTPOnlyCookieModified:
      return "Pages with cache-control:no-store went into bfcache temporarily "
             "because of the flag, and while in bfcache the HTTP-only cookie"
             "was modified or deleted and thus evicted.";
    case Reason::kNoResponseHead:
      return "main RenderFrameHost doesn't have response headers set, probably "
             "due not having successfully committed a navigation.";
    case Reason::kActivationNavigationsDisallowedForBug1234857:
      return "Activation navigations are disallowed to avoid bypassing "
             "PasswordProtectionService as a workaround for "
             "https://crbug.com/1234857.";
  }
}

void BackForwardCacheCanStoreDocumentResult::No(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  // Either |NoDueToFeatures()| or |NoDueToDisableForRenderFrameHostCalled|
  // should be called instead.
  DCHECK_NE(reason,
            BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures);
  DCHECK_NE(reason, BackForwardCacheMetrics::NotRestoredReason::
                        kDisableForRenderFrameHostCalled);

  AddNotStoredReason(reason);
}

void BackForwardCacheCanStoreDocumentResult::NoDueToFeatures(
    BlockListedFeatures features) {
  AddNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures);
  blocklisted_features_.PutAll(features);
}

void BackForwardCacheCanStoreDocumentResult::
    NoDueToDisableForRenderFrameHostCalled(
        const std::set<BackForwardCache::DisabledReason>& reasons) {
  AddNotStoredReason(BackForwardCacheMetrics::NotRestoredReason::
                         kDisableForRenderFrameHostCalled);
  for (const BackForwardCache::DisabledReason& reason : reasons)
    disabled_reasons_.insert(reason);
}

void BackForwardCacheCanStoreDocumentResult::NoDueToRelatedActiveContents(
    absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result) {
  AddNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kRelatedActiveContentsExist);
  browsing_instance_swap_result_ = browsing_instance_swap_result;
}

void BackForwardCacheCanStoreDocumentResult::NoDueToDisallowActivation(
    uint64_t reason) {
  AddNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict);
  disallow_activation_reasons_.insert(reason);
}

void BackForwardCacheCanStoreDocumentResult::NoDueToAXEvents(
    const std::vector<ui::AXEvent>& events) {
  for (auto& event : events) {
    ax_events_.insert(event.event_type);
  }
  AddNotStoredReason(
      BackForwardCacheMetrics::NotRestoredReason::kIgnoreEventAndEvict);
  disallow_activation_reasons_.insert(DisallowActivationReasonId::kAXEvent);
}

void BackForwardCacheCanStoreDocumentResult::AddReasonsFrom(
    const BackForwardCacheCanStoreDocumentResult& other) {
  not_stored_reasons_.PutAll(other.not_stored_reasons_);
  blocklisted_features_.PutAll(other.blocklisted_features());
  for (const BackForwardCache::DisabledReason& reason :
       other.disabled_reasons()) {
    disabled_reasons_.insert(reason);
  }
  if (other.browsing_instance_swap_result_)
    browsing_instance_swap_result_ = other.browsing_instance_swap_result_;
  for (const auto reason : other.disallow_activation_reasons()) {
    disallow_activation_reasons_.insert(reason);
  }
  for (const auto event : other.ax_events()) {
    ax_events_.insert(event);
  }
}

BackForwardCacheCanStoreDocumentResult::
    BackForwardCacheCanStoreDocumentResult() = default;
BackForwardCacheCanStoreDocumentResult::BackForwardCacheCanStoreDocumentResult(
    BackForwardCacheCanStoreDocumentResult&&) = default;
BackForwardCacheCanStoreDocumentResult&
BackForwardCacheCanStoreDocumentResult::operator=(
    BackForwardCacheCanStoreDocumentResult&&) = default;
BackForwardCacheCanStoreDocumentResult::
    ~BackForwardCacheCanStoreDocumentResult() = default;

}  // namespace content
