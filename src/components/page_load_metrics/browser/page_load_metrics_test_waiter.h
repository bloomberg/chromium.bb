// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_WAITER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_WAITER_H_

#include <memory>
#include <unordered_set>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature_tracker.h"
#include "ui/gfx/geometry/size.h"

namespace page_load_metrics {

class PageLoadMetricsTestWaiter
    : public page_load_metrics::MetricsWebContentsObserver::TestingObserver {
 public:
  // A bitvector to express which timing fields to match on.
  enum class TimingField : int {
    kFirstPaint = 1 << 0,
    kFirstContentfulPaint = 1 << 1,
    kFirstMeaningfulPaint = 1 << 2,
    kDocumentWriteBlockReload = 1 << 3,
    kLoadEvent = 1 << 4,
    // kLoadTimingInfo waits for main frame timing info only.
    kLoadTimingInfo = 1 << 5,
    kLargestContentfulPaint = 1 << 6,
    kFirstInputOrScroll = 1 << 7,
    kFirstInputDelay = 1 << 8,
    kFirstPaintAfterBackForwardCacheRestore = 1 << 9,
    kFirstInputDelayAfterBackForwardCacheRestore = 1 << 10,
    kLayoutShift = 1 << 11,
    kRequestAnimationFrameAfterBackForwardCacheRestore = 1 << 12,
  };
  using FrameTreeNodeId =
      page_load_metrics::PageLoadMetricsObserver::FrameTreeNodeId;

  explicit PageLoadMetricsTestWaiter(content::WebContents* web_contents);

  ~PageLoadMetricsTestWaiter() override;

  // Add a page-level expectation.
  void AddPageExpectation(TimingField field);

  // Add a subframe-level expectation.
  void AddSubFrameExpectation(TimingField field);

  // Add a frame size expectation. Expects that at least one frame receives a
  // size update of |size|.
  void AddFrameSizeExpectation(const gfx::Size& size);

  // Add a main frame intersection expectation. Expects that a frame
  // receives an intersection update with a main frame intersection
  // of |rect|. Subsequent calls overwrite unmet expectations.
  void AddMainFrameIntersectionExpectation(const gfx::Rect& rect);

  // Indicates that we expect at least one main frame intersection update, with
  // any rect allowed.
  // TODO(skobes): Unify this API with AddMainFrameIntersectionExpectation.
  void SetMainFrameIntersectionExpectation();

  // Add a single WebFeature expectation.
  void AddWebFeatureExpectation(blink::mojom::WebFeature web_feature);

  // Add a single UseCounterFeature expectation.
  void AddUseCounterFeatureExpectation(const blink::UseCounterFeature& feature);

  // Wait for the subframe to navigate at least once.
  void AddSubframeNavigationExpectation();

  // Wait for the subframe to load at least one byte.
  void AddSubframeDataExpectation();

  // Add a minimum completed resource expectation.
  void AddMinimumCompleteResourcesExpectation(
      int expected_minimum_complete_resources);

  // Add aggregate received resource bytes expectation.
  void AddMinimumNetworkBytesExpectation(int expected_minimum_network_bytes);

  // Add aggregate time spent in cpu for page expectation.
  void AddMinimumAggregateCpuTimeExpectation(base::TimeDelta minimum);

  // Inserts `routing_id` into `expected_.memory_update_frame_ids_`, the set of
  // frame routing IDs expected to receive a memory measurement update.
  void AddMemoryUpdateExpectation(content::GlobalRenderFrameHostId routing_id);

  // Adds all |blink::LoadingBehaviorFlag|s set in |behavior_flags| to the
  // set of expected behaviors.
  void AddLoadingBehaviorExpectation(int behavior_flags);

  // Whether the given TimingField was observed in the page.
  bool DidObserveInPage(TimingField field) const;

  // Whether the given WebFeature was observed in the page.
  bool DidObserveWebFeature(blink::mojom::WebFeature feature) const;

  // Waits for PageLoadMetrics events that match the fields set by the add
  // expectation methods. All matching fields must be set to end this wait.
  // All expectations are reset when the wait ends.
  void Wait();

  int64_t current_network_bytes() const { return current_network_bytes_; }

  int64_t current_network_body_bytes() const {
    return current_network_body_bytes_;
  }

 protected:
  virtual bool ExpectationsSatisfied() const;

  // Intended to be overridden in tests to allow tests to wait on other resource
  // conditions.
  virtual void HandleResourceUpdate(
      const page_load_metrics::mojom::ResourceDataUpdatePtr& resource) {}

  // Resets all expectations.
  virtual void ResetExpectations();

 private:
  // PageLoadMetricsObserver used by the PageLoadMetricsTestWaiter to observe
  // metrics updates.
  class WaiterMetricsObserver
      : public page_load_metrics::PageLoadMetricsObserver {
   public:
    using FrameTreeNodeId =
        page_load_metrics::PageLoadMetricsObserver::FrameTreeNodeId;
    // We use a WeakPtr to the PageLoadMetricsTestWaiter because |waiter| can be
    // destroyed before this WaiterMetricsObserver.
    explicit WaiterMetricsObserver(
        base::WeakPtr<PageLoadMetricsTestWaiter> waiter);
    ~WaiterMetricsObserver() override;

    void OnTimingUpdate(
        content::RenderFrameHost* subframe_rfh,
        const page_load_metrics::mojom::PageLoadTiming& timing) override;

    void OnCpuTimingUpdate(
        content::RenderFrameHost* subframe_rfh,
        const page_load_metrics::mojom::CpuTiming& timing) override;

    void OnLoadingBehaviorObserved(content::RenderFrameHost* rfh,
                                   int behavior_flags) override;
    void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                              extra_request_complete_info) override;

    void OnResourceDataUseObserved(
        content::RenderFrameHost* rfh,
        const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
            resources) override;

    void OnFeaturesUsageObserved(
        content::RenderFrameHost* rfh,
        const std::vector<blink::UseCounterFeature>&) override;

    void OnDidFinishSubFrameNavigation(
        content::NavigationHandle* navigation_handle) override;
    void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                          const gfx::Size& frame_size) override;
    void OnFrameIntersectionUpdate(
        content::RenderFrameHost* rfh,
        const page_load_metrics::mojom::FrameIntersectionUpdate&
            frame_intersection_update) override;

