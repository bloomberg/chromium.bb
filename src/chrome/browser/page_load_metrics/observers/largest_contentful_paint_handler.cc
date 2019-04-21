// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/largest_contentful_paint_handler.h"
#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "content/public/browser/render_frame_host.h"

namespace page_load_metrics {

// TODO(crbug/616901): True in test only. Since we are unable to config
// navigation start in tests, we disable the offsetting to make the test
// deterministic.
static bool g_disable_subframe_navigation_start_offset = false;

namespace {

const TimingInfo& MergeTimingsBySizeAndTime(const TimingInfo& timing1,
                                            const TimingInfo& timing2) {
  // When both are empty, just return either.
  if (timing1.IsEmpty() && timing2.IsEmpty())
    return timing1;

  if (timing1.IsEmpty() && !timing2.IsEmpty())
    return timing2;
  if (!timing1.IsEmpty() && timing2.IsEmpty())
    return timing1;
  if (timing1.Size() > timing2.Size())
    return timing1;
  if (timing1.Size() < timing2.Size())
    return timing2;
  // When both sizes are equal
  DCHECK(timing1.Time());
  DCHECK(timing2.Time());
  if (timing1.Time().value() < timing2.Time().value())
    return timing1;
  return timing2;
}

void MergeForSubframesWithAdjustedTime(
    TimingInfo* inout_timing,
    const base::Optional<base::TimeDelta>& candidate_new_time,
    const uint64_t& candidate_new_size) {
  DCHECK(inout_timing);
  const TimingInfo new_candidate(candidate_new_time, candidate_new_size,
                                 inout_timing->Type());
  const TimingInfo& merged_candidate =
      MergeTimingsBySizeAndTime(new_candidate, *inout_timing);
  inout_timing->Reset(merged_candidate.Time(), merged_candidate.Size());
}

void MergeForSubframes(
    TimingInfo* inout_timing,
    const base::Optional<base::TimeDelta>& candidate_new_time,
    const uint64_t& candidate_new_size,
    base::TimeDelta navigation_start_offset) {
  MergeForSubframesWithAdjustedTime(
      inout_timing,
      candidate_new_time ? navigation_start_offset + candidate_new_time.value()
                         : candidate_new_time,
      candidate_new_size);
}

bool IsSubframe(content::RenderFrameHost* subframe_rfh) {
  return subframe_rfh != nullptr && subframe_rfh->GetParent() != nullptr;
}

}  // namespace

TimingInfo::TimingInfo(PageLoadMetricsObserver::LargestContentType type)
    : time_(base::Optional<base::TimeDelta>()), size_(0), type_(type) {}
TimingInfo::TimingInfo(
    const base::Optional<base::TimeDelta>& time,
    const uint64_t& size,
    const page_load_metrics::PageLoadMetricsObserver::LargestContentType type)
    : time_(time), size_(size), type_(type) {}

TimingInfo::TimingInfo(const TimingInfo& other) = default;

// static
void LargestContentfulPaintHandler::SetTestMode(bool enabled) {
  g_disable_subframe_navigation_start_offset = enabled;
}

void TimingInfo::Reset(const base::Optional<base::TimeDelta>& time,
                       const uint64_t& size) {
  size_ = size;
  time_ = time;
  DCHECK(HasConsistentTimeAndSize());
}

ContentfulPaint::ContentfulPaint()
    : text_(PageLoadMetricsObserver::LargestContentType::kText),
      image_(PageLoadMetricsObserver::LargestContentType::kImage) {}

const TimingInfo& ContentfulPaint::MergeTextAndImageTiming() {
  return MergeTimingsBySizeAndTime(text_, image_);
}

LargestContentfulPaintHandler::LargestContentfulPaintHandler() = default;
LargestContentfulPaintHandler::~LargestContentfulPaintHandler() = default;

void LargestContentfulPaintHandler::RecordTiming(
    const mojom::PaintTimingPtr& timing,
    content::RenderFrameHost* subframe_rfh) {
  if (!IsSubframe(subframe_rfh)) {
    RecordMainFrameTiming(timing);
    return;
  }
  // For subframes
  base::TimeDelta navigation_start_offset;
  if (!g_disable_subframe_navigation_start_offset) {
    const auto it = subframe_navigation_start_offset_.find(
        subframe_rfh->GetFrameTreeNodeId());
    if (it == subframe_navigation_start_offset_.end()) {
      // We received timing information for an untracked load. Ignore it.
      return;
    }
    navigation_start_offset = it->second;
  }
  RecordSubframeTiming(timing, navigation_start_offset);
}

const TimingInfo& LargestContentfulPaintHandler::MergeMainFrameAndSubframes() {
  const TimingInfo& main_frame_timing =
      main_frame_contentful_paint_.MergeTextAndImageTiming();
  const TimingInfo& subframe_timing =
      subframe_contentful_paint_.MergeTextAndImageTiming();
  return MergeTimingsBySizeAndTime(main_frame_timing, subframe_timing);
}

// We handle subframe and main frame differently. For main frame, we directly
// substitute the candidate when we receive a new one. For subframes (plural),
// we merge the candidates from different subframes by keeping the largest one.
// Note that the merging of subframes' timings will make
// |subframe_contentful_paint_| unable to be replaced with a smaller paint (it
// should have been able when a large ephemeral element is removed). This is a
// trade-off we make to keep a simple algorithm, otherwise we will have to
// track one candidate per subframe.
void LargestContentfulPaintHandler::RecordSubframeTiming(
    const mojom::PaintTimingPtr& timing,
    const base::TimeDelta& navigation_start_offset) {
  MergeForSubframes(&subframe_contentful_paint_.Text(),
                    timing->largest_text_paint, timing->largest_text_paint_size,
                    navigation_start_offset);
  MergeForSubframes(&subframe_contentful_paint_.Image(),
                    timing->largest_image_paint,
                    timing->largest_image_paint_size, navigation_start_offset);
}

void LargestContentfulPaintHandler::RecordMainFrameTiming(
    const mojom::PaintTimingPtr& timing) {
  main_frame_contentful_paint_.Text().Reset(timing->largest_text_paint,
                                            timing->largest_text_paint_size);
  main_frame_contentful_paint_.Image().Reset(timing->largest_image_paint,
                                             timing->largest_image_paint_size);
}

void LargestContentfulPaintHandler::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  if (!navigation_handle->HasCommitted())
    return;

  // We have a new committed navigation, so discard information about the
  // previously committed navigation.
  subframe_navigation_start_offset_.erase(
      navigation_handle->GetFrameTreeNodeId());

  if (extra_info.navigation_start > navigation_handle->NavigationStart())
    return;
  base::TimeDelta navigation_delta =
      navigation_handle->NavigationStart() - extra_info.navigation_start;
  subframe_navigation_start_offset_.insert(std::make_pair(
      navigation_handle->GetFrameTreeNodeId(), navigation_delta));
}

}  // namespace page_load_metrics
