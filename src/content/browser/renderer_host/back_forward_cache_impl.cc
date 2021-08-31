// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/visibility.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-shared.h"
#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace content {

class RenderProcessHostInternalObserver;

namespace {

using blink::scheduler::WebSchedulerTrackedFeature;

// The default number of entries the BackForwardCache can hold per tab.
static constexpr size_t kDefaultBackForwardCacheSize = 1;

// The default number value for the "foreground_cache_size" field trial
// parameter. This parameter controls the numbers of entries associated with
// foregrounded process the BackForwardCache can hold per tab, when using the
// foreground/background cache-limiting strategy. This strategy is enabled if
// the parameter values is non-zero.
static constexpr size_t kDefaultForegroundBackForwardCacheSize = 0;

// The default time to live in seconds for documents in BackForwardCache.
static constexpr int kDefaultTimeToLiveInBackForwardCacheInSeconds = 15;

#if defined(OS_ANDROID)
bool IsProcessBindingEnabled() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled())
    return false;
  const std::string process_binding_param =
      base::GetFieldTrialParamValueByFeature(features::kBackForwardCache,
                                             "process_binding_strength");
  return process_binding_param.empty() || process_binding_param == "DISABLE";
}

// Association of ChildProcessImportance to corresponding string names.
const base::FeatureParam<ChildProcessImportance>::Option
    child_process_importance_options[] = {
        {ChildProcessImportance::IMPORTANT, "IMPORTANT"},
        {ChildProcessImportance::MODERATE, "MODERATE"},
        {ChildProcessImportance::NORMAL, "NORMAL"}};

// Defines the binding strength for a processes holding cached pages. The value
// is read from an experiment parameter value. Ideally this would be lower than
// the one for processes holding the foreground page and similar to that of
// background tabs so that the OS will hopefully kill the foreground tab last.
// The default importance is set to MODERATE.
const base::FeatureParam<ChildProcessImportance> kChildProcessImportanceParam{
    &features::kBackForwardCache, "process_binding_strength",
    ChildProcessImportance::MODERATE, &child_process_importance_options};
#endif

bool IsGeolocationSupported() {
  if (!IsBackForwardCacheEnabled())
    return false;
  static constexpr base::FeatureParam<bool> geolocation_supported(
      &features::kBackForwardCache, "geolocation_supported",
      true
  );
  return geolocation_supported.Get();
}

bool IsContentInjectionSupported() {
  if (!IsBackForwardCacheEnabled())
    return false;
  static constexpr base::FeatureParam<bool> content_injection_supported(
      &features::kBackForwardCache, "content_injection_supported", false);
  return content_injection_supported.Get();
}

bool IsFileSystemSupported() {
  if (!IsBackForwardCacheEnabled())
    return false;
  static constexpr base::FeatureParam<bool> file_system_api_supported(
      &features::kBackForwardCache, "file_system_api_supported", false);
  return file_system_api_supported.Get();
}

bool IsOptInHeaderRequired() {
  if (!IsBackForwardCacheEnabled())
    return false;

  // TODO(crbug.com/1201653): Remove this feature param and make it one of the
  //                          `unload_support`.
  static constexpr base::FeatureParam<bool> opt_in_header_required(
      &features::kBackForwardCache, "opt_in_header_required", false);
  return opt_in_header_required.Get();
}

enum class HeaderPresence {
  kNotPresent,
  kPresent,
  kUnsure,
};

HeaderPresence OptInUnloadHeaderPresence(RenderFrameHostImpl* rfh) {
  const network::mojom::URLResponseHeadPtr& response_head =
      rfh->last_response_head();
  if (!response_head)
    return HeaderPresence::kUnsure;

  const network::mojom::ParsedHeadersPtr& headers =
      response_head->parsed_headers;
  if (!headers)
    return HeaderPresence::kUnsure;

  return headers->bfcache_opt_in_unload ? HeaderPresence::kPresent
                                        : HeaderPresence::kNotPresent;
}

constexpr base::FeatureParam<BackForwardCacheImpl::UnloadSupportStrategy>::
    Option kUnloadSupportStrategyOptions[] = {
        {BackForwardCacheImpl::UnloadSupportStrategy::kAlways, "always"},
        {BackForwardCacheImpl::UnloadSupportStrategy::kOptInHeaderRequired,
         "opt_in_header_required"},
        {BackForwardCacheImpl::UnloadSupportStrategy::kNo, "no"},
};

BackForwardCacheImpl::UnloadSupportStrategy GetUnloadSupportStrategy() {
  // TODO(crbug.com/1201653): Make the default "kNo" for desktops once
  //                          the experiment config is updated.
  constexpr auto kDefaultStrategy =
      BackForwardCacheImpl::UnloadSupportStrategy::kAlways;

  if (!IsBackForwardCacheEnabled())
    return kDefaultStrategy;

  static constexpr base::FeatureParam<
      BackForwardCacheImpl::UnloadSupportStrategy>
      unload_support(&features::kBackForwardCache, "unload_support",
                     kDefaultStrategy, &kUnloadSupportStrategyOptions);
  return unload_support.Get();
}

