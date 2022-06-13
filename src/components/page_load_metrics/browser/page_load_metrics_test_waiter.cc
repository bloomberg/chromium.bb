// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"

#include "base/check_op.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace page_load_metrics {

namespace {

template <class Set>
bool IsSubset(const Set& set1, const Set& set2) {
  for (const auto& elem : set1) {
    if (set2.find(elem) == set2.end())
      return false;
  }
  return true;
}

}  // namespace

PageLoadMetricsTestWaiter::PageLoadMetricsTestWaiter(
    content::WebContents* web_contents)
    : TestingObserver(web_contents) {}

PageLoadMetricsTestWaiter::~PageLoadMetricsTestWaiter() {
  CHECK(did_add_observer_);
  CHECK_EQ(nullptr, run_loop_.get());
}

void PageLoadMetricsTestWaiter::AddPageExpectation(TimingField field) {
  expected_.page_fields_.Set(field);
  if (field == TimingField::kLoadTimingInfo) {
    attach_on_tracker_creation_ = true;
  }
}

void PageLoadMetricsTestWaiter::AddFrameSizeExpectation(const gfx::Size& size) {
  expected_.frame_sizes_.insert(size);
}

void PageLoadMetricsTestWaiter::AddMainFrameIntersectionExpectation(
    const gfx::Rect& rect) {
  expected_.did_set_main_frame_intersection_ = true;
  expected_.main_frame_intersections_.push_back(rect);
}

void PageLoadMetricsTestWaiter::SetMainFrameIntersectionExpectation() {
  expected_.did_set_main_frame_intersection_ = true;
}

void PageLoadMetricsTestWaiter::AddSubFrameExpectation(TimingField field) {
  CHECK_NE(field, TimingField::kLoadTimingInfo)
      << "LOAD_TIMING_INFO should only be used as a page-level expectation";
  expected_.subframe_fields_.Set(field);
}

void PageLoadMetricsTestWaiter::AddWebFeatureExpectation(
    blink::mojom::WebFeature web_feature) {
  AddUseCounterFeatureExpectation(
      {blink::mojom::UseCounterFeatureType::kWebFeature,
       static_cast<blink::UseCounterFeature::EnumValue>(web_feature)});
}

void PageLoadMetricsTestWaiter::AddUseCounterFeatureExpectation(
    const blink::UseCounterFeature& feature) {
  expected_.feature_tracker_.TestAndSet(feature);
}

void PageLoadMetricsTestWaiter::AddSubframeNavigationExpectation() {
  expected_.subframe_navigation_ = true;
}

void PageLoadMetricsTestWaiter::AddSubframeDataExpectation() {
  expected_.subframe_data_ = true;
}

void PageLoadMetricsTestWaiter::AddMinimumCompleteResourcesExpectation(
    int expected_minimum_complete_resources) {
  expected_minimum_complete_resources_ = expected_minimum_complete_resources;
}

void PageLoadMetricsTestWaiter::AddMinimumNetworkBytesExpectation(
    int expected_minimum_network_bytes) {
  expected_minimum_network_bytes_ = expected_minimum_network_bytes;
}

void PageLoadMetricsTestWaiter::AddMinimumAggregateCpuTimeExpectation(
    base::TimeDelta minimum) {
  expected_minimum_aggregate_cpu_time_ = minimum;
}

void PageLoadMetricsTestWaiter::AddMemoryUpdateExpectation(
    content::GlobalRenderFrameHostId routing_id) {
  expected_.memory_update_frame_ids_.insert(routing_id);
}

void PageLoadMetricsTestWaiter::AddLoadingBehaviorExpectation(
    int behavior_flags) {
  expected_.loading_behavior_flags_ |= behavior_flags;
}

bool PageLoadMetricsTestWaiter::DidObserveInPage(TimingField field) const {
  return observed_.page_fields_.IsSet(field);
}

bool PageLoadMetricsTestWaiter::DidObserveWebFeature(
    blink::mojom::WebFeature feature) const {
  return observed_.feature_tracker_.Test(
      {blink::mojom::UseCounterFeatureType::kWebFeature,
       static_cast<blink::UseCounterFeature::EnumValue>(feature)});
}

void PageLoadMetricsTestWaiter::Wait() {
  if (!ExpectationsSatisfied()) {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_ = nullptr;
  }
  EXPECT_TRUE(ExpectationsSatisfied());
  ResetExpectations();
}

