// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_
#define CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_

#include <bitset>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace url {
class Origin;
}

namespace content {
class BackForwardCacheCanStoreDocumentResult;
class BackForwardCacheCanStoreTreeResult;
class NavigationEntryImpl;
class NavigationRequest;
class RenderFrameHostImpl;

// Helper class for recording metrics around history navigations.
// Associated with a main frame document and shared between all
// NavigationEntries with the same document_sequence_number for the main
// document.
//
// TODO(altimin, crbug.com/933147): Remove this class after we are done
// with implementing back-forward cache.
class BackForwardCacheMetrics
    : public base::RefCounted<BackForwardCacheMetrics> {
 public:
  // Please keep in sync with BackForwardCacheNotRestoredReason in
  // tools/metrics/histograms/enums.xml. These values should not be renumbered.
  enum class NotRestoredReason : uint8_t {
    kMinValue = 0,
    kNotMainFrame = 0,
    // BackForwardCache is disabled due to low memory device, base::Feature or
    // command line. Note that the more specific NotRestoredReasons
    // kBackForwardCacheDisabledByLowMemory and
    // kBackForwardCacheDisabledByCommandLine will also be set as other reasons
    // along with this when appropriate.
    kBackForwardCacheDisabled = 1,
    kRelatedActiveContentsExist = 2,
    kHTTPStatusNotOK = 3,
    kSchemeNotHTTPOrHTTPS = 4,
    kLoading = 5,
    kWasGrantedMediaAccess = 6,
    kBlocklistedFeatures = 7,
    kDisableForRenderFrameHostCalled = 8,
    kDomainNotAllowed = 9,
    kHTTPMethodNotGET = 10,
    kSubframeIsNavigating = 11,
    kTimeout = 12,
    kCacheLimit = 13,
    kJavaScriptExecution = 14,
    kRendererProcessKilled = 15,
    kRendererProcessCrashed = 16,
    // 17: Dialogs are no longer a reason to exclude from BackForwardCache
    kGrantedMediaStreamAccess = 18,
    kSchedulerTrackedFeatureUsed = 19,
    kConflictingBrowsingInstance = 20,
    kCacheFlushed = 21,
    kServiceWorkerVersionActivation = 22,
    kSessionRestored = 23,
    kUnknown = 24,
    kServiceWorkerPostMessage = 25,
    kEnteredBackForwardCacheBeforeServiceWorkerHostAdded = 26,
    // 27: kRenderFrameHostReused_SameSite was removed.
    // 28: kRenderFrameHostReused_CrossSite was removed.
    kNotMostRecentNavigationEntry = 29,
    kServiceWorkerClaim = 30,
    kIgnoreEventAndEvict = 31,
    kHaveInnerContents = 32,
    kTimeoutPuttingInCache = 33,
    // BackForwardCache is disabled due to low memory device.
    kBackForwardCacheDisabledByLowMemory = 34,
    // BackForwardCache is disabled due to command-line switch (may include
    // cases where the embedder disabled it due to, e.g., enterprise policy).
    kBackForwardCacheDisabledByCommandLine = 35,
    // 36: kFrameTreeNodeStateReset was removed.
    // 37: kNetworkRequestDatapipeDrained = 37 was removed and broken into 43
    // and 44.
    kNetworkRequestRedirected = 38,
    kNetworkRequestTimeout = 39,
    kNetworkExceedsBufferLimit = 40,
    kNavigationCancelledWhileRestoring = 41,
    kBackForwardCacheDisabledForPrerender = 42,
    kUserAgentOverrideDiffers = 43,
    // 44: kNetworkRequestDatapipeDrainedAsDatapipe was removed now that
    // ScriptStreamer is supported.
    kNetworkRequestDatapipeDrainedAsBytesConsumer = 45,
    kForegroundCacheLimit = 46,
    kBrowsingInstanceNotSwapped = 47,
    kBackForwardCacheDisabledForDelegate = 48,
    kOptInUnloadHeaderNotPresent = 49,
    kUnloadHandlerExistsInMainFrame = 50,
    kUnloadHandlerExistsInSubFrame = 51,
    kServiceWorkerUnregistration = 52,
    kCacheControlNoStore = 53,
    kCacheControlNoStoreCookieModified = 54,
    kCacheControlNoStoreHTTPOnlyCookieModified = 55,
    kNoResponseHead = 56,
    kActivationNavigationsDisallowedForBug1234857 = 57,
    kMaxValue = kActivationNavigationsDisallowedForBug1234857,
  };

  using NotRestoredReasons =
      std::bitset<static_cast<size_t>(NotRestoredReason::kMaxValue) + 1ul>;

  // Please keep in sync with BackForwardCacheHistoryNavigationOutcome in
  // tools/metrics/histograms/enums.xml. These values should not be renumbered.
  enum class HistoryNavigationOutcome {
    kRestored = 0,
    kNotRestored = 1,
    kMaxValue = kNotRestored,
  };

  // Please keep in sync with BackForwardCacheEvictedAfterDocumentRestoredReason
  // in tools/metrics/histograms/enums.xml. These values should not be
  // renumbered.
  enum class EvictedAfterDocumentRestoredReason {
    kRestored = 0,
    kByJavaScript = 1,
    kMaxValue = kByJavaScript,
  };

  // Please keep in sync with BackForwardCacheReloadsAndHistoryNavigations
  // in tools/metrics/histograms/enums.xml. These values should not be
  // renumbered.
  enum class ReloadsAndHistoryNavigations {
    kHistoryNavigation = 0,
    kReloadAfterHistoryNavigation = 1,
    kMaxValue = kReloadAfterHistoryNavigation,
  };

  // Please keep in sync with BackForwardCacheReloadsAfterHistoryNavigation
  // in tools/metrics/histograms/enums.xml. These values should not be
  // renumbered.
  enum class ReloadsAfterHistoryNavigation {
    kNotServedFromBackForwardCache = 0,
    kServedFromBackForwardCache = 1,
    kMaxValue = kServedFromBackForwardCache,
  };

  // Creates a potential new metrics object for the navigation.
  // Note that this object will not be used if the entry we are navigating to
  // already has the BackForwardCacheMetrics object (which happens for history
  // navigations).
  //
  // |document_sequence_number| is the sequence number of the document
  // associated with the document associated with the navigating frame and it is
  // ignored if the navigating frame is not a main one.
  static scoped_refptr<BackForwardCacheMetrics>
  CreateOrReuseBackForwardCacheMetrics(
      NavigationEntryImpl* currently_committed_entry,
      bool is_main_frame_navigation,
      int64_t document_sequence_number);

  BackForwardCacheMetrics(const BackForwardCacheMetrics&) = delete;
  BackForwardCacheMetrics& operator=(const BackForwardCacheMetrics&) = delete;

  // Records when the page is evicted after the document is restored e.g. when
  // the race condition by JavaScript happens.
  static void RecordEvictedAfterDocumentRestored(
      EvictedAfterDocumentRestoredReason reason);

  // Sets the reason why the browsing instance is not swapped. Passing
  // absl::nullopt resets the reason.
  void SetBrowsingInstanceSwapResult(
      absl::optional<ShouldSwapBrowsingInstance> reason);

  absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result()
      const {
    return browsing_instance_swap_result_;
  }

  // Notifies that the main frame has started a navigation to an entry
  // associated with |this|.
  //
  // This is the point in time that a back-forward cache hit could be shown to
  // the user.
  //
  // Note that in some cases (synchronous renderer-initiated navigations
  // which create navigation entry only when committed) this call might
  // be missing, but they should not matter for bfcache.
  void MainFrameDidStartNavigationToDocument();

  // Notifies that an associated entry has committed a navigation.
  // |back_forward_cache_allowed| indicates whether back-forward cache is
  // allowed for the URL of |navigation_request|.
  void DidCommitNavigation(NavigationRequest* navigation_request,
                           bool back_forward_cache_allowed);

  // Records when another navigation commits away from the most recent entry
  // associated with |this|.  This is the point in time that the previous
  // document could enter the back-forward cache.
  // |new_main_document| points to the newly committed RFH, which might or might
  // not be the same as the RFH for the old document.
  void MainFrameDidNavigateAwayFromDocument(
      RenderFrameHostImpl* new_main_document,
      NavigationRequest* navigation);

  // Snapshots the state of the features active on the page before closing it.
  // It should be called at the same time when the document might have been
  // placed in the back-forward cache.
  void RecordFeatureUsage(RenderFrameHostImpl* main_frame);

  // Marks when the page is not cached, or evicted. This information is useful
  // e.g., to prioritize the tasks to improve cache-hit rate.
  void MarkNotRestoredWithReason(
      const BackForwardCacheCanStoreDocumentResult& can_store);

  // TODO: Take BackForwardCacheCanStoreDocumentResultWithTree as an argument
  // instead of using BackForwardCacheCanStoreDocumentResult and
  // BackForwardCacheCanStoreTreeResult as arguments.
  void FinalizeNotRestoredReasons(
      const BackForwardCacheCanStoreDocumentResult& can_store_flat,
      std::unique_ptr<BackForwardCacheCanStoreTreeResult> can_store_tree);

  // Exported for testing.
  // The DisabledReason's source and id combined to give a unique uint64.
  CONTENT_EXPORT static uint64_t MetricValue(BackForwardCache::DisabledReason);

  // Injects a clock for mocking time.
  // Should be called only from the UI thread.
  CONTENT_EXPORT static void OverrideTimeForTesting(base::TickClock* clock);

 private:
  friend class base::RefCounted<BackForwardCacheMetrics>;

  explicit BackForwardCacheMetrics(int64_t document_sequence_number);

  ~BackForwardCacheMetrics();

  // Recursively collects the feature usage information from the subtree
  // of a given frame.
  void CollectFeatureUsageFromSubtree(RenderFrameHostImpl* rfh,
                                      const url::Origin& main_frame_origin);

  // Dumps the current recorded information.
  // |back_forward_cache_allowed| indicates whether back-forward cache is
  // allowed for the URL of |navigation_request|.
  void RecordMetricsForHistoryNavigationCommit(
      NavigationRequest* navigation,
      bool back_forward_cache_allowed) const;

  // Record metrics for the number of reloads after history navigation. In
  // particular we are interested in number of reloads after a restore from
  // the back-forward cache as a proxy for detecting whether the page was
  // broken or not.
  void RecordHistogramForReloadsAndHistoryNavigations(
      bool is_reload,
      bool back_forward_cache_allowed) const;

  // Record additional reason why navigation was not served from bfcache which
  // are known only at the commit time.
  void UpdateNotRestoredReasonsForNavigation(NavigationRequest* navigation);

  void RecordHistoryNavigationUkm(NavigationRequest* navigation);

  bool DidSwapBrowsingInstance() const;

  // Main frame document sequence number that identifies all NavigationEntries
  // this metrics object is associated with.
  const int64_t document_sequence_number_;

  // NavigationHandle's ID for the last main frame navigation. This is updated
  // for a main frame, not-same-document navigation.
  //
  // Should not be confused with NavigationEntryId.
  int64_t last_committed_cross_document_main_frame_navigation_id_ = -1;

  blink::scheduler::WebSchedulerTrackedFeatures main_frame_features_;
  // We record metrics for same-origin frames and cross-origin frames
  // differently as we might want to apply different policies for them,
  // especially for the things around web platform compatibility (e.g. ignore
  // unload handlers in cross-origin iframes but not in same-origin). The
  // details are still subject to metrics, however. NOTE: This is not related to
  // which process these frames are hosted in.
  blink::scheduler::WebSchedulerTrackedFeatures same_origin_frames_features_;
  blink::scheduler::WebSchedulerTrackedFeatures cross_origin_frames_features_;

  absl::optional<base::TimeTicks> started_navigation_timestamp_;
  absl::optional<base::TimeTicks> navigated_away_from_main_document_timestamp_;

  // TODO: Store BackForwardCacheCanStoreDocumentResultWithTree instead of
  // storing unique_ptr of BackForwardCacheCanStoreDocumentResult and
  // BackForwardCacheCanStoreTreeResult respectively.
  std::unique_ptr<BackForwardCacheCanStoreDocumentResult> page_store_result_;
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> page_store_tree_result_;

  // This value is updated only for navigations which are not same-document and
  // main-frame navigations.
  bool previous_navigation_is_history_ = false;
  bool previous_navigation_is_served_from_bfcache_ = false;

  absl::optional<base::TimeTicks> renderer_killed_timestamp_;

  // The reason why the last attempted navigation in the frame used or didn't
  // use a new BrowsingInstance.
  absl::optional<ShouldSwapBrowsingInstance> browsing_instance_swap_result_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BACK_FORWARD_CACHE_METRICS_H_