uint64_t SupportedFeaturesBitmaskImpl() {
  if (!IsBackForwardCacheEnabled())
    return 0;

  static constexpr base::FeatureParam<std::string> supported_features(
      &features::kBackForwardCache, "supported_features", "");
  std::vector<std::string> tokens =
      base::SplitString(supported_features.Get(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  uint64_t mask = 0;
  for (const std::string& token : tokens) {
    auto feature = blink::scheduler::StringToFeature(token);
    DCHECK(feature.has_value()) << "invalid feature string: " << token;
    if (feature.has_value()) {
      mask |= blink::scheduler::FeatureToBit(feature.value());
    }
  }
  return mask;
}

uint64_t SupportedFeaturesBitmask() {
  static uint64_t mask = SupportedFeaturesBitmaskImpl();
  return mask;
}

bool IgnoresOutstandingNetworkRequestForTesting() {
  if (!IsBackForwardCacheEnabled())
    return false;
  static constexpr base::FeatureParam<bool>
      outstanding_network_request_supported(
          &features::kBackForwardCache,
          "ignore_outstanding_network_request_for_testing", false);
  return outstanding_network_request_supported.Get();
}

// Ignore all features that the page is using and all DisableForRenderFrameHost
// calls and force all pages to be cached. Should be used only for local testing
// and debugging -- things will break when this param is used.
bool ShouldIgnoreBlocklists() {
  if (!IsBackForwardCacheEnabled())
    return false;
  static constexpr base::FeatureParam<bool> should_ignore_blocklists(
      &features::kBackForwardCache, "should_ignore_blocklists", false);
  return should_ignore_blocklists.Get();
}

enum RequestedFeatures { kAll, kOnlySticky };

uint64_t GetDisallowedFeatures(RenderFrameHostImpl* rfh,
                               RequestedFeatures requested_features) {
  // TODO(https://crbug.com/1015784): Finalize disallowed feature list, and test
  // for each disallowed feature.
  constexpr uint64_t kAlwaysDisallowedFeatures =
      FeatureToBit(WebSchedulerTrackedFeature::kAppBanner) |
      FeatureToBit(WebSchedulerTrackedFeature::kBroadcastChannel) |
      FeatureToBit(WebSchedulerTrackedFeature::kContainsPlugins) |
      FeatureToBit(WebSchedulerTrackedFeature::kDedicatedWorkerOrWorklet) |
      FeatureToBit(WebSchedulerTrackedFeature::kIdleManager) |
      FeatureToBit(WebSchedulerTrackedFeature::kIndexedDBConnection) |
      FeatureToBit(WebSchedulerTrackedFeature::kKeyboardLock) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kOutstandingIndexedDBTransaction) |
      FeatureToBit(WebSchedulerTrackedFeature::kPaymentManager) |
      FeatureToBit(WebSchedulerTrackedFeature::kPictureInPicture) |
      FeatureToBit(WebSchedulerTrackedFeature::kPortal) |
      FeatureToBit(WebSchedulerTrackedFeature::kPrinting) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedAudioCapturePermission) |
      FeatureToBit(WebSchedulerTrackedFeature::
                       kRequestedBackForwardCacheBlockedSensors) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission) |
      FeatureToBit(WebSchedulerTrackedFeature::kRequestedMIDIPermission) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedNotificationsPermission) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kRequestedVideoCapturePermission) |
      FeatureToBit(WebSchedulerTrackedFeature::kSharedWorker) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebOTPService) |
      FeatureToBit(WebSchedulerTrackedFeature::kSpeechRecognizer) |
      FeatureToBit(WebSchedulerTrackedFeature::kSpeechSynthesis) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebDatabase) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebHID) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebLocks) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebRTC) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebShare) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebSocket) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebVR) |
      FeatureToBit(WebSchedulerTrackedFeature::kWebXR) |
      FeatureToBit(
          WebSchedulerTrackedFeature::kMediaSessionImplOnServiceCreated);

  uint64_t result = kAlwaysDisallowedFeatures;

  if (!IsGeolocationSupported()) {
    result |= FeatureToBit(
        WebSchedulerTrackedFeature::kRequestedGeolocationPermission);
  }

  if (!IsContentInjectionSupported()) {
    result |= FeatureToBit(WebSchedulerTrackedFeature::kIsolatedWorldScript) |
              FeatureToBit(WebSchedulerTrackedFeature::kInjectedStyleSheet);
  }

  if (!IgnoresOutstandingNetworkRequestForTesting()) {
    result |=
        FeatureToBit(
            WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers) |
        FeatureToBit(
            WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch) |
        FeatureToBit(WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR);
  }

  if (!IsFileSystemSupported()) {
    result |= FeatureToBit(WebSchedulerTrackedFeature::kWebFileSystem);
  }

  if (requested_features == RequestedFeatures::kOnlySticky) {
    // Remove all non-sticky features from |result|.
    result &= blink::scheduler::StickyFeaturesBitmask();
  }

  result &= ~SupportedFeaturesBitmask();

  return result;
}

// The BackForwardCache feature is controlled via an experiment. This function
// returns the allowed URL list where it is enabled.
std::string GetAllowedURLList() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled()) {
    if (base::FeatureList::IsEnabled(
            kRecordBackForwardCacheMetricsWithoutEnabling)) {
      return base::GetFieldTrialParamValueByFeature(
          kRecordBackForwardCacheMetricsWithoutEnabling, "allowed_websites");
    }
    return "";
  }

  return base::GetFieldTrialParamValueByFeature(features::kBackForwardCache,
                                                "allowed_websites");
}