void PageLoadMetricsTestWaiter::OnTimingUpdated(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  auto* delegate = GetDelegateForCommittedLoad();
  if (!delegate)
    return;

  const page_load_metrics::mojom::FrameMetadata& metadata =
      subframe_rfh ? delegate->GetSubframeMetadata()
                   : delegate->GetMainFrameMetadata();
  // There is no way to get the layout shift score only for a subframe so far.
  // See the score only when the frame is the main frame.
  const PageRenderData* render_data =
      subframe_rfh ? nullptr : &delegate->GetMainFrameRenderData();

  TimingFieldBitSet matched_bits =
      GetMatchedBits(timing, metadata, render_data);

  if (subframe_rfh)
    observed_.subframe_fields_.Merge(matched_bits);
  else
    observed_.page_fields_.Merge(matched_bits);

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnCpuTimingUpdated(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  current_aggregate_cpu_time_ += timing.task_time;
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnLoadingBehaviorObserved(int behavior_flags) {
  observed_.loading_behavior_flags_ |= behavior_flags;
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (extra_request_complete_info.request_destination !=
      network::mojom::RequestDestination::kDocument) {
    // The waiter confirms loading timing for the main frame only.
    return;
  }

  if (!extra_request_complete_info.load_timing_info->send_start.is_null() &&
      !extra_request_complete_info.load_timing_info->send_end.is_null() &&
      !extra_request_complete_info.load_timing_info->request_start.is_null()) {
    observed_.page_fields_.Set(TimingField::kLoadTimingInfo);
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    HandleResourceUpdate(resource);
    if (resource->is_complete) {
      current_complete_resources_++;
      if (resource->cache_type ==
          page_load_metrics::mojom::CacheType::kNotCached)
        current_network_body_bytes_ += resource->encoded_body_length;
    }
    current_network_bytes_ += resource->delta_bytes;

    // If |rfh| is a subframe with nonzero bytes, update the subframe
    // data observation.
    if (rfh->GetParent() && resource->delta_bytes > 0)
      observed_.subframe_data_ = true;
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  for (const auto& feature : features) {
    observed_.feature_tracker_.TestAndSet(feature);
  }

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnFrameIntersectionUpdate(
    content::RenderFrameHost* rfh,
    const page_load_metrics::mojom::FrameIntersectionUpdate&
        frame_intersection_update) {
  if (frame_intersection_update.main_frame_intersection_rect) {
    observed_.did_set_main_frame_intersection_ = true;
    observed_.main_frame_intersections_.push_back(
        *frame_intersection_update.main_frame_intersection_rect);
  }
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  observed_.subframe_navigation_ = true;

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  for (const auto& update : memory_updates)
    observed_.memory_update_frame_ids_.insert(update.routing_id);

  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

void PageLoadMetricsTestWaiter::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  observed_.frame_sizes_.insert(frame_size);
  if (ExpectationsSatisfied() && run_loop_)
    run_loop_->Quit();
}

PageLoadMetricsTestWaiter::TimingFieldBitSet
PageLoadMetricsTestWaiter::GetMatchedBits(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::mojom::FrameMetadata& metadata,
    const PageRenderData* render_data) {
  PageLoadMetricsTestWaiter::TimingFieldBitSet matched_bits;
  if (timing.document_timing->load_event_start)
    matched_bits.Set(TimingField::kLoadEvent);
  if (timing.paint_timing->first_paint)
    matched_bits.Set(TimingField::kFirstPaint);
  if (timing.paint_timing->first_contentful_paint)
    matched_bits.Set(TimingField::kFirstContentfulPaint);
  if (timing.paint_timing->first_meaningful_paint)
    matched_bits.Set(TimingField::kFirstMeaningfulPaint);
  if (metadata.behavior_flags &
      blink::LoadingBehaviorFlag::kLoadingBehaviorDocumentWriteBlockReload) {
    matched_bits.Set(TimingField::kDocumentWriteBlockReload);
  }
  if (timing.paint_timing->largest_contentful_paint->largest_image_paint ||
      timing.paint_timing->largest_contentful_paint->largest_text_paint) {
    matched_bits.Set(TimingField::kLargestContentfulPaint);
  }
  if (timing.paint_timing->first_input_or_scroll_notified_timestamp)
    matched_bits.Set(TimingField::kFirstInputOrScroll);
  if (timing.interactive_timing->first_input_delay)
    matched_bits.Set(TimingField::kFirstInputDelay);
  if (!timing.back_forward_cache_timings.empty()) {
    if (!timing.back_forward_cache_timings.back()
             ->first_paint_after_back_forward_cache_restore.is_zero()) {
      matched_bits.Set(TimingField::kFirstPaintAfterBackForwardCacheRestore);
    }
    if (timing.back_forward_cache_timings.back()
            ->first_input_delay_after_back_forward_cache_restore.has_value()) {
      matched_bits.Set(
          TimingField::kFirstInputDelayAfterBackForwardCacheRestore);
    }
    if (!timing.back_forward_cache_timings.back()
             ->request_animation_frames_after_back_forward_cache_restore
             .empty()) {
      matched_bits.Set(
          TimingField::kRequestAnimationFrameAfterBackForwardCacheRestore);
    }
  }

  if (render_data) {
    double layout_shift_score = render_data->layout_shift_score;
    if (last_main_frame_layout_shift_score_ < layout_shift_score)
      matched_bits.Set(TimingField::kLayoutShift);
    last_main_frame_layout_shift_score_ = layout_shift_score;
  }

  return matched_bits;
}

void PageLoadMetricsTestWaiter::OnTrackerCreated(
    page_load_metrics::PageLoadTracker* tracker) {
  // A PageLoadMetricsWaiter should only wait for events from a single page
  // load.
  if (!attach_on_tracker_creation_)
    return;
  AddObserver(tracker);
}

void PageLoadMetricsTestWaiter::OnCommit(
    page_load_metrics::PageLoadTracker* tracker) {
  // A PageLoadMetricsWaiter should only wait for events from a single page
  // load.
  if (attach_on_tracker_creation_)
    return;
  AddObserver(tracker);
}

void PageLoadMetricsTestWaiter::OnActivate(
    page_load_metrics::PageLoadTracker* tracker) {
  // A PageLoadMetricsWaiter should only wait for events from a single page
  // load.
  if (attach_on_tracker_creation_)
    return;
  AddObserver(tracker);
}

void PageLoadMetricsTestWaiter::AddObserver(
    page_load_metrics::PageLoadTracker* tracker) {
  ASSERT_FALSE(did_add_observer_);
  tracker->AddObserver(
      std::make_unique<WaiterMetricsObserver>(weak_factory_.GetWeakPtr()));
  did_add_observer_ = true;
}

bool PageLoadMetricsTestWaiter::CpuTimeExpectationsSatisfied() const {
  return current_aggregate_cpu_time_ >= expected_minimum_aggregate_cpu_time_;
}

bool PageLoadMetricsTestWaiter::LoadingBehaviorExpectationsSatisfied() const {
  // Once we've observed everything we've expected, we're satisfied. We allow
  // other behaviors to be present incidentally.
  return (expected_.loading_behavior_flags_ &
          observed_.loading_behavior_flags_) ==
         expected_.loading_behavior_flags_;
}

bool PageLoadMetricsTestWaiter::ResourceUseExpectationsSatisfied() const {
  return (expected_minimum_complete_resources_ == 0 ||
          current_complete_resources_ >=
              expected_minimum_complete_resources_) &&
         (expected_minimum_network_bytes_ == 0 ||
          current_network_bytes_ >= expected_minimum_network_bytes_);
}

bool PageLoadMetricsTestWaiter::UseCounterExpectationsSatisfied() const {
  // We are only interested to see if all features being set in
  // |expected_.feature_tracker| are observed, but don't care about whether
  // extra features are observed.
  return observed_.feature_tracker_.ContainsForTesting(
      expected_.feature_tracker_);
}

bool PageLoadMetricsTestWaiter::SubframeNavigationExpectationsSatisfied()
    const {
  return !expected_.subframe_navigation_ || observed_.subframe_navigation_;
}

bool PageLoadMetricsTestWaiter::SubframeDataExpectationsSatisfied() const {
  return !expected_.subframe_data_ || observed_.subframe_data_;
}

bool PageLoadMetricsTestWaiter::MainFrameIntersectionExpectationsSatisfied()
    const {
  if (!expected_.did_set_main_frame_intersection_)
    return true;
  if (!observed_.did_set_main_frame_intersection_)
    return false;

  // All expectations must be observed, in the same order.
  // But extra observations are ok.
  auto it = observed_.main_frame_intersections_.begin();
  for (const gfx::Rect& expected : expected_.main_frame_intersections_) {
    while (true) {
      if (it == observed_.main_frame_intersections_.end())
        return false;
      if (*it++ == expected)
        break;
    }
  }
  return true;
}

bool PageLoadMetricsTestWaiter::MemoryUpdateExpectationsSatisfied() const {
  return IsSubset(expected_.memory_update_frame_ids_,
                  observed_.memory_update_frame_ids_);
}

bool PageLoadMetricsTestWaiter::ExpectationsSatisfied() const {
  return expected_.page_fields_.AreAllSetIn(observed_.page_fields_) &&
         expected_.subframe_fields_.AreAllSetIn(observed_.subframe_fields_) &&
         ResourceUseExpectationsSatisfied() &&
         UseCounterExpectationsSatisfied() &&
         SubframeNavigationExpectationsSatisfied() &&
         SubframeDataExpectationsSatisfied() &&
         IsSubset(expected_.frame_sizes_, observed_.frame_sizes_) &&
         LoadingBehaviorExpectationsSatisfied() &&
         CpuTimeExpectationsSatisfied() &&
         MainFrameIntersectionExpectationsSatisfied() &&
         MemoryUpdateExpectationsSatisfied();
}

void PageLoadMetricsTestWaiter::ResetExpectations() {
  expected_ = State();
  observed_ = State();
  expected_minimum_complete_resources_ = 0;
  expected_minimum_network_bytes_ = 0;
  expected_minimum_aggregate_cpu_time_ = base::TimeDelta();
}

PageLoadMetricsTestWaiter::WaiterMetricsObserver::~WaiterMetricsObserver() =
    default;

PageLoadMetricsTestWaiter::WaiterMetricsObserver::WaiterMetricsObserver(
    base::WeakPtr<PageLoadMetricsTestWaiter> waiter)
    : waiter_(waiter) {}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (waiter_)
    waiter_->OnTimingUpdated(subframe_rfh, timing);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  if (waiter_)
    waiter_->OnCpuTimingUpdated(subframe_rfh, timing);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::
    OnLoadingBehaviorObserved(content::RenderFrameHost*, int behavior_flags) {
  if (waiter_)
    waiter_->OnLoadingBehaviorObserved(behavior_flags);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (waiter_)
    waiter_->OnLoadedResource(extra_request_complete_info);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::
    OnResourceDataUseObserved(
        content::RenderFrameHost* rfh,
        const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
            resources) {
  if (waiter_)
    waiter_->OnResourceDataUseObserved(rfh, resources);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  if (waiter_)
    waiter_->OnFeaturesUsageObserved(nullptr, features);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::
    OnFrameIntersectionUpdate(
        content::RenderFrameHost* rfh,
        const page_load_metrics::mojom::FrameIntersectionUpdate&
            frame_intersection_update) {
  if (waiter_)
    waiter_->OnFrameIntersectionUpdate(rfh, frame_intersection_update);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::
    OnDidFinishSubFrameNavigation(
        content::NavigationHandle* navigation_handle) {
  if (waiter_)
    waiter_->OnDidFinishSubFrameNavigation(navigation_handle);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::FrameSizeChanged(
    content::RenderFrameHost* render_frame_host,
    const gfx::Size& frame_size) {
  if (waiter_)
    waiter_->FrameSizeChanged(render_frame_host, frame_size);
}

void PageLoadMetricsTestWaiter::WaiterMetricsObserver::OnV8MemoryChanged(
    const std::vector<MemoryUpdate>& memory_updates) {
  if (waiter_)
    waiter_->OnV8MemoryChanged(memory_updates);
}

bool PageLoadMetricsTestWaiter::FrameSizeComparator::operator()(
    const gfx::Size a,
    const gfx::Size b) const {
  return a.width() < b.width() ||
         (a.width() == b.width() && a.height() < b.height());
}

PageLoadMetricsTestWaiter::State::State() = default;

PageLoadMetricsTestWaiter::State::~State() = default;

}  // namespace page_load_metrics
