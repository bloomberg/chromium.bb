// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"

#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"

namespace page_load_metrics {

// TODO(crbug/616901): True in test only. Since we are unable to config
// navigation start in tests, we disable the offsetting to make the test
// deterministic.
static bool g_disable_subframe_navigation_start_offset = false;

namespace {

const ContentfulPaintTimingInfo& MergeTimingsBySizeAndTime(
    const ContentfulPaintTimingInfo& timing1,
    const ContentfulPaintTimingInfo& timing2) {
  // When both are empty, just return either.
  if (timing1.Empty() && timing2.Empty())
    return timing1;

  if (timing1.Empty() && !timing2.Empty())
    return timing2;
  if (!timing1.Empty() && timing2.Empty())
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
    ContentfulPaintTimingInfo* inout_timing,
    const absl::optional<base::TimeDelta>& candidate_new_time,
    const uint64_t& candidate_new_size) {
  DCHECK(inout_timing);
  const ContentfulPaintTimingInfo new_candidate(
      candidate_new_time, candidate_new_size, inout_timing->TextOrImage(),
      inout_timing->InMainFrame(), inout_timing->Type());
  const ContentfulPaintTimingInfo& merged_candidate =
      MergeTimingsBySizeAndTime(new_candidate, *inout_timing);
  inout_timing->Reset(merged_candidate.Time(), merged_candidate.Size(),
                      merged_candidate.Type());
}

bool IsSubframe(content::RenderFrameHost* subframe_rfh) {
  return subframe_rfh != nullptr && subframe_rfh->GetParent() != nullptr;
}

void Reset(ContentfulPaintTimingInfo& timing) {
  timing.Reset(absl::nullopt, 0u, 0 /*type*/);
}

bool IsSameSite(const GURL& url1, const GURL& url2) {
  // We can't use SiteInstance::IsSameSiteWithURL() because both mainframe and
  // subframe are under default SiteInstance on low-end Android environment, and
  // it treats them as same-site even though the passed url is actually not a
  // same-site.
  return url1.SchemeIs(url2.scheme()) &&
         net::registry_controlled_domains::SameDomainOrHost(
             url1, url2,
             net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}
}  // namespace

ContentfulPaintTimingInfo::ContentfulPaintTimingInfo(
    LargestContentTextOrImage text_or_image,
    bool in_main_frame,
    uint32_t type)
    : size_(0),
      text_or_image_(text_or_image),
      type_(type),
      in_main_frame_(in_main_frame) {}
ContentfulPaintTimingInfo::ContentfulPaintTimingInfo(
    const absl::optional<base::TimeDelta>& time,
    const uint64_t& size,
    const LargestContentTextOrImage text_or_image,
    bool in_main_frame,
    uint32_t type)
    : time_(time),
      size_(size),
      text_or_image_(text_or_image),
      type_(type),
      in_main_frame_(in_main_frame) {}

ContentfulPaintTimingInfo::ContentfulPaintTimingInfo(
    const ContentfulPaintTimingInfo& other) = default;

std::unique_ptr<base::trace_event::TracedValue>
ContentfulPaintTimingInfo::DataAsTraceValue() const {
  std::unique_ptr<base::trace_event::TracedValue> data =
      std::make_unique<base::trace_event::TracedValue>();
  data->SetInteger("durationInMilliseconds", time_.value().InMilliseconds());
  data->SetInteger("size", size_);
  data->SetString("type", TextOrImageInString());
  data->SetBoolean("inMainFrame", InMainFrame());
  data->SetBoolean(
      "isAnimated",
      Type() & blink::LargestContentfulPaintType::kLCPTypeAnimatedImage);
  return data;
}

std::string ContentfulPaintTimingInfo::TextOrImageInString() const {
  switch (TextOrImage()) {
    case LargestContentTextOrImage::kText:
      return "text";
    case LargestContentTextOrImage::kImage:
      return "image";
    default:
      NOTREACHED();
      return "NOT_REACHED";
  }
}

// static
void LargestContentfulPaintHandler::SetTestMode(bool enabled) {
  g_disable_subframe_navigation_start_offset = enabled;
}

void ContentfulPaintTimingInfo::Reset(
    const absl::optional<base::TimeDelta>& time,
    const uint64_t& size,
    uint32_t type) {
  size_ = size;
  time_ = time;
  type_ = type;
}
ContentfulPaint::ContentfulPaint(bool in_main_frame, uint32_t type)
    : text_(ContentfulPaintTimingInfo::LargestContentTextOrImage::kText,
            in_main_frame,
            type),
      image_(ContentfulPaintTimingInfo::LargestContentTextOrImage::kImage,
             in_main_frame,
             type) {}

const ContentfulPaintTimingInfo& ContentfulPaint::MergeTextAndImageTiming()
    const {
  return MergeTimingsBySizeAndTime(text_, image_);
}

// static
bool LargestContentfulPaintHandler::AssignTimeAndSizeForLargestContentfulPaint(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    absl::optional<base::TimeDelta>* largest_content_paint_time,
    uint64_t* largest_content_paint_size,
    ContentfulPaintTimingInfo::LargestContentTextOrImage*
        largest_content_type) {
  // Size being 0 means the paint time is not recorded.
  if (!largest_contentful_paint.largest_text_paint_size &&
      !largest_contentful_paint.largest_image_paint_size)
    return false;

  if ((largest_contentful_paint.largest_text_paint_size >
       largest_contentful_paint.largest_image_paint_size) ||
      (largest_contentful_paint.largest_text_paint_size ==
           largest_contentful_paint.largest_image_paint_size &&
       largest_contentful_paint.largest_text_paint <
           largest_contentful_paint.largest_image_paint)) {
    *largest_content_paint_time = largest_contentful_paint.largest_text_paint;
    *largest_content_paint_size =
        largest_contentful_paint.largest_text_paint_size;
    *largest_content_type =
        ContentfulPaintTimingInfo::LargestContentTextOrImage::kText;
  } else {
    *largest_content_paint_time = largest_contentful_paint.largest_image_paint;
    *largest_content_paint_size =
        largest_contentful_paint.largest_image_paint_size;
    *largest_content_type =
        ContentfulPaintTimingInfo::LargestContentTextOrImage::kImage;
  }
  return true;
}

LargestContentfulPaintHandler::LargestContentfulPaintHandler()
    : main_frame_contentful_paint_(true /*in_main_frame*/, 0 /*type*/),
      subframe_contentful_paint_(false /*in_main_frame*/, 0 /*type*/),
      cross_site_subframe_contentful_paint_(false /*in_main_frame*/,
                                            0 /*type*/) {}

LargestContentfulPaintHandler::~LargestContentfulPaintHandler() = default;

void LargestContentfulPaintHandler::RecordTiming(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    const absl::optional<base::TimeDelta>&
        first_input_or_scroll_notified_timestamp,
    content::RenderFrameHost* subframe_rfh) {
  if (!IsSubframe(subframe_rfh)) {
    RecordMainFrameTiming(largest_contentful_paint,
                          first_input_or_scroll_notified_timestamp);
    return;
  }
  // For subframes
  const auto it = subframe_navigation_start_offset_.find(
      subframe_rfh->GetFrameTreeNodeId());
  if (it == subframe_navigation_start_offset_.end()) {
    // We received timing information for an untracked load. Ignore it.
    return;
  }
  RecordSubframeTiming(largest_contentful_paint,
                       first_input_or_scroll_notified_timestamp, it->second);
  if (!IsSameSite(subframe_rfh->GetLastCommittedURL(),
                  subframe_rfh->GetMainFrame()->GetLastCommittedURL())) {
    RecordCrossSiteSubframeTiming(largest_contentful_paint, it->second);
  }
}

const ContentfulPaintTimingInfo&
LargestContentfulPaintHandler::MergeMainFrameAndSubframes() const {
  const ContentfulPaintTimingInfo& main_frame_timing =
      main_frame_contentful_paint_.MergeTextAndImageTiming();
  const ContentfulPaintTimingInfo& subframe_timing =
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
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    const absl::optional<base::TimeDelta>&
        first_input_or_scroll_notified_timestamp,
    const base::TimeDelta& navigation_start_offset) {
  UpdateFirstInputOrScrollNotified(first_input_or_scroll_notified_timestamp,
                                   navigation_start_offset);
  MergeForSubframes(&subframe_contentful_paint_.Text(),
                    largest_contentful_paint.largest_text_paint,
                    largest_contentful_paint.largest_text_paint_size,
                    navigation_start_offset);
  MergeForSubframes(&subframe_contentful_paint_.Image(),
                    largest_contentful_paint.largest_image_paint,
                    largest_contentful_paint.largest_image_paint_size,
                    navigation_start_offset);
}

void LargestContentfulPaintHandler::RecordCrossSiteSubframeTiming(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    const base::TimeDelta& navigation_start_offset) {
  MergeForSubframes(&cross_site_subframe_contentful_paint_.Text(),
                    largest_contentful_paint.largest_text_paint,
                    largest_contentful_paint.largest_text_paint_size,
                    navigation_start_offset);
  MergeForSubframes(&cross_site_subframe_contentful_paint_.Image(),
                    largest_contentful_paint.largest_image_paint,
                    largest_contentful_paint.largest_image_paint_size,
                    navigation_start_offset);
}

void LargestContentfulPaintHandler::RecordMainFrameTiming(
    const page_load_metrics::mojom::LargestContentfulPaintTiming&
        largest_contentful_paint,
    const absl::optional<base::TimeDelta>&
        first_input_or_scroll_notified_timestamp) {
  UpdateFirstInputOrScrollNotified(
      first_input_or_scroll_notified_timestamp,
      /* navigation_start_offset */ base::TimeDelta());
  if (IsValid(largest_contentful_paint.largest_text_paint)) {
    main_frame_contentful_paint_.Text().Reset(
        largest_contentful_paint.largest_text_paint,
        largest_contentful_paint.largest_text_paint_size, 0 /*type*/);
  }
  if (IsValid(largest_contentful_paint.largest_image_paint)) {
    main_frame_contentful_paint_.Image().Reset(
        largest_contentful_paint.largest_image_paint,
        largest_contentful_paint.largest_image_paint_size,
        largest_contentful_paint.type);
  }
}

void LargestContentfulPaintHandler::UpdateFirstInputOrScrollNotified(
    const absl::optional<base::TimeDelta>& candidate_new_time,
    const base::TimeDelta& navigation_start_offset) {
  if (!candidate_new_time.has_value())
    return;

  if (first_input_or_scroll_notified_ >
      navigation_start_offset + *candidate_new_time) {
    first_input_or_scroll_notified_ =
        navigation_start_offset + *candidate_new_time;
    // Consider candidates after input to be invalid. This is needed because
    // IPCs from different frames can arrive out of order. For example, this is
    // consistently the case when a click on the main frame produces a new
    // iframe which contains the largest content so far.
    if (!IsValid(main_frame_contentful_paint_.Text().Time()))
      Reset(main_frame_contentful_paint_.Text());
    if (!IsValid(main_frame_contentful_paint_.Image().Time()))
      Reset(main_frame_contentful_paint_.Image());
    if (!IsValid(subframe_contentful_paint_.Text().Time()))
      Reset(subframe_contentful_paint_.Text());
    if (!IsValid(subframe_contentful_paint_.Image().Time()))
      Reset(subframe_contentful_paint_.Image());
    if (!IsValid(cross_site_subframe_contentful_paint_.Text().Time()))
      Reset(cross_site_subframe_contentful_paint_.Text());
    if (!IsValid(cross_site_subframe_contentful_paint_.Image().Time()))
      Reset(cross_site_subframe_contentful_paint_.Image());
  }
}

void LargestContentfulPaintHandler::OnDidFinishSubFrameNavigation(
    content::NavigationHandle* navigation_handle,
    base::TimeTicks navigation_start) {
  if (!navigation_handle->HasCommitted())
    return;

  // We have a new committed navigation, so discard information about the
  // previously committed navigation.
  subframe_navigation_start_offset_.erase(
      navigation_handle->GetFrameTreeNodeId());

  if (navigation_start > navigation_handle->NavigationStart())
    return;
  base::TimeDelta navigation_delta;
  // If navigation start offset tracking has been disabled for tests, then
  // record a zero-value navigation delta. Otherwise, compute the actual delta
  // between the main frame navigation start and the subframe navigation start.
  // See crbug/616901 for more details on why navigation start offset tracking
  // is disabled in tests.
  if (!g_disable_subframe_navigation_start_offset) {
    navigation_delta = navigation_handle->NavigationStart() - navigation_start;
  }
  subframe_navigation_start_offset_.insert(std::make_pair(
      navigation_handle->GetFrameTreeNodeId(), navigation_delta));
}

void LargestContentfulPaintHandler::OnSubFrameDeleted(int frame_tree_node_id) {
  subframe_navigation_start_offset_.erase(frame_tree_node_id);
}

void LargestContentfulPaintHandler::MergeForSubframes(
    ContentfulPaintTimingInfo* inout_timing,
    const absl::optional<base::TimeDelta>& candidate_new_time,
    const uint64_t& candidate_new_size,
    base::TimeDelta navigation_start_offset) {
  absl::optional<base::TimeDelta> new_time = absl::nullopt;
  if (candidate_new_time) {
    // If |candidate_new_time| is TimeDelta(), this means that the candidate is
    // an image that has not finished loading. Preserve its meaning by not
    // adding the |navigation_start_offset|.
    new_time = candidate_new_time->is_positive()
                   ? navigation_start_offset + candidate_new_time.value()
                   : base::TimeDelta();
  }
  if (IsValid(new_time))
    MergeForSubframesWithAdjustedTime(inout_timing, new_time,
                                      candidate_new_size);
}

}  // namespace page_load_metrics
