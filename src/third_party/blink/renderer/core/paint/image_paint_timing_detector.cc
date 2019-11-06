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
    return static_cast<double>(visual_size) * intrinsic_image_size /
           displayed_image_size;
  }
  return visual_size;
}

}  // namespace

// Set a big enough limit for the number of nodes to ensure memory usage is
// capped. Exceeding such limit will make the detactor stops recording entries.
constexpr size_t kImageNodeNumberLimit = 5000;

static bool LargeImageFirst(const base::WeakPtr<ImageRecord>& a,
                            const base::WeakPtr<ImageRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different nodes with the same |first_size| wouldn't
  // be merged in the set.
  if (a->node_id != b->node_id)
    return a->node_id > b->node_id;
  return a->record_id > b->record_id;
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
  DCHECK(!largest_image_record.paint_time.is_null());
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_image_record);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestImagePaint::Candidate",
                                   largest_image_record.paint_time, "data",
                                   std::move(value), "frame",
                                   ToTraceValue(&frame_view_->GetFrame()));
}

void ImagePaintTimingDetector::ReportNoCandidateToTrace() {
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
  bool changed =
      frame_view_->GetPaintTimingDetector().NotifyIfChangedLargestImagePaint(
          time, size);
  if (!changed)
    return;
  if (largest_image_record && !largest_image_record->paint_time.is_null()) {
    // If an image has paint time, it must have been loaded.
    DCHECK(largest_image_record->loaded);
    ReportCandidateToTrace(*largest_image_record);
  } else {
    ReportNoCandidateToTrace();
  }
}

void ImagePaintTimingDetector::OnPaintFinished() {
  frame_index_++;
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    UpdateCandidate();
  }
  if (!records_manager_.NeedMeausuringPaintTime())
    return;

  if (!records_manager_.HasUnregisteredRecordsInQueued(
          last_registered_frame_index_))
    return;

  last_registered_frame_index_ = records_manager_.LastQueuedFrameIndex();
  RegisterNotifySwapTime();
}

void ImagePaintTimingDetector::NotifyNodeRemoved(DOMNodeId node_id) {
  if (!is_recording_)
    return;
  // Todo: check whether it is visible background image.
  if (!records_manager_.IsRecordedVisibleNode(node_id))
    return;
  records_manager_.SetNodeDetached(node_id);
  need_update_timing_at_frame_end_ = true;
}

void ImagePaintTimingDetector::NotifyBackgroundImageRemoved(
    DOMNodeId node_id,
    const ImageResourceContent* cached_image) {
  if (!is_recording_)
    return;
  BackgroundImageId background_image_id = std::make_pair(node_id, cached_image);
  if (!records_manager_.IsRecordedVisibleNode(background_image_id))
    return;
  records_manager_.SetNodeDetached(background_image_id.first);
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
  DCHECK(!images_queued_for_paint_time_.empty());
  while (!images_queued_for_paint_time_.empty()) {
    base::WeakPtr<ImageRecord>& record = images_queued_for_paint_time_.front();
    if (record->frame_index > last_queued_frame_index)
      break;
    record->paint_time = timestamp;
    images_queued_for_paint_time_.pop();
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

  records_manager_.SetNodeReattachedIfNeeded(node_id);
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
      rect_size, intrinsic_size.Area(),
      (visual_rect.Width() * visual_rect.Height()));

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

  if (records_manager_.RecordedTooManyNodes())
    HandleTooManyNodes();
}

void ImagePaintTimingDetector::RecordImage(
    const LayoutObject& object,
    const IntSize& intrinsic_size,
    const ImageResourceContent& cached_image,
    const PropertyTreeState& current_paint_chunk_properties) {
  // TODO(crbug.com/933479): Use LayoutObject::GeneratingNode() to include
  // anonymous objects' rect.
  Node* node = object.GetNode();
  if (!node)
    return;

  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  DCHECK_NE(node_id, kInvalidDOMNodeId);

  if (records_manager_.IsRecordedInvisibleNode(node_id))
    return;

  records_manager_.SetNodeReattachedIfNeeded(node_id);

  bool is_loaded = cached_image.IsLoaded();
  bool is_recored_visible_node =
      records_manager_.IsRecordedVisibleNode(node_id);
  if (is_recored_visible_node &&
      !records_manager_.WasVisibleNodeLoaded(node_id) && is_loaded) {
    records_manager_.OnImageLoaded(node_id, frame_index_);
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
      rect_size, intrinsic_size.Area(),
      visual_rect.Width() * visual_rect.Height());
  DVLOG(2) << "Node id (" << node_id << "): size=" << rect_size
           << ", type=" << object.DebugName();
  if (rect_size == 0) {
    records_manager_.RecordInvisibleNode(node_id);
  } else {
    records_manager_.RecordVisibleNode(node_id, rect_size);
    if (is_loaded) {
      records_manager_.OnImageLoaded(node_id, frame_index_);
      need_update_timing_at_frame_end_ = true;
    }
  }

  if (records_manager_.RecordedTooManyNodes())
    HandleTooManyNodes();
}