// This function returns the blocked URL list.
std::string GetBlockedURLList() {
  return IsBackForwardCacheEnabled()
             ? base::GetFieldTrialParamValueByFeature(
                   features::kBackForwardCache, "blocked_websites")
             : "";
}

// Returns the list of blocked CGI params
std::string GetBlockedCgiParams() {
  return IsBackForwardCacheEnabled()
             ? base::GetFieldTrialParamValueByFeature(
                   features::kBackForwardCache, "blocked_cgi_params")
             : "";
}

// Parses the “allowed_websites” and "blocked_websites" field trial parameters
// and creates a map to represent hosts and corresponding path prefixes.
std::map<std::string, std::vector<std::string>> ParseCommaSeparatedURLs(
    base::StringPiece comma_separated_urls) {
  std::map<std::string, std::vector<std::string>> urls;
  for (auto& it :
       base::SplitString(comma_separated_urls, ",", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL)) {
    GURL url = GURL(it);
    urls[url.host()].push_back(url.path());
  }
  return urls;
}

// Parses the "cgi_params" field trial parameter into a set by splitting on "|".
std::unordered_set<std::string> ParseBlockedCgiParams(
    base::StringPiece cgi_params_string) {
  std::vector<std::string> split =
      base::SplitString(cgi_params_string, "|", base::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  std::unordered_set<std::string> cgi_params;
  cgi_params.insert(split.begin(), split.end());
  return cgi_params;
}

BackForwardCacheTestDelegate* g_bfcache_disabled_test_observer = nullptr;

void RestoreBrowserControlsState(RenderFrameHostImpl* cached_rfh) {
  auto* current_rfh =
      cached_rfh->frame_tree_node()->render_manager()->current_frame_host();

  DCHECK_NE(current_rfh, cached_rfh);

  float prev_top_controls_shown_ratio = current_rfh->GetRenderWidgetHost()
                                            ->render_frame_metadata_provider()
                                            ->LastRenderFrameMetadata()
                                            .top_controls_shown_ratio;
  if (prev_top_controls_shown_ratio < 1) {
    // Make sure the state in the restored renderer matches the current one.
    // If we currently aren't showing the controls let the cached renderer
    // know, so that it then reacts correctly to the SHOW controls message
    // that might follow during DidCommitNavigation.
    cached_rfh->UpdateBrowserControlsState(
        cc::BrowserControlsState::kBoth, cc::BrowserControlsState::kHidden,
        // Do not animate as we want this to happen "instantaneously"
        false);
  }
}

void RequestRecordTimeToVisible(RenderFrameHostImpl* rfh,
                                base::TimeTicks navigation_start) {
  // Make sure we record only when the frame is not in hidden state to avoid
  // cases like page navigating back with window.history.back(), while being
  // hidden.
  if (rfh->delegate()->GetVisibility() != Visibility::HIDDEN) {
    rfh->GetView()->SetRecordContentToVisibleTimeRequest(
        navigation_start, false /* destination_is_loaded */,
        false /* show_reason_tab_switching */,
        false /* show_reason_unoccluded */,
        true /* show_reason_bfcache_restore */);
  }
}

// Returns true if any of the processes associated with the RenderViewHosts in
// this Entry are foregrounded.
bool HasForegroundedProcess(BackForwardCacheImpl::Entry& entry) {
  for (auto* rvh : entry.render_view_hosts) {
    if (!rvh->GetProcess()->IsProcessBackgrounded()) {
      return true;
    }
  }
  return false;
}

// Returns true if all of the RenderViewHosts in this Entry have received the
// acknowledgement from renderer.
bool AllRenderViewHostsReceivedAckFromRenderer(
    BackForwardCacheImpl::Entry& entry) {
  for (auto* rvh : entry.render_view_hosts) {
    if (!rvh->DidReceiveBackForwardCacheAck()) {
      return false;
    }
  }
  return true;
}

}  // namespace

// static
BackForwardCacheImpl::MessageHandlingPolicyWhenCached
BackForwardCacheImpl::GetChannelAssociatedMessageHandlingPolicy() {
  // Avoid activating BackForwardCache trial for checking the parameters
  // associated with it.
  if (!IsBackForwardCacheEnabled())
    return kMessagePolicyNone;

  static constexpr char kFieldTrialParam[] = "message_handling_when_cached";
  auto param = base::GetFieldTrialParamValueByFeature(
      features::kBackForwardCache, kFieldTrialParam);
  if (param.empty() || param == "log") {
    return kMessagePolicyLog;
  } else if (param == "none") {
    return kMessagePolicyNone;
  } else if (param == "dump") {
    return kMessagePolicyDump;
  } else {
    DLOG(WARNING) << "Failed to parse field trial param " << kFieldTrialParam
                  << " with string value " << param
                  << " under feature kBackForwardCache"
                  << features::kBackForwardCache.name;
    return kMessagePolicyLog;
  }
}

BackForwardCacheImpl::Entry::Entry(
    std::unique_ptr<RenderFrameHostImpl> rfh,
    RenderFrameProxyHostMap proxies,
    std::set<RenderViewHostImpl*> render_view_hosts)
    : render_frame_host(std::move(rfh)),
      proxy_hosts(std::move(proxies)),
      render_view_hosts(std::move(render_view_hosts)) {}

