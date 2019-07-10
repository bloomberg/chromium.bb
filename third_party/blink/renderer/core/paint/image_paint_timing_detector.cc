// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {

// In order for |rect_size| to align with the importance of the image, we
// use this heuristics to alleviate the effect of scaling. For example,
// an image has intrinsic size being 1x1 and scaled to 100x100, but only 50x100
// is visible in the viewport. In this case, |intrinsic_image_size| is 1x1;
// |displayed_image_size| is 100x100. |intrinsic_image_size| is 50x100.
// As the image do not have a lot of content, we down scale |visual_size| by the
// ratio of |intrinsic_image_size|/|displayed_image_size| = 1/10000.
//
// * |visual_size| referes to the size of the |displayed_image_size| after
// clipping and transforming. The size is in the main-frame's coordinate.
// * |displayed_image_size| refers to the paint size in the image object's
// coordinate.
// * |intrinsic_image_size| refers to the the image object's original size
// before scaling. The size is in the image object's coordinate.
uint64_t DownScaleIfIntrinsicSizeIsSmaller(
    uint64_t visual_size,
    const uint64_t& intrinsic_image_size,
    const uint64_t& displayed_image_size) {
  // This is an optimized equivalence to:
  // |visual_size| * min(|displayed_image_size|, |intrinsic_image_size|) /
  // |displayed_image_size|
  if (intrinsic_image_size < displayed_image_size) {
    DCHECK_GT(displayed_image_size, 0u);
    return static_cast<double>(visual_size) * intrinsic_image_size /
           displayed_image_size;
  }
  return visual_size;
}

}  // namespace

static bool LargeImageFirst(const base::WeakPtr<ImageRecord>& a,
                            const base::WeakPtr<ImageRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different nodes with the same |first_size| wouldn't
  // be merged in the set.
  return a->insertion_index < b->insertion_index;
}

ImagePaintTimingDetector::ImagePaintTimingDetector(LocalFrameView* frame_view)
    : frame_view_(frame_view) {}

void ImagePaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const ImageRecord& first_image_paint) {
  value.SetInteger("DOMNodeId", static_cast<int>(first_image_paint.node_id));
  // The cached_image could have been deleted when this is called.
  value.SetString("imageUrl",
                  first_image_paint.cached_image
                      ? String(first_image_paint.cached_image->Url())
                      : "(deleted)");
  value.SetInteger("size", static_cast<int>(first_image_paint.first_size));
  value.SetInteger("candidateIndex", ++count_candidates_);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
}

void ImagePaintTimingDetector::ReportCandidateToTrace(
    ImageRecord& largest_image_record) {
  if (!PaintTimingDetector::IsTracing())
    return;
  DCHECK(!largest_image_record.paint_time.is_null());
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_image_record);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestImagePaint::Candidate",
                                   largest_image_record.paint_time, "data",
                                   std::move(value), "frame",
                                   ToTraceValue(&frame_view_->GetFrame()));
}

void ImagePaintTimingDetector::ReportNoCandidateToTrace() {
  if (!PaintTimingDetector::IsTracing())
    return;
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("candidateIndex", ++count_candidates_);
  value->SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("isOOPIF",
                    !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
  TRACE_EVENT2("loading", "LargestImagePaint::NoCandidate", "data",
               std::move(value), "frame",
               ToTraceValue(&frame_view_->GetFrame()));
}

void ImagePaintTimingDetector::UpdateCandidate() {
  ImageRecord* largest_image_record =
      records_manager_.FindLargestPaintCandidate();
  const base::TimeTicks time = largest_image_record
                                   ? largest_image_record->paint_time
                                   : base::TimeTicks();
  const uint64_t size =
      largest_image_record ? largest_image_record->first_size : 0;
  PaintTimingDetector& detector = frame_view_->GetPaintTimingDetector();
  bool changed = detector.NotifyIfChangedLargestImagePaint(time, size);
  if (!changed)
    return;
  if (!time.is_null()) {
    if (auto* lcp_calculator = detector.GetLargestContentfulPaintCalculator())
      lcp_calculator->OnLargestImageUpdated(largest_image_record);
    // If an image has paint time, it must have been loaded.
    DCHECK(largest_image_record->loaded);
    ReportCandidateToTrace(*largest_image_record);
  } else {
    if (auto* lcp_calculator = detector.GetLargestContentfulPaintCalculator())
      lcp_calculator->OnLargestImageUpdated(nullptr);
    ReportNoCandidateToTrace();
  }
}

void ImagePaintTimingDetector::OnPaintFinished() {
  frame_index_++;
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    UpdateCandidate();
  }

  if (!records_manager_.HasUnregisteredRecordsInQueued(
          last_registered_frame_index_))
    return;

  last_registered_frame_index_ = records_manager_.LastQueuedFrameIndex();
  RegisterNotifySwapTime();
}

void ImagePaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  if (!is_recording_)
    return;

  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(object.GetNode());
  if (node_id == kInvalidDOMNodeId)
    return;
  // The visible record removal has been handled by
  // |NotifyBackgroundImageRemoved|.
  records_manager_.RemoveInvisibleRecordIfNeeded(node_id);
}

void ImagePaintTimingDetector::NotifyBackgroundImageRemoved(
    DOMNodeId node_id,
    const ImageResourceContent* cached_image) {
  if (!is_recording_)
    return;
  BackgroundImageId background_image_id = std::make_pair(node_id, cached_image);
  if (!records_manager_.IsRecordedVisibleNode(background_image_id))
    return;
  records_manager_.RemoveVisibleRecord(background_image_id);
  need_update_timing_at_frame_end_ = true;
}