void ImagePaintTimingDetector::HandleTooManyNodes() {
  TRACE_EVENT_INSTANT0("loading", "ImagePaintTimingDetector::OverNodeLimit",
                       TRACE_EVENT_SCOPE_THREAD);
  StopRecordEntries();
}

ImageRecordsManager::ImageRecordsManager()
    : size_ordered_set_(&LargeImageFirst) {}

void ImageRecordsManager::SetNodeReattachedIfNeeded(
    const DOMNodeId& visible_node_id) {
  if (!detached_ids_.Contains(visible_node_id))
    return;
  detached_ids_.erase(visible_node_id);
}

base::WeakPtr<ImageRecord> ImageRecordsManager::FindVisibleRecord(
    const BackgroundImageId& background_image_id) const {
  DCHECK(visible_background_image_map_.Contains(background_image_id));
  return visible_background_image_map_.find(background_image_id)
      ->value->AsWeakPtr();
}

base::WeakPtr<ImageRecord> ImageRecordsManager::FindVisibleRecord(
    const DOMNodeId& node_id) const {
  DCHECK(visible_node_map_.Contains(node_id));
  return visible_node_map_.find(node_id)->value->AsWeakPtr();
}

void ImageRecordsManager::OnImageLoaded(const DOMNodeId& node_id,
                                        unsigned current_frame_index) {
  base::WeakPtr<ImageRecord> record = FindVisibleRecord(node_id);
  OnImageLoadedInternal(record, current_frame_index);
}

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

void ImageRecordsManager::SetLoaded(base::WeakPtr<ImageRecord>& record) {
  record->loaded = true;
}

bool ImageRecordsManager::RecordedTooManyNodes() const {
  return visible_node_map_.size() + visible_background_image_map_.size() +
             invisible_node_ids_.size() >
         kImageNodeNumberLimit;
}

void ImageRecordsManager::SetNodeDetached(const DOMNodeId& visible_node_id) {
  detached_ids_.insert(visible_node_id);
}

bool ImageRecordsManager::HasUnregisteredRecordsInQueued(
    unsigned last_registered_frame_index) {
  DCHECK(last_registered_frame_index <= LastQueuedFrameIndex());
  return last_registered_frame_index < LastQueuedFrameIndex();
}

bool ImageRecordsManager::WasVisibleNodeLoaded(const DOMNodeId& node_id) const {
  DCHECK(visible_node_map_.Contains(node_id));
  return visible_node_map_.at(node_id)->loaded;
}

bool ImageRecordsManager::WasVisibleNodeLoaded(
    const BackgroundImageId& background_image_id) const {
  DCHECK(visible_background_image_map_.Contains(background_image_id));
  return visible_background_image_map_.at(background_image_id)->loaded;
}

void ImageRecordsManager::QueueToMeasurePaintTime(
    base::WeakPtr<ImageRecord>& record,
    unsigned current_frame_index) {
  images_queued_for_paint_time_.push(record);
  record->frame_index = current_frame_index;
}

void ImageRecordsManager::RecordInvisibleNode(const DOMNodeId& node_id) {
  DCHECK(!RecordedTooManyNodes());
  invisible_node_ids_.insert(node_id);
}

void ImageRecordsManager::RecordVisibleNode(const DOMNodeId& node_id,
                                            const uint64_t& visual_size) {
  std::unique_ptr<ImageRecord> record =
      CreateImageRecord(node_id, nullptr, visual_size);
  size_ordered_set_.insert(record->AsWeakPtr());
  visible_node_map_.insert(node_id, std::move(record));
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
  DCHECK(!RecordedTooManyNodes());
  DCHECK_GT(visual_size, 0u);
  std::unique_ptr<ImageRecord> record = std::make_unique<ImageRecord>();
  record->record_id = max_record_id_++;
  record->node_id = node_id;
  record->first_size = visual_size;
  record->cached_image = cached_image;
  return record;
}

void ImagePaintTimingDetector::StopRecordEntries() {
  is_recording_ = false;
}

bool ImagePaintTimingDetector::FinishedReportingImages() const {
  return !is_recording_ && num_pending_swap_callbacks_ == 0;
}

ImageRecord* ImageRecordsManager::FindLargestPaintCandidate() const {
  DCHECK_EQ(visible_node_map_.size() + visible_background_image_map_.size(),
            size_ordered_set_.size());
  for (auto it = size_ordered_set_.begin(); it != size_ordered_set_.end();
       ++it) {
    if (detached_ids_.Contains((*it)->node_id))
      continue;
    return (*it).get();
  }
  return nullptr;
}

void ImagePaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}
}  // namespace blink