BackForwardCacheImpl::Entry::~Entry() = default;

void BackForwardCacheImpl::Entry::WriteIntoTrace(
    perfetto::TracedValue context) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("render_frame_host", render_frame_host);
}

void BackForwardCacheImpl::RenderProcessBackgroundedChanged(
    RenderProcessHostImpl* host) {
  EnforceCacheSizeLimit();
}

BackForwardCacheTestDelegate::BackForwardCacheTestDelegate() {
  DCHECK(!g_bfcache_disabled_test_observer);
  g_bfcache_disabled_test_observer = this;
}

BackForwardCacheTestDelegate::~BackForwardCacheTestDelegate() {
  DCHECK_EQ(g_bfcache_disabled_test_observer, this);
  g_bfcache_disabled_test_observer = nullptr;
}

BackForwardCacheImpl::BackForwardCacheImpl()
    : allowed_urls_(ParseCommaSeparatedURLs(GetAllowedURLList())),
      blocked_urls_(ParseCommaSeparatedURLs(GetBlockedURLList())),
      blocked_cgi_params_(ParseBlockedCgiParams(GetBlockedCgiParams())),
      unload_strategy_(GetUnloadSupportStrategy()),
      weak_factory_(this) {}

BackForwardCacheImpl::~BackForwardCacheImpl() {
  Shutdown();
}

base::TimeDelta BackForwardCacheImpl::GetTimeToLiveInBackForwardCache() {
  // We use the following order of priority if multiple values exist:
  // - The programmatical value set in params. Used in specific tests.
  // - Infinite if kBackForwardCacheNoTimeEviction is enabled.
  // - Default value otherwise, kDefaultTimeToLiveInBackForwardCacheInSeconds.
  if (base::FeatureList::IsEnabled(kBackForwardCacheNoTimeEviction) &&
      GetFieldTrialParamValueByFeature(features::kBackForwardCache,
                                       "TimeToLiveInBackForwardCacheInSeconds")
          .empty()) {
    return base::TimeDelta::Max();
  }

  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "TimeToLiveInBackForwardCacheInSeconds",
      kDefaultTimeToLiveInBackForwardCacheInSeconds));
}

// static
size_t BackForwardCacheImpl::GetCacheSize() {
  if (!IsBackForwardCacheEnabled())
    return 0;
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "cache_size", kDefaultBackForwardCacheSize);
}

// static
size_t BackForwardCacheImpl::GetForegroundedEntriesCacheSize() {
  if (!IsBackForwardCacheEnabled())
    return 0;
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kBackForwardCache, "foreground_cache_size",
      kDefaultForegroundBackForwardCacheSize);
}

// static
bool BackForwardCacheImpl::UsingForegroundBackgroundCacheSizeLimit() {
  return GetForegroundedEntriesCacheSize() > 0;
}

BackForwardCacheCanStoreDocumentResult BackForwardCacheImpl::CanStorePageNow(
    RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult result =
      CanPotentiallyStorePageLater(rfh);
  CheckDynamicBlocklistedFeaturesOnSubtree(&result, rfh);
  DVLOG(1) << "CanStorePageNow: " << rfh->GetLastCommittedURL() << " : "
           << result.ToString();
  return result;
}

