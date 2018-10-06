// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

// Set a big enough limit for the number of nodes to ensure memory usage is
// capped. Exceeding such limit will deactivate the algorithm.
constexpr size_t kNodeNumberLimit = 5000;

static bool LargeOnTop(const std::shared_ptr<ImageRecord>& a,
                       const std::shared_ptr<ImageRecord>& b) {
  return a->first_size < b->first_size;
}

static bool LateOnTop(const std::shared_ptr<ImageRecord>& a,
                      const std::shared_ptr<ImageRecord>& b) {
  return a->first_paint_index < b->first_paint_index;
}

ImagePaintTimingDetector::ImagePaintTimingDetector(LocalFrameView* frame_view)
    : largest_image_heap_(LargeOnTop),
      latest_image_heap_(LateOnTop),
      frame_view_(frame_view) {}

void ImagePaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const ImageRecord& first_image_paint,
    unsigned candidate_index) const {
  value.SetInteger("DOMNodeId", first_image_paint.node_id);
  value.SetString("imageUrl", first_image_paint.image_url);
  value.SetInteger("size", first_image_paint.first_size);
  value.SetInteger("candidateIndex", candidate_index);
  value.SetString("frame",
                  IdentifiersFactory::FrameId(&frame_view_->GetFrame()));
}

IntRect ImagePaintTimingDetector::CalculateTransformedRect(
    LayoutRect& invalidated_rect,
    const PaintLayer& painting_layer) const {
  const auto* local_transform = painting_layer.GetLayoutObject()
                                    .FirstFragment()
                                    .LocalBorderBoxProperties()
                                    .Transform();
  const auto* ancestor_transform = painting_layer.GetLayoutObject()
                                       .View()
                                       ->FirstFragment()
                                       .LocalBorderBoxProperties()
                                       .Transform();
  FloatRect invalidated_rect_abs = FloatRect(invalidated_rect);
  if (invalidated_rect_abs.IsEmpty() || invalidated_rect_abs.IsZero())
    return IntRect();
  DCHECK(local_transform);
  DCHECK(ancestor_transform);
  GeometryMapper::SourceToDestinationRect(local_transform, ancestor_transform,
                                          invalidated_rect_abs);
  IntRect invalidated_rect_in_viewport = RoundedIntRect(invalidated_rect_abs);
  ScrollableArea* scrollable_area = frame_view_->GetScrollableArea();
  DCHECK(scrollable_area);
  IntRect viewport = scrollable_area->VisibleContentRect();
  invalidated_rect_in_viewport.Intersect(viewport);
  return invalidated_rect_in_viewport;
}

void ImagePaintTimingDetector::Analyze() {
  ImageRecord* largest_image_record = FindLargestPaintCandidate();
  // In cases where largest/last image is still pending for timing, we discard
  // the result and wait for the next analysis.
  if (largest_image_record &&
      !largest_image_record->first_paint_time_after_loaded.is_null()) {
    std::unique_ptr<TracedValue> value = TracedValue::Create();
    PopulateTraceValue(*value, *largest_image_record,
                       ++largest_image_candidate_index_max_);
    TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
        "loading", "LargestImagePaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
        largest_image_record->first_paint_time_after_loaded, "data",
        std::move(value));
  }
  ImageRecord* last_image_record = FindLastPaintCandidate();
  if (last_image_record &&
      !last_image_record->first_paint_time_after_loaded.is_null()) {
    std::unique_ptr<TracedValue> value = TracedValue::Create();
    PopulateTraceValue(*value, *last_image_record,
                       ++last_image_candidate_index_max_);
    TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
        "loading", "LastImagePaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
        last_image_record->first_paint_time_after_loaded, "data",
        std::move(value));
  }
}

void ImagePaintTimingDetector::OnPrePaintFinished() {
  frame_index_++;
  if (records_pending_timing_.size() <= 0)
    return;
  // If the last frame index of queue has changed, it means there are new
  // records pending timing.
  DOMNodeId node_id = records_pending_timing_.back();
  if (!id_record_map_.Contains(node_id))
    return;
  unsigned last_frame_index = id_record_map_.at(node_id)->frame_index;
  if (last_frame_index_queued_for_timing_ >= last_frame_index)
    return;

  last_frame_index_queued_for_timing_ = last_frame_index;
  // Records with frame index up to last_frame_index_queued_for_timing_ will be
  // queued for swap time.
  RegisterNotifySwapTime();
}

void ImagePaintTimingDetector::NotifyNodeRemoved(DOMNodeId node_id) {
  if (id_record_map_.Contains(node_id)) {
    // We assume that the removed node's id wouldn't be recycled, so we don't
    // bother to remove these records from largest_image_heap_ or
    // latest_image_heap_, to reduce computation.
    id_record_map_.erase(node_id);
  }
}