void ImagePaintTimingDetector::RegisterNotifySwapTime() {
  auto callback = CrossThreadBindOnce(&ImagePaintTimingDetector::ReportSwapTime,
                                      WrapCrossThreadWeakPersistent(this),
                                      last_registered_frame_index_);
  if (notify_swap_time_override_for_testing_) {
    // Run is not to run the |callback|, but to queue it.
    notify_swap_time_override_for_testing_.Run(
        ConvertToBaseOnceCallback(std::move(callback)));
    num_pending_swap_callbacks_++;
    return;
  }
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage())
    return;

  frame.GetPage()->GetChromeClient().NotifySwapTime(frame, std::move(callback));
  num_pending_swap_callbacks_++;
}

void ImagePaintTimingDetector::ReportSwapTime(
    unsigned last_queued_frame_index,
    WebWidgetClient::SwapResult result,
    base::TimeTicks timestamp) {
  if (!is_recording_)
    return;
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  records_manager_.AssignPaintTimeToRegisteredQueuedNodes(
      timestamp, last_queued_frame_index);
  UpdateCandidate();
  num_pending_swap_callbacks_--;
  DCHECK_GE(num_pending_swap_callbacks_, 0);
}

void ImageRecordsManager::AssignPaintTimeToRegisteredQueuedNodes(
    const base::TimeTicks& timestamp,
    unsigned last_queued_frame_index) {
  // TODO(crbug.com/971419): should guarantee the queue not empty.
  while (!images_queued_for_paint_time_.IsEmpty()) {
    base::WeakPtr<ImageRecord>& record = images_queued_for_paint_time_.front();
    if (!record) {
      images_queued_for_paint_time_.pop_front();
      continue;
    }
    if (record->frame_index > last_queued_frame_index)
      break;
    record->paint_time = timestamp;
    images_queued_for_paint_time_.pop_front();
  }
}

void ImagePaintTimingDetector::RecordBackgroundImage(
    const LayoutObject& object,
    const IntSize& intrinsic_size,
    const ImageResourceContent& cached_image,
    const PropertyTreeState& current_paint_chunk_properties) {
  Node* node = object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  DCHECK_NE(node_id, kInvalidDOMNodeId);
  if (records_manager_.IsRecordedInvisibleNode(node_id))
    return;

  BackgroundImageId background_image_id =
      std::make_pair(node_id, &cached_image);
  bool is_recored_visible_node =
      records_manager_.IsRecordedVisibleNode(background_image_id);
  if (is_recored_visible_node &&
      !records_manager_.WasVisibleNodeLoaded(background_image_id) &&
      cached_image.IsLoaded()) {
    records_manager_.OnImageLoaded(background_image_id, frame_index_);
    need_update_timing_at_frame_end_ = true;
    return;
  }

  if (is_recored_visible_node || !is_recording_)
    return;
  IntRect visual_rect = object.FragmentsVisualRectBoundingBox();
  // Before the image resource starts loading, <img> has no size info. We wait
  // until the size is known.
  if (visual_rect.IsEmpty())
    return;
  uint64_t rect_size =
      frame_view_->GetPaintTimingDetector()
          .CalculateVisualRect(visual_rect, current_paint_chunk_properties)
          .Size()
          .Area();
  rect_size = DownScaleIfIntrinsicSizeIsSmaller(
      rect_size, intrinsic_size.Area(), visual_rect.Size().Area());

  if (rect_size == 0) {
    // Each invisible background image is tracked by its node id. In other
    // words, when a node is deemed as invisible, all of the background images
    // are deemed as invisible.
    records_manager_.RecordInvisibleNode(node_id);
  } else {
    records_manager_.RecordVisibleNode(background_image_id, rect_size);
    if (cached_image.IsLoaded()) {
      records_manager_.OnImageLoaded(background_image_id, frame_index_);
      need_update_timing_at_frame_end_ = true;
    }
  }
}

ImageRecordsManager::ImageRecordsManager()
    : size_ordered_set_(&LargeImageFirst) {}

void ImageRecordsManager::OnImageLoaded(
    const BackgroundImageId& background_image_id,
    unsigned current_frame_index) {
  base::WeakPtr<ImageRecord> record = FindVisibleRecord(background_image_id);
  OnImageLoadedInternal(record, current_frame_index);
}

void ImageRecordsManager::OnImageLoadedInternal(
    base::WeakPtr<ImageRecord>& record,
    unsigned current_frame_index) {
  SetLoaded(record);
  QueueToMeasurePaintTime(record, current_frame_index);
}

void ImageRecordsManager::RecordVisibleNode(
    const BackgroundImageId& background_image_id,
    const uint64_t& visual_size) {
  std::unique_ptr<ImageRecord> record = CreateImageRecord(
      background_image_id.first, background_image_id.second, visual_size);
  size_ordered_set_.insert(record->AsWeakPtr());
  visible_background_image_map_.insert(background_image_id, std::move(record));
}

std::unique_ptr<ImageRecord> ImageRecordsManager::CreateImageRecord(
    const DOMNodeId& node_id,
    const ImageResourceContent* cached_image,
    const uint64_t& visual_size) {
  DCHECK_GT(visual_size, 0u);
  std::unique_ptr<ImageRecord> record =
      std::make_unique<ImageRecord>(node_id, cached_image, visual_size);
  return record;
}

ImageRecord* ImageRecordsManager::FindLargestPaintCandidate() const {
  DCHECK_EQ(visible_background_image_map_.size(), size_ordered_set_.size());
  if (size_ordered_set_.size() == 0)
    return nullptr;
  return size_ordered_set_.begin()->get();
}

void ImagePaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}
}  // namespace blink