BackForwardCacheCanStoreDocumentResult
BackForwardCacheImpl::CanPotentiallyStorePageLater(RenderFrameHostImpl* rfh) {
  BackForwardCacheCanStoreDocumentResult result;

  // Use the BackForwardCache only for the main frame.
  if (rfh->GetParent())
    result.No(BackForwardCacheMetrics::NotRestoredReason::kNotMainFrame);

  // If the the delegate doesn't support back forward cache, disable it.
  if (!rfh->delegate()->IsBackForwardCacheSupported()) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::
                  kBackForwardCacheDisabledForDelegate);
  }

  if (!IsBackForwardCacheEnabled() || is_disabled_for_testing_ ||
      // TODO(https://crbug.com/1176151): Replace with LifecycleState check once
      // it tracks prerender too.
      rfh->frame_tree()->is_prerendering()) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kBackForwardCacheDisabled);

    // In addition to the general "BackForwardCacheDisabled" reason above, also
    // track more specific reasons on why BackForwardCache is disabled.
    if (IsBackForwardCacheDisabledByCommandLine()) {
      result.No(BackForwardCacheMetrics::NotRestoredReason::
                    kBackForwardCacheDisabledByCommandLine);
    }

    if (!DeviceHasEnoughMemoryForBackForwardCache()) {
      result.No(BackForwardCacheMetrics::NotRestoredReason::
                    kBackForwardCacheDisabledByLowMemory);
    }

    // TODO(https://crbug.com/1176151): Replace with LifecycleState check once
    // it tracks prerender too.
    if (rfh->frame_tree()->is_prerendering()) {
      result.No(BackForwardCacheMetrics::NotRestoredReason::
                    kBackForwardCacheDisabledForPrerender);
    }
  }

  // If this function is called after we navigated to a new RenderFrameHost,
  // then |rfh| must already be replaced by the new RenderFrameHost. If this
  // function is called before we navigated, then |rfh| must be a current
  // RenderFrameHost.
  bool is_current_rfh = rfh->IsCurrent();

  // Two pages in the same BrowsingInstance can script each other. When a page
  // can be scripted from outside, it can't enter the BackForwardCache.
  //
  // If the |rfh| is not a "current" RenderFrameHost anymore, the
  // "RelatedActiveContentsCount" below is compared against 0, not 1. This is
  // because |rfh| is not "active" itself.
  //
  // This check makes sure the old and new document aren't sharing the same
  // BrowsingInstance. Note that the existence of related active contents might
  // change in the future, but we are checking this in
  // CanPotentiallyStorePageLater instead of CanStorePageNow because it's needed
  // to determine whether to do a proactive BrowsingInstance swap or not, which
  // should not be done if the page has related active contents.
  unsigned expected_related_active_contents_count = is_current_rfh ? 1 : 0;
  // We should never have fewer than expected.
  DCHECK_GE(rfh->GetSiteInstance()->GetRelatedActiveContentsCount(),
            expected_related_active_contents_count);
  if (rfh->GetSiteInstance()->GetRelatedActiveContentsCount() >
      expected_related_active_contents_count) {
    result.No(BackForwardCacheMetrics::NotRestoredReason::
                  kRelatedActiveContentsExist);
  }

  // Only store documents that have successful http status code.
  // Note that for error pages, |last_http_status_code| is equal to 0.
  if (rfh->last_http_status_code() != net::HTTP_OK)
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPStatusNotOK);

  // Only store documents that were fetched via HTTP GET method.
  if (rfh->last_http_method() != net::HttpRequestHeaders::kGetMethod)
    result.No(BackForwardCacheMetrics::NotRestoredReason::kHTTPMethodNotGET);

  // Do not store main document with non HTTP/HTTPS URL scheme. Among other
  // things, this excludes the new tab page and all WebUI pages.
  if (!rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    result.No(
        BackForwardCacheMetrics::NotRestoredReason::kSchemeNotHTTPOrHTTPS);
  }

  // We should not cache pages with Cache-control: no-store. Note that
  // even though this is categorized as a "feature", we will check this within
  // CanPotentiallyStorePageLater as it's not possible to change the HTTP
  // headers, so if it's not possible to cache this page now due to this, it's
  // impossible to cache this page later.
  // TODO(rakina): Once we move cache-control tracking to RenderFrameHostImpl,
  // change this part to use the information stored in RenderFrameHostImpl
  // instead.
  uint64_t cache_control_no_store_feature = FeatureToBit(
      WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore);
  if (rfh->scheduler_tracked_features() & cache_control_no_store_feature) {
    result.NoDueToFeatures(cache_control_no_store_feature);
  }

  // Only store documents that have URLs allowed through experiment.
  if (!IsAllowed(rfh->GetLastCommittedURL()))
    result.No(BackForwardCacheMetrics::NotRestoredReason::kDomainNotAllowed);

  if (IsOptInHeaderRequired()) {
    HeaderPresence presence = OptInUnloadHeaderPresence(rfh);
    switch (presence) {
      case HeaderPresence::kNotPresent:
        result.No(BackForwardCacheMetrics::NotRestoredReason::
                      kOptInUnloadHeaderNotPresent);
        break;
      case HeaderPresence::kPresent:
        // The opt-in header is present, so the page is eligible for BFCache.
        break;
      case HeaderPresence::kUnsure:
        // For the cases which we didn't parse the opt-in header, we should have
        // already bailed out of BFCache for other reasons.
        DCHECK(!result.CanStore());
        break;
    }
  }

  CanStoreRenderFrameHostLater(&result, rfh);

  DVLOG(1) << "CanPotentiallyStorePageLater: " << rfh->GetLastCommittedURL()
           << " : " << result.ToString();
  return result;
}

// Recursively checks whether this RenderFrameHost and all child frames
// can be cached later.
void BackForwardCacheImpl::CanStoreRenderFrameHostLater(
    BackForwardCacheCanStoreDocumentResult* result,
    RenderFrameHostImpl* rfh) {
  // If the rfh has ever granted media access, prevent it from entering cache.
  // TODO(crbug.com/989379): Consider only blocking when there's an active
  //                         media stream.
  if (rfh->was_granted_media_access()) {
    result->No(
        BackForwardCacheMetrics::NotRestoredReason::kWasGrantedMediaAccess);
  }

  if (rfh->IsBackForwardCacheDisabled() && !ShouldIgnoreBlocklists()) {
    result->NoDueToDisableForRenderFrameHostCalled(
        rfh->back_forward_cache_disabled_reasons());
  }

  // Do not store documents if they have inner WebContents.
  if (rfh->IsOuterDelegateFrame())
    result->No(BackForwardCacheMetrics::NotRestoredReason::kHaveInnerContents);

  const bool has_unload_handler = rfh->GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType::kUnloadHandler);
  switch (unload_strategy_) {
    case BackForwardCacheImpl::UnloadSupportStrategy::kAlways:
      break;
    case BackForwardCacheImpl::UnloadSupportStrategy::kOptInHeaderRequired:
      if (has_unload_handler) {
        HeaderPresence presence =
            OptInUnloadHeaderPresence(rfh->GetMainFrame());
        switch (presence) {
          case HeaderPresence::kNotPresent:
            result->No(rfh->GetParent()
                           ? BackForwardCacheMetrics::NotRestoredReason::
                                 kUnloadHandlerExistsInSubFrame
                           : BackForwardCacheMetrics::NotRestoredReason::
                                 kOptInUnloadHeaderNotPresent);
            break;
          case HeaderPresence::kPresent:
            // The opt-in header is present for the main frame with an unload
            // handler, so the page is eligible for BFCache.
            break;
          case HeaderPresence::kUnsure:
            // For the cases which we didn't parse the opt-in header, we should
            // have already bailed out of BFCache for other reasons.
            DCHECK(!result->CanStore());
            break;
        }
      }
      break;
    case BackForwardCacheImpl::UnloadSupportStrategy::kNo:
      if (has_unload_handler) {
        result->No(rfh->GetParent()
                       ? BackForwardCacheMetrics::NotRestoredReason::
                             kUnloadHandlerExistsInSubFrame
                       : BackForwardCacheMetrics::NotRestoredReason::
                             kUnloadHandlerExistsInMainFrame);
      }
      break;
  }

  // When it's not the final decision for putting a page in the back-forward
  // cache, we should only consider "sticky" features here - features that
  // will always result in a page becoming ineligible for back-forward cache
  // since the first time it's used.
  if (uint64_t banned_features =
          GetDisallowedFeatures(rfh, RequestedFeatures::kOnlySticky) &
          rfh->scheduler_tracked_features()) {
    if (!ShouldIgnoreBlocklists()) {
      result->NoDueToFeatures(banned_features);
    }
  }

  for (size_t i = 0; i < rfh->child_count(); i++)
    CanStoreRenderFrameHostLater(result,
                                 rfh->child_at(i)->current_frame_host());
}