void ImagePaintTimingDetector::RegisterNotifySwapTime() {
  WebLayerTreeView::ReportTimeCallback callback =
      WTF::Bind(&ImagePaintTimingDetector::ReportSwapTime,
                WrapWeakPersistent(this), last_frame_index_queued_for_timing_);
  if (notify_swap_time_override_for_testing_) {
    // Run is not to run the |callback|, but to queue it.
    notify_swap_time_override_for_testing_.Run(std::move(callback));
    return;
  }
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage())
    return;
  WebLayerTreeView* layerTreeView =
      frame.GetPage()->GetChromeClient().GetWebLayerTreeView(&frame);
  if (!layerTreeView)
    return;
  layerTreeView->NotifySwapTime(std::move(callback));
}

void ImagePaintTimingDetector::ReportSwapTime(
    unsigned max_frame_index_to_time,
    WebLayerTreeView::SwapResult result,
    base::TimeTicks timestamp) {
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  // This callback is queued only when there are records in
  // records_pending_timing_.
  DCHECK_GT(records_pending_timing_.size(), 0UL);
  while (records_pending_timing_.size() > 0) {
    DOMNodeId node_id = records_pending_timing_.front();
    if (!id_record_map_.Contains(node_id)) {
      records_pending_timing_.pop();
      continue;
    }
    auto&& record = id_record_map_.at(node_id);
    if (record->frame_index > max_frame_index_to_time)
      break;
    record->first_paint_time_after_loaded = timestamp;
    records_pending_timing_.pop();
  }

  Analyze();
}

void ImagePaintTimingDetector::RecordImage(const LayoutObject& object,
                                           const PaintLayer& painting_layer) {
  Node* node = object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  if (size_zero_ids_.Contains(node_id))
    return;

  const LayoutImage* image = ToLayoutImage(&object);
  if (!id_record_map_.Contains(node_id)) {
    recorded_node_count_++;
    if (recorded_node_count_ < kNodeNumberLimit) {
      LayoutRect invalidated_rect = image->FirstFragment().VisualRect();
      // Do not record first size until invalidated_rect's size becomes
      // non-empty.
      if (invalidated_rect.IsEmpty())
        return;
      IntRect invalidated_rect_in_viewport =
          CalculateTransformedRect(invalidated_rect, painting_layer);
      int rect_size = invalidated_rect_in_viewport.Height() *
                      invalidated_rect_in_viewport.Width();
      if (rect_size == 0) {
        // When rect_size == 0, it either means the image is size 0 or the image
        // is out of viewport. Either way, we don't track this image anymore, to
        // reduce computation.
        size_zero_ids_.insert(node_id);
        return;
      }
      std::shared_ptr<ImageRecord> record = std::make_shared<ImageRecord>();
      record->node_id = node_id;
      record->frame_index = frame_index_;
      record->first_size = rect_size;
      record->first_paint_index = ++first_paint_index_max_;
      ImageResourceContent* cachedImg = image->CachedImage();
      record->image_url =
          !cachedImg ? "" : cachedImg->Url().StrippedForUseAsReferrer();
      id_record_map_.insert(node_id, record);
      largest_image_heap_.push(record);
      latest_image_heap_.push(record);
    } else {
      // for assessing whether kNodeNumberLimit is large enough for all
      // normal cases
      TRACE_EVENT_INSTANT1("loading", "ImagePaintTimingDetector::OverNodeLimit",
                           TRACE_EVENT_SCOPE_THREAD, "recorded_node_count",
                           recorded_node_count_);
    }
  }

  if (id_record_map_.Contains(node_id) &&
      IsJustLoaded(image, id_record_map_.at(node_id))) {
    records_pending_timing_.push(node_id);
    id_record_map_.at(node_id)->loaded = true;
  }
}

bool ImagePaintTimingDetector::IsJustLoaded(
    const LayoutImage* image,
    std::shared_ptr<ImageRecord>&& record) const {
  ImageResourceContent* cachedImg = image->CachedImage();
  return cachedImg && cachedImg->IsLoaded() && !record->loaded;
}

ImageRecord* ImagePaintTimingDetector::FindLargestPaintCandidate() {
  while (!largest_image_heap_.empty() &&
         !id_record_map_.Contains(largest_image_heap_.top()->node_id)) {
    // If node_id_record_map_ doesn't have record.node_id, the node has been
    // deleted. We discard the records of deleted node.
    largest_image_heap_.pop();
  }
  // We report the result as the first paint after the largest image finishes
  // loading. If the largest image is still loading, we report nothing and come
  // back later to see if the largest image by then has finished loading.
  if (!largest_image_heap_.empty() && largest_image_heap_.top()->loaded)
    return largest_image_heap_.top().get();
  return nullptr;
}

ImageRecord* ImagePaintTimingDetector::FindLastPaintCandidate() {
  while (!latest_image_heap_.empty() &&
         !id_record_map_.Contains(latest_image_heap_.top()->node_id)) {
    // If node_id_record_map_ doesn't have record.node_id, the node has been
    // deleted. We discard the records of deleted node.
    latest_image_heap_.pop();
  }
  // We report the result as the first paint after the latest image finishes
  // loading. If the latest image is still loading, we report nothing and come
  // back later to see if the latest image at that time has finished loading.
  if (!latest_image_heap_.empty() && latest_image_heap_.top()->loaded)
    return latest_image_heap_.top().get();
  return nullptr;
}

void ImagePaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}
}  // namespace blink