    void OnV8MemoryChanged(
        const std::vector<MemoryUpdate>& memory_updates) override;

   private:
    const base::WeakPtr<PageLoadMetricsTestWaiter> waiter_;
  };

  // Manages a bitset of TimingFields.
  class TimingFieldBitSet {
   public:
    TimingFieldBitSet() {}

    // Returns whether this bitset has all bits unset.
    bool Empty() const { return bitmask_ == 0; }

    // Returns whether this bitset has the given bit set.
    bool IsSet(TimingField field) const {
      return (bitmask_ & static_cast<int>(field)) != 0;
    }

    // Sets the bit for the given |field|.
    void Set(TimingField field) { bitmask_ |= static_cast<int>(field); }

    // Clears the bit for the given |field|.
    void Clear(TimingField field) { bitmask_ &= ~static_cast<int>(field); }

    // Merges bits set in |other| into this bitset.
    void Merge(const TimingFieldBitSet& other) { bitmask_ |= other.bitmask_; }

    // Clears all bits set in the |other| bitset.
    void ClearMatching(const TimingFieldBitSet& other) {
      bitmask_ &= ~other.bitmask_;
    }

    // Returns whether all the bits in this bitset are set in |other|.
    bool AreAllSetIn(const TimingFieldBitSet& other) const {
      return !((bitmask_ & other.bitmask_) ^ bitmask_);
    }

   private:
    int bitmask_ = 0;
  };

  struct FrameSizeComparator {
    bool operator()(const gfx::Size a, const gfx::Size b) const;
  };

  TimingFieldBitSet GetMatchedBits(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::mojom::FrameMetadata& metadata,
      const PageRenderData* render_data);

  // Updates observed page fields when a timing update is received by the
  // MetricsWebContentsObserver. Stops waiting if expectations are satsfied
  // after update.
  void OnTimingUpdated(content::RenderFrameHost* subframe_rfh,
                       const page_load_metrics::mojom::PageLoadTiming& timing);

  // Updates observed page fields when a timing update is received by the
  // MetricsWebContentsObserver. Stops waiting if expectations are satsfied
  // after update.
  void OnCpuTimingUpdated(content::RenderFrameHost* subframe_rfh,
                          const page_load_metrics::mojom::CpuTiming& timing);

  // Updates observed page fields when a loading behavior (see
  // |blink::LoadingBehaviorFlag|) is observed by MetricsWebContentsObserver.
  // Stops waiting if expectations are satsfied after update.
  void OnLoadingBehaviorObserved(int behavior_flags);

  // Updates observed page fields when a resource load is observed by
  // MetricsWebContentsObserver.  Stops waiting if expectations are satsfied
  // after update.
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info);

  // Updates counters as updates are received from a resource load. Stops
  // waiting if expectations are satisfied after update.
  void OnResourceDataUseObserved(
      content::RenderFrameHost* rfh,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources);

  // Updates |observed_.web_features_| to record any new feature observed.
  // Stops waiting if expectations are satisfied after update.
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& features);

  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size);

  void OnFrameIntersectionUpdate(
      content::RenderFrameHost* rfh,
      const page_load_metrics::mojom::FrameIntersectionUpdate&
          frame_intersection_update);

  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle);

  // Called when V8 per-frame memory usage updates are available.
  void OnV8MemoryChanged(const std::vector<MemoryUpdate>& memory_updates);

  void OnTrackerCreated(page_load_metrics::PageLoadTracker* tracker) override;

  void OnCommit(page_load_metrics::PageLoadTracker* tracker) override;

  void OnActivate(page_load_metrics::PageLoadTracker* tracker) override;

  // These methods check whether expectations are satisfied for specific fields
  // inside the State object, by comparing them in expected_ and observed_.
  bool CpuTimeExpectationsSatisfied() const;
  bool LoadingBehaviorExpectationsSatisfied() const;
  bool ResourceUseExpectationsSatisfied() const;
  bool UseCounterExpectationsSatisfied() const;
  bool SubframeNavigationExpectationsSatisfied() const;
  bool SubframeDataExpectationsSatisfied() const;
  bool MainFrameIntersectionExpectationsSatisfied() const;
  bool MemoryUpdateExpectationsSatisfied() const;

  void AddObserver(page_load_metrics::PageLoadTracker* tracker);

  std::unique_ptr<base::RunLoop> run_loop_;

  // Holds information about events that can be expected or observed. Each call
  // to Wait() compares expected_ to observed_, and resets both.
  struct State {
    State();
    ~State();

    TimingFieldBitSet page_fields_;
    TimingFieldBitSet subframe_fields_;
    blink::UseCounterFeatureTracker feature_tracker_;
    int loading_behavior_flags_ = 0;
    bool subframe_navigation_ = false;
    bool subframe_data_ = false;
    std::set<gfx::Size, FrameSizeComparator> frame_sizes_;
    bool did_set_main_frame_intersection_ = false;
    std::vector<gfx::Rect> main_frame_intersections_;
    std::unordered_set<content::GlobalRenderFrameHostId,
                       content::GlobalRenderFrameHostIdHasher>
        memory_update_frame_ids_;
  };
  State expected_;
  State observed_;

  TimingFieldBitSet observed_page_fields_;

  int current_complete_resources_ = 0;
  int64_t current_network_bytes_ = 0;

  // Network body bytes are only counted for complete resources.
  int64_t current_network_body_bytes_ = 0;
  int expected_minimum_complete_resources_ = 0;
  int expected_minimum_network_bytes_ = 0;

  // Total time spent int the cpu aggregated across the frames on the page.
  base::TimeDelta current_aggregate_cpu_time_;
  base::TimeDelta expected_minimum_aggregate_cpu_time_;

  bool attach_on_tracker_creation_ = false;
  bool did_add_observer_ = false;

  double last_main_frame_layout_shift_score_ = 0;

  base::WeakPtrFactory<PageLoadMetricsTestWaiter> weak_factory_{this};
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_TEST_WAITER_H_