// Recursively checks dynamic states that might affect whether this
// RenderFrameHost and all child frames can be cached right now.
void BackForwardCacheImpl::CheckDynamicBlocklistedFeaturesOnSubtree(
    BackForwardCacheCanStoreDocumentResult* result,
    RenderFrameHostImpl* rfh) {
  if (!rfh->IsDOMContentLoaded())
    result->No(BackForwardCacheMetrics::NotRestoredReason::kLoading);

  // Check for banned features currently being used. Note that unlike the check
  // in CanStoreRenderFrameHostLater, we are checking all banned features here
  // (not only the "sticky" features), because this time we're making a decision
  // on whether we should store a page in the back-forward cache or not.
  if (uint64_t banned_features =
          GetDisallowedFeatures(rfh, RequestedFeatures::kAll) &
          rfh->scheduler_tracked_features()) {
    bool should_ignore_features_for_now =
        CheckFeatureUsageOnlyAfterAck() &&
        !rfh->render_view_host()->DidReceiveBackForwardCacheAck();
    if (!ShouldIgnoreBlocklists() && !should_ignore_features_for_now) {
      result->NoDueToFeatures(banned_features);
    }
  }

  bool has_navigation_request = rfh->frame_tree_node()->navigation_request() ||
                                rfh->HasPendingCommitNavigation();
  // Do not cache if we have navigations in any of the subframes.
  if (rfh->GetParent() && has_navigation_request) {
    result->No(
        BackForwardCacheMetrics::NotRestoredReason::kSubframeIsNavigating);
  }

  for (size_t i = 0; i < rfh->child_count(); i++)
    CheckDynamicBlocklistedFeaturesOnSubtree(
        result, rfh->child_at(i)->current_frame_host());
}

void BackForwardCacheImpl::StoreEntry(
    std::unique_ptr<BackForwardCacheImpl::Entry> entry) {
  TRACE_EVENT("navigation", "BackForwardCache::StoreEntry", "entry", entry);
  DCHECK(CanStorePageNow(entry->render_frame_host.get()));

#if defined(OS_ANDROID)
  if (!IsProcessBindingEnabled()) {
    // Set the priority of the main frame on entering the back-forward cache to
    // make sure the page gets evicted instead of foreground tab. This might not
    // become the effective priority of the process if it owns other higher
    // priority RenderWidgetHost. We don't need to reset the priority in
    // RestoreEntry as it is taken care by WebContentsImpl::NotifyFrameSwapped
    // on restoration.
    RenderWidgetHostImpl* rwh = entry->render_frame_host->GetRenderWidgetHost();
    ChildProcessImportance current_importance = rwh->importance();
    rwh->SetImportance(
        std::min(current_importance, kChildProcessImportanceParam.Get()));
  }
#endif

  entry->render_frame_host->DidEnterBackForwardCache();
  entries_.push_front(std::move(entry));
  AddProcessesForEntry(*entries_.front());
  EnforceCacheSizeLimit();
}

void BackForwardCacheImpl::EnforceCacheSizeLimit() {
  if (!IsBackForwardCacheEnabled())
    return;

  if (UsingForegroundBackgroundCacheSizeLimit()) {
    // First enforce the foregrounded limit. The idea is that we need to
    // strictly enforce the limit on pages using foregrounded processes because
    // Android will not kill a foregrounded process, however it will kill a
    // backgrounded process if there is memory pressue, so we can allow more of
    // those to be kept in the cache.
    EnforceCacheSizeLimitInternal(GetForegroundedEntriesCacheSize(),
                                  /*foregrounded_only=*/true);
  }
  EnforceCacheSizeLimitInternal(GetCacheSize(),
                                /*foregrounded_only=*/false);
}

