// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/cookies/canonical_cookie.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

namespace blink {
struct MobileFriendliness;
}  // namespace blink

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace page_load_metrics {

struct MemoryUpdate;
class PageLoadMetricsEmbedderInterface;
class PageLoadMetricsMemoryTracker;
class PageLoadTracker;

// MetricsWebContentsObserver tracks page loads and loading metrics
// related data based on IPC messages received from a
// MetricsRenderFrameObserver.
class MetricsWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MetricsWebContentsObserver>,
      public content::RenderWidgetHost::InputEventObserver,
      public base::SupportsWeakPtr<MetricsWebContentsObserver>,
      public mojom::PageLoadMetrics {
 public:
  // TestingObserver allows tests to observe MetricsWebContentsObserver state
  // changes. Tests may use TestingObserver to wait until certain state changes,
  // such as the arrivial of PageLoadTiming messages from the render process,
  // have been observed.
  class TestingObserver {
   public:
    explicit TestingObserver(content::WebContents* web_contents);

    TestingObserver(const TestingObserver&) = delete;
    TestingObserver& operator=(const TestingObserver&) = delete;

    virtual ~TestingObserver();

    void OnGoingAway();

    // Some PageLoadTiming messages will race with the navigation
    // commit. OnTrackerCreated() allows tests to manipulate the tracker very
    // early (eg, to add observers) to handle those cases.
    virtual void OnTrackerCreated(PageLoadTracker* tracker) {}

    // In cases where LoadTimingInfo is not needed, waiting until commit is
    // fine.
    virtual void OnCommit(PageLoadTracker* tracker) {}

    // This is called both for prerender activation and restoration from
    // the back/forward cache.
    virtual void OnActivate(PageLoadTracker* tracker) {}

    // Returns the observer delegate for the committed load associated with
    // the MetricsWebContentsObserver, or null if the observer has gone away
    // (via MetricsWebContentsObserver::WebContentsDestroyed).
    const PageLoadMetricsObserverDelegate* GetDelegateForCommittedLoad();

   private:
    raw_ptr<page_load_metrics::MetricsWebContentsObserver> observer_;
  };

  // Record a set of WebFeatures directly from the browser process. This
  // should only be used for features that were detected browser-side; features
  // sources from the renderer should go via MetricsRenderFrameObserver.
  static void RecordFeatureUsage(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::WebFeature>& features);
  static void RecordFeatureUsage(content::RenderFrameHost* render_frame_host,
                                 blink::mojom::WebFeature feature);

  // Note that the returned metrics is owned by the web contents.
  static MetricsWebContentsObserver* CreateForWebContents(
      content::WebContents* web_contents,
      std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);

  MetricsWebContentsObserver(const MetricsWebContentsObserver&) = delete;
  MetricsWebContentsObserver& operator=(const MetricsWebContentsObserver&) =
      delete;

  ~MetricsWebContentsObserver() override;

  // Binds a Mojo receiver to the instance associated with the RenderFrameHost.
  static void BindPageLoadMetrics(
      mojo::PendingAssociatedReceiver<mojom::PageLoadMetrics> receiver,
      content::RenderFrameHost* rfh);

  // Any visibility changes that occur after this method should be ignored since
  // they are just clean up prior to destroying the WebContents instance.
  void WebContentsWillSoonBeDestroyed();

  // content::WebContentsObserver implementation:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationStopped() override;
  void OnInputEvent(const blink::WebInputEvent& event) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void FrameDeleted(int frame_tree_node_id) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id) override;
  void WebContentsDestroyed() override;
  void ResourceLoadComplete(
      content::RenderFrameHost* render_frame_host,
      const content::GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;
  void FrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host) override;
  void FrameDisplayStateChanged(content::RenderFrameHost* render_frame_host,
                                bool is_display_none) override;
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::RenderFrameHost* rfh,
                         const content::CookieAccessDetails& details) override;
  void OnStorageAccessed(content::RenderFrameHost* rfh,
                         const GURL& url,
                         const GURL& first_party_url,
                         bool blocked_by_policy,
                         StorageType storage_type);
  void DidActivatePortal(content::WebContents* predecessor_web_contents,
                         base::TimeTicks activation_time) override;

  // These methods are forwarded from the MetricsNavigationThrottle.
  void WillStartNavigationRequest(content::NavigationHandle* navigation_handle);
  void WillProcessNavigationResponse(
      content::NavigationHandle* navigation_handle);

  // Flush any buffered metrics, as part of the metrics subsystem persisting
  // metrics as the application goes into the background. The application may be
  // killed at any time after this method is invoked without further
  // notification.
  void FlushMetricsOnAppEnterBackground();

  // Returns the delegate for the current committed load, required for testing.
  const PageLoadMetricsObserverDelegate& GetDelegateForCommittedLoad();

  // Register / unregister TestingObservers. Should only be called from tests.
  void AddTestingObserver(TestingObserver* observer);
  void RemoveTestingObserver(TestingObserver* observer);

  // public only for testing
  void OnTimingUpdated(
      content::RenderFrameHost* render_frame_host,
      mojom::PageLoadTimingPtr timing,
      mojom::FrameMetadataPtr metadata,
      const std::vector<blink::UseCounterFeature>& new_features,
      const std::vector<mojom::ResourceDataUpdatePtr>& resources,
      mojom::FrameRenderDataUpdatePtr render_data,
      mojom::CpuTimingPtr cpu_timing,
      mojom::DeferredResourceCountsPtr new_deferred_resource_data,
      mojom::InputTimingPtr input_timing_delta,
      const absl::optional<blink::MobileFriendliness>& mobile_friendliness);

  // Informs the observers of the currently committed primary page load that
  // it's likely that prefetch will occur in this WebContents. This should
  // not be called within WebContentsObserver::DidFinishNavigation methods.
  void OnPrefetchLikely();

  // Called when V8 per-frame memory usage updates are available. Virtual for
  // test classes to override.
  virtual void OnV8MemoryChanged(
      const std::vector<MemoryUpdate>& memory_updates);

 protected:
  // Protected rather than private so that derived test classes can call
  // constructor.
  MetricsWebContentsObserver(
      content::WebContents* web_contents,
      std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface);

 private:
  friend class content::WebContentsUserData<MetricsWebContentsObserver>;

  // Gets the PageLoadTracker associated with |rfh| if it exists, or nullptr
  // otherwise.
  PageLoadTracker* GetPageLoadTracker(content::RenderFrameHost* rfh);

  // Gets the memory tracker for the BrowserContext if it exists, or nullptr
  // otherwise. The tracker measures per-frame memory usage by V8.
  PageLoadMetricsMemoryTracker* GetMemoryTracker() const;

  void WillStartNavigationRequestImpl(
      content::NavigationHandle* navigation_handle);

  // page_load_metrics::mojom::PageLoadMetrics implementation.
  void UpdateTiming(mojom::PageLoadTimingPtr timing,
                    mojom::FrameMetadataPtr metadata,
                    const std::vector<blink::UseCounterFeature>& new_features,
                    std::vector<mojom::ResourceDataUpdatePtr> resources,
                    mojom::FrameRenderDataUpdatePtr render_data,
                    mojom::CpuTimingPtr cpu_timing,
                    mojom::DeferredResourceCountsPtr new_deferred_resource_data,
                    mojom::InputTimingPtr input_timing,
                    const absl::optional<blink::MobileFriendliness>&
                        mobile_friendliness) override;

  void SetUpSharedMemoryForSmoothness(
      base::ReadOnlySharedMemoryRegion shared_memory) override;

  // Common part for UpdateThroughput and OnTimingUpdated.
  bool DoesTimingUpdateHaveError(PageLoadTracker* tracker);

  void HandleFailedNavigationForTrackedLoad(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<PageLoadTracker> tracker);

  void HandleCommittedNavigationForTrackedLoad(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<PageLoadTracker> tracker);

  void HandleCommittedNavigationForPrerendering(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<PageLoadTracker> tracker);

  void FinalizeCurrentlyCommittedLoad(
      content::NavigationHandle* newly_committed_navigation,
      PageLoadTracker* newly_committed_navigation_tracker);

  // Return a PageLoadTracker (either provisional or committed) that matches the
  // given request attributes, or nullptr if there are no matching
  // PageLoadTrackers.
  PageLoadTracker* GetTrackerOrNullForRequest(
      const content::GlobalRequestID& request_id,
      content::RenderFrameHost* render_frame_host_or_null,
      network::mojom::RequestDestination request_destination,
      base::TimeTicks creation_time);

  // Notify all loads, provisional and committed, that we performed an action
  // that might abort them.
  void NotifyPageEndAllLoads(PageEndReason page_end_reason,
                             UserInitiatedInfo user_initiated_info);
  void NotifyPageEndAllLoadsWithTimestamp(PageEndReason page_end_reason,
                                          UserInitiatedInfo user_initiated_info,
                                          base::TimeTicks timestamp,
                                          bool is_certainly_browser_timestamp);

  // Register / Unregister input event callback to given RenderViewHost
  void RegisterInputEventObserver(content::RenderViewHost* host);
  void UnregisterInputEventObserver(content::RenderViewHost* host);

  // Notify aborted provisional loads that a new navigation occurred. This is
  // used for more consistent attribution tracking for aborted provisional
  // loads. This method returns the provisional load that was likely aborted
  // by this navigation, to help instantiate the new PageLoadTracker.
  std::unique_ptr<PageLoadTracker> NotifyAbortedProvisionalLoadsNewNavigation(
      content::NavigationHandle* new_navigation,
      UserInitiatedInfo user_initiated_info);

  // Whether metrics should be tracked, and a PageLoadTracker should be created,
  // for the given main frame navigation.
  bool ShouldTrackMainFrameNavigation(
      content::NavigationHandle* navigation_handle) const;

  void OnBrowserFeatureUsage(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::UseCounterFeature>& new_features);

  // Before deleting PageLoadTracker, check if we need to keep it alive as the
  // page is stored in back-forward cache. The page can either be restored later
  // (we will be notified via DidFinishNavigation and NavigationHandle::
  // IsServedFromBackForwardCache) or will be evicted from the cache (we will be
  // notified via RenderFrameDeleted).
  void MaybeStorePageLoadTrackerForBackForwardCache(
      content::NavigationHandle* next_navigation_handle,
      std::unique_ptr<PageLoadTracker> page_load_tracker);

  // Tries to move a PageLoadTracker from |inactive_pages_| to
  // |committed_load_|, when a navigation activates a back/forward-cached or
  // prerendered page. Returns true if |committed_load_| is updated.
  bool MaybeActivatePageLoadTracker(
      content::NavigationHandle* navigation_handle);

  // Notify |tracker| about cookie read or write.
  void OnCookiesAccessedImpl(PageLoadTracker& tracker,
                             const content::CookieAccessDetails& details);

  // True if the web contents is currently in the foreground.
  bool in_foreground_;

  // The PageLoadTrackers must be deleted before the |embedder_interface_|,
  // because they hold a pointer to the |embedder_interface_|.
  std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface_;

  // This map tracks all of the navigations ongoing that are not committed
  // yet. Once a navigation is committed, it moves from the map to
  // |committed_load_| or |inactive_pages_|. Note that a PageLoadTrackers
  // NavigationHandle is only valid until commit time, when we remove it from
  // the map.
  std::map<content::NavigationHandle*, std::unique_ptr<PageLoadTracker>>
      provisional_loads_;

  // Tracks aborted provisional loads for a little bit longer than usual (one
  // more navigation commit at the max), in order to better understand how the
  // navigation failed. This is because most provisional loads are destroyed
  // and vanish before we get signal about what caused the abort (new
  // navigation, stop button, etc.).
  std::vector<std::unique_ptr<PageLoadTracker>> aborted_provisional_loads_;

  // Direct usage of this is discouraged. Use GetPageLoadTracker() instead.
  std::unique_ptr<PageLoadTracker> committed_load_;

  // Memory updates that are accumulated while there is no PageLoadTracker
  // associated with RenderFrameHost. Will be sent in
  // HandleCommittedNavigationForTrackedLoad, unless the RenderFrameHost is
  // deleted and/or web contents is destroyed.
  std::vector<MemoryUpdate> queued_memory_updates_;

  // This is currently set only for the main frame of each page associated with
  // the WebContents.
  base::flat_map<content::RenderFrameHost*, base::ReadOnlySharedMemoryRegion>
      ukm_smoothness_data_;

  // This stores the PageLoadTracker for each main frame of inactive pages,
  // including pages in the back/forward cache and prerendered pages. (The main
  // frame of the active page is in |committed_load_|.)
  base::flat_map<content::RenderFrameHost*, std::unique_ptr<PageLoadTracker>>
      inactive_pages_;

  // Has the MWCO observed at least one navigation?
  bool has_navigated_;

  base::ObserverList<TestingObserver>::Unchecked testing_observers_;
  content::RenderFrameHostReceiverSet<mojom::PageLoadMetrics>
      page_load_metrics_receivers_;

  bool web_contents_will_soon_be_destroyed_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_METRICS_WEB_CONTENTS_OBSERVER_H_