size_t BackForwardCacheImpl::EnforceCacheSizeLimitInternal(
    size_t limit,
    bool foregrounded_only) {
  size_t count = 0;
  size_t not_received_ack_count = 0;
  for (auto& stored_entry : entries_) {
    if (stored_entry->render_frame_host->is_evicted_from_back_forward_cache())
      continue;
    if (foregrounded_only && !HasForegroundedProcess(*stored_entry))
      continue;
    if (!AllRenderViewHostsReceivedAckFromRenderer(*stored_entry)) {
      not_received_ack_count++;
      continue;
    }
    if (++count > limit) {
      stored_entry->render_frame_host->EvictFromBackForwardCacheWithReason(
          foregrounded_only
              ? BackForwardCacheMetrics::NotRestoredReason::
                    kForegroundCacheLimit
              : BackForwardCacheMetrics::NotRestoredReason::kCacheLimit);
    }
  }
  UMA_HISTOGRAM_COUNTS_100(
      "BackForwardCache.AllSites.HistoryNavigationOutcome."
      "CountEntriesWithoutRendererAck",
      not_received_ack_count);
  return count;
}

std::unique_ptr<BackForwardCacheImpl::Entry> BackForwardCacheImpl::RestoreEntry(
    int navigation_entry_id,
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  TRACE_EVENT0("navigation", "BackForwardCache::RestoreEntry");
  // Select the RenderFrameHostImpl matching the navigation entry.
  auto matching_entry = std::find_if(
      entries_.begin(), entries_.end(),
      [navigation_entry_id](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host->nav_entry_id() == navigation_entry_id;
      });

  // Not found.
  if (matching_entry == entries_.end())
    return nullptr;

  // Don't restore an evicted frame.
  if ((*matching_entry)
          ->render_frame_host->is_evicted_from_back_forward_cache())
    return nullptr;

  std::unique_ptr<Entry> entry = std::move(*matching_entry);
  TRACE_EVENT_INSTANT("navigation",
                      "BackForwardCache::RestoreEntry_matched_entry", "entry",
                      entry);

  entries_.erase(matching_entry);
  RemoveProcessesForEntry(*entry);
  entry->page_restore_params = std::move(page_restore_params);
  RequestRecordTimeToVisible(entry->render_frame_host.get(),
                             entry->page_restore_params->navigation_start);
  entry->render_frame_host->WillLeaveBackForwardCache();

  RestoreBrowserControlsState(entry->render_frame_host.get());

  return entry;
}

void BackForwardCacheImpl::Flush() {
  TRACE_EVENT0("navigation", "BackForwardCache::Flush");
  for (std::unique_ptr<Entry>& entry : entries_) {
    entry->render_frame_host->EvictFromBackForwardCacheWithReason(
        BackForwardCacheMetrics::NotRestoredReason::kCacheFlushed);
  }
}

void BackForwardCacheImpl::Shutdown() {
  if (UsingForegroundBackgroundCacheSizeLimit()) {
    for (auto& entry : entries_)
      RemoveProcessesForEntry(*entry.get());
  }
  entries_.clear();
}

void BackForwardCacheImpl::EvictFramesInRelatedSiteInstances(
    SiteInstance* site_instance) {
  for (std::unique_ptr<Entry>& entry : entries_) {
    if (entry->render_frame_host->GetSiteInstance()->IsRelatedSiteInstance(
            site_instance)) {
      entry->render_frame_host->EvictFromBackForwardCacheWithReason(
          BackForwardCacheMetrics::NotRestoredReason::
              kConflictingBrowsingInstance);
    }
  }
}

void BackForwardCacheImpl::PostTaskToDestroyEvictedFrames() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BackForwardCacheImpl::DestroyEvictedFrames,
                                weak_factory_.GetWeakPtr()));
}

// static
bool BackForwardCache::IsBackForwardCacheFeatureEnabled() {
  return IsBackForwardCacheEnabled();
}

// static
void BackForwardCache::DisableForRenderFrameHost(
    RenderFrameHost* render_frame_host,
    BackForwardCache::DisabledReason reason) {
  DisableForRenderFrameHost(static_cast<RenderFrameHostImpl*>(render_frame_host)
                                ->GetGlobalFrameRoutingId(),
                            reason);
}

// static
void BackForwardCache::DisableForRenderFrameHost(
    GlobalFrameRoutingId id,
    BackForwardCache::DisabledReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_bfcache_disabled_test_observer)
    g_bfcache_disabled_test_observer->OnDisabledForFrameWithReason(id, reason);

  if (auto* rfh = RenderFrameHostImpl::FromID(id))
    rfh->DisableBackForwardCache(reason);
}

void BackForwardCacheImpl::DisableForTesting(DisableForTestingReason reason) {
  is_disabled_for_testing_ = true;

  // Flush all the entries to make sure there are no entries in the cache after
  // DisableForTesting() is called.
  Flush();
}

const std::list<std::unique_ptr<BackForwardCacheImpl::Entry>>&
BackForwardCacheImpl::GetEntries() {
  return entries_;
}

BackForwardCacheImpl::Entry* BackForwardCacheImpl::GetEntry(
    int navigation_entry_id) {
  auto matching_entry = std::find_if(
      entries_.begin(), entries_.end(),
      [navigation_entry_id](std::unique_ptr<Entry>& entry) {
        return entry->render_frame_host->nav_entry_id() == navigation_entry_id;
      });

  if (matching_entry == entries_.end())
    return nullptr;

  // Don't return the frame if it is evicted.
  if ((*matching_entry)
          ->render_frame_host->is_evicted_from_back_forward_cache())
    return nullptr;

  return (*matching_entry).get();
}

void BackForwardCacheImpl::AddProcessesForEntry(Entry& entry) {
  if (!UsingForegroundBackgroundCacheSizeLimit())
    return;
  for (auto* rvh : entry.render_view_hosts) {
    RenderProcessHostImpl* process =
        static_cast<RenderProcessHostImpl*>(rvh->GetProcess());
    if (observed_processes_.find(process) == observed_processes_.end())
      process->AddInternalObserver(this);
    observed_processes_.insert(process);
  }
}

void BackForwardCacheImpl::RemoveProcessesForEntry(Entry& entry) {
  if (!UsingForegroundBackgroundCacheSizeLimit())
    return;
  for (auto* rvh : entry.render_view_hosts) {
    RenderProcessHostImpl* process =
        static_cast<RenderProcessHostImpl*>(rvh->GetProcess());
    // Remove 1 instance of this process from the multiset.
    observed_processes_.erase(observed_processes_.find(process));
    if (observed_processes_.find(process) == observed_processes_.end())
      process->RemoveInternalObserver(this);
  }
}

void BackForwardCacheImpl::DestroyEvictedFrames() {
  TRACE_EVENT0("navigation", "BackForwardCache::DestroyEvictedFrames");
  if (entries_.empty())
    return;

  base::EraseIf(entries_, [this](std::unique_ptr<Entry>& entry) {
    if (entry->render_frame_host->is_evicted_from_back_forward_cache()) {
      RemoveProcessesForEntry(*entry);
      return true;
    }
    return false;
  });
}

bool BackForwardCacheImpl::IsAllowed(const GURL& current_url) {
  return IsHostPathAllowed(current_url) && IsQueryAllowed(current_url);
}

bool BackForwardCacheImpl::IsHostPathAllowed(const GURL& current_url) {
  // If the current_url matches the blocked host and path, current_url is
  // not allowed to be cached.
  const auto& it = blocked_urls_.find(current_url.host());
  if (it != blocked_urls_.end()) {
    for (const std::string& blocked_path : it->second) {
      if (base::StartsWith(current_url.path_piece(), blocked_path))
        return false;
    }
  }

  // By convention, when |allowed_urls_| is empty, it means there are no
  // restrictions about what RenderFrameHost can enter the BackForwardCache.
  if (allowed_urls_.empty())
    return true;

  // Checking for each url in the |allowed_urls_|, if the current_url matches
  // the corresponding host and path is the prefix of the allowed url path. We
  // only check for host and path and not any other components including url
  // scheme here.
  const auto& entry = allowed_urls_.find(current_url.host());
  if (entry != allowed_urls_.end()) {
    for (const std::string& allowed_path : entry->second) {
      if (base::StartsWith(current_url.path_piece(), allowed_path))
        return true;
    }
  }
  return false;
}

bool BackForwardCacheImpl::IsQueryAllowed(const GURL& current_url) {
  std::vector<std::string> cgi_params =
      base::SplitString(current_url.query_piece(), "&", base::TRIM_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  for (const std::string& cgi_param : cgi_params) {
    if (base::Contains(blocked_cgi_params_, cgi_param))
      return false;
  }
  return true;
}

bool BackForwardCacheImpl::CheckFeatureUsageOnlyAfterAck() {
  if (!IsBackForwardCacheEnabled())
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      features::kBackForwardCache, "check_eligibility_after_pagehide", false);
}

bool BackForwardCacheImpl::IsMediaSessionImplOnServiceCreatedAllowed() {
  return (SupportedFeaturesBitmask() &
          FeatureToBit(
              WebSchedulerTrackedFeature::kMediaSessionImplOnServiceCreated)) !=
         0;
}

void BackForwardCacheImpl::WillCommitNavigationToCachedEntry(
    Entry& bfcache_entry,
    base::OnceClosure done_callback) {
  // Disable JS eviction in renderers and defer the navigation commit until
  // we've received confirmation that eviction is disabled from renderers.
  auto cb = base::BarrierClosure(
      bfcache_entry.render_view_hosts.size(),
      base::BindOnce(
          [](base::TimeTicks ipc_start_time, base::OnceClosure cb) {
            std::move(cb).Run();
            base::UmaHistogramTimes(
                "BackForwardCache.Restore.DisableEvictionDelay",
                base::TimeTicks::Now() - ipc_start_time);
          },
          base::TimeTicks::Now(), std::move(done_callback)));

  for (auto* rvh : bfcache_entry.render_view_hosts) {
    rvh->PrepareToLeaveBackForwardCache(cb);
  }
}

bool BackForwardCache::DisabledReason::operator<(
    const DisabledReason& other) const {
  return std::tie(source, id) < std::tie(other.source, other.id);
}
bool BackForwardCache::DisabledReason::operator==(
    const DisabledReason& other) const {
  return std::tie(source, id) == std::tie(other.source, other.id);
}
bool BackForwardCache::DisabledReason::operator!=(
    const DisabledReason& other) const {
  return !(*this == other);
}

}  // namespace content
