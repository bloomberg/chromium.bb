// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
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
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {
#ifndef NDEBUG
String GetImageUrl(const LayoutObject& object) {
  if (object.IsLayoutImage()) {
    const ImageResourceContent* cached_image =
        ToLayoutImage(&object)->CachedImage();
    return cached_image ? cached_image->Url().StrippedForUseAsReferrer() : "";
  }
  if (object.IsVideo()) {
    const ImageResourceContent* cached_image =
        ToLayoutVideo(&object)->CachedImage();
    return cached_image ? cached_image->Url().StrippedForUseAsReferrer() : "";
  }
  if (object.IsSVGImage()) {
    const LayoutImageResource* image_resource =
        ToLayoutSVGImage(&object)->ImageResource();
    const ImageResourceContent* cached_image = image_resource->CachedImage();
    return cached_image ? cached_image->Url().StrippedForUseAsReferrer() : "";
  }
  DCHECK(ImagePaintTimingDetector::HasBackgroundImage(object));
  const ComputedStyle* style = object.Style();
  StringBuilder concatenated_result;
  for (const FillLayer* bg_layer = &style->BackgroundLayers(); bg_layer;
       bg_layer = bg_layer->Next()) {
    StyleImage* bg_image = bg_layer->GetImage();
    if (!bg_image || !bg_image->IsImageResource())
      continue;
    const StyleFetchedImage* fetched_image = To<StyleFetchedImage>(bg_image);
    const String url = fetched_image->Url().StrippedForUseAsReferrer();
    concatenated_result.Append(url.Utf8().data(), url.length());
  }
  return concatenated_result.ToString();
}
#endif

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

bool AttachedBackgroundImagesAllLoaded(const LayoutObject& object) {
  DCHECK(ImagePaintTimingDetector::HasBackgroundImage(object));
  const ComputedStyle* style = object.Style();
  DCHECK(style);
  for (const FillLayer* bg_layer = &style->BackgroundLayers(); bg_layer;
       bg_layer = bg_layer->Next()) {
    StyleImage* bg_image = bg_layer->GetImage();
    // A layout object with background images is not loaded until all of the
    // background images are loaded.
    if (!bg_image || !bg_image->IsImageResource())
      continue;
    if (!bg_image->IsLoaded())
      return false;
  }
  return true;
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
  return a->node_id > b->node_id;
}

ImagePaintTimingDetector::ImagePaintTimingDetector(LocalFrameView* frame_view)
    : frame_view_(frame_view) {}

void ImagePaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const ImageRecord& first_image_paint,
    unsigned candidate_index) const {
  value.SetInteger("DOMNodeId", static_cast<int>(first_image_paint.node_id));
#ifndef NDEBUG
  value.SetString("imageUrl", first_image_paint.image_url);
#endif
  value.SetInteger("size", static_cast<int>(first_image_paint.first_size));
  value.SetInteger("candidateIndex", candidate_index);
  value.SetString("frame",
                  IdentifiersFactory::FrameId(&frame_view_->GetFrame()));
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
}

void ImagePaintTimingDetector::OnLargestImagePaintDetected(
    ImageRecord* largest_image_record) {
  DCHECK(largest_image_record);
  DCHECK(!largest_image_record->paint_time.is_null());
  largest_image_paint_ = largest_image_record;
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, *largest_image_record, ++count_candidates_);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "loading", "LargestImagePaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
      largest_image_record->paint_time, "data", std::move(value));
}

void ImagePaintTimingDetector::Analyze() {
  // These conditions represents the following scenarios:
  // 1. candiate being nullptr: no loaded image is found.
  // 2. candidate's first paint being null: largest image is still pending
  //   for timing. We discard the candidate and wait for the next analysis.
  // 3. new candidate equals to old candidate: we don't need to update the
  //   result unless it's a new candidate.
  ImageRecord* largest_image_record =
      records_manager_.FindLargestPaintCandidate();
  if (largest_image_record && !largest_image_record->paint_time.is_null() &&
      largest_image_record != largest_image_paint_) {
    OnLargestImagePaintDetected(largest_image_record);
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
  }
}

void ImagePaintTimingDetector::OnPaintFinished() {
  frame_index_++;
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
  if (!records_manager_.IsRecordedVisibleNode(node_id))
    return;
  records_manager_.SetNodeDetached(node_id);
  if (!records_manager_.AreAllVisibleNodesDetached())
    return;

  if (!largest_image_paint_)
    return;
  largest_image_paint_ = nullptr;
  // This will dispatch the updated |largest_image_paint_| to the browser.
  frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
}

void ImagePaintTimingDetector::RegisterNotifySwapTime() {
  WebWidgetClient::ReportTimeCallback callback =
      WTF::Bind(&ImagePaintTimingDetector::ReportSwapTime,
                WrapWeakPersistent(this), last_registered_frame_index_);
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
  frame.GetPage()->GetChromeClient().NotifySwapTime(frame, std::move(callback));
}

void ImagePaintTimingDetector::ReportSwapTime(
    unsigned last_queued_frame_index,
    WebWidgetClient::SwapResult result,
    base::TimeTicks timestamp) {
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  records_manager_.AssignPaintTimeToRegisteredQueuedNodes(
      timestamp, last_queued_frame_index);
  Analyze();
}

void ImageRecordsManager::AssignPaintTimeToRegisteredQueuedNodes(
    const base::TimeTicks& timestamp,
    unsigned last_queued_frame_index) {
  DCHECK(!images_queued_for_paint_time_.empty());
  while (!images_queued_for_paint_time_.empty()) {
    base::WeakPtr<ImageRecord>& record = images_queued_for_paint_time_.front();
    DCHECK(visible_node_map_.Contains(record->node_id));
    if (record->frame_index > last_queued_frame_index)
      break;
    record->paint_time = timestamp;
    images_queued_for_paint_time_.pop();
  }
}

// static
bool ImagePaintTimingDetector::HasBackgroundImage(const LayoutObject& object) {
  Node* node = object.GetNode();
  if (!node)
    return false;
  const ComputedStyle* style = object.Style();
  if (!style)
    return false;
  for (const FillLayer* bg_layer = &style->BackgroundLayers(); bg_layer;
       bg_layer = bg_layer->Next()) {
    StyleImage* bg_image = bg_layer->GetImage();
    // Rule out images that doesn't load any image resources, e.g., a gradient.
    if (!bg_image || !bg_image->IsImageResource())
      continue;
    return true;
  }
  return false;
}

void ImagePaintTimingDetector::RecordImage(
    const LayoutObject& object,
    const IntSize& intrinsic_size,
    bool is_loaded,
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

  if (records_manager_.IsRecordedVisibleNode(node_id) &&
      !records_manager_.WasVisibleNodeLoaded(node_id) && is_loaded) {
    // TODO(crbug/936149): This can be simplified after we track each background
    // image individually.
    bool has_background_image = HasBackgroundImage(object);
    if ((!has_background_image && is_loaded) ||
        (has_background_image && AttachedBackgroundImagesAllLoaded(object))) {
      records_manager_.OnImageLoaded(node_id, frame_index_);
      return;
    }
  }

  if (records_manager_.IsRecordedVisibleNode(node_id) || !is_recording_)
    return;
  IntRect visual_rect = object.FragmentsVisualRectBoundingBox();
  // Before the image resource starts loading, <img> has no size info. We wait
  // until the size is known.
  if (visual_rect.IsEmpty())
    return;
  uint64_t rect_size =
      frame_view_->GetPaintTimingDetector().CalculateVisualSize(
          visual_rect, current_paint_chunk_properties);
  rect_size = DownScaleIfIntrinsicSizeIsSmaller(
      rect_size, intrinsic_size.Area(),
      visual_rect.Width() * visual_rect.Height());
  DVLOG(2) << "Node id (" << node_id << "): size=" << rect_size
           << ", type=" << object.DebugName();
  if (rect_size == 0) {
    records_manager_.RecordInvisibleNode(node_id);
  } else {
    records_manager_.RecordVisibleNode(node_id, rect_size, object);
    if (is_loaded)
      records_manager_.OnImageLoaded(node_id, frame_index_);
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
  if (!visible_node_map_.Contains(visible_node_id))
    return;
  if (!detached_ids_.Contains(visible_node_id))
    return;
  detached_ids_.erase(visible_node_id);
}

base::WeakPtr<ImageRecord> ImageRecordsManager::FindVisibleRecord(
    const DOMNodeId& node_id) const {
  DCHECK(visible_node_map_.Contains(node_id));
  return visible_node_map_.find(node_id)->value->AsWeakPtr();
}

void ImageRecordsManager::OnImageLoaded(const DOMNodeId& node_id,
                                        unsigned current_frame_index) {
  base::WeakPtr<ImageRecord> record = FindVisibleRecord(node_id);
  SetLoaded(record);
  QueueToMeasurePaintTime(record, current_frame_index);
}

void ImageRecordsManager::SetLoaded(base::WeakPtr<ImageRecord>& record) {
  record->loaded = true;
}

bool ImageRecordsManager::RecordedTooManyNodes() const {
  return visible_node_map_.size() + invisible_node_ids_.size() >
         kImageNodeNumberLimit;
}

void ImageRecordsManager::SetNodeDetached(const DOMNodeId& visible_node_id) {
  DCHECK(visible_node_map_.Contains(visible_node_id));
  detached_ids_.insert(visible_node_id);
}

bool ImageRecordsManager::HasUnregisteredRecordsInQueued(
    unsigned last_registered_frame_index) {
  DCHECK(last_registered_frame_index <= LastQueuedFrameIndex());
  return last_registered_frame_index < LastQueuedFrameIndex();
}

bool ImageRecordsManager::AreAllVisibleNodesDetached() const {
  return visible_node_map_.size() - detached_ids_.size() == 0;
}

bool ImageRecordsManager::WasVisibleNodeLoaded(const DOMNodeId& node_id) const {
  DCHECK(visible_node_map_.Contains(node_id));
  return visible_node_map_.at(node_id)->loaded;
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
                                            const uint64_t& visual_size,
                                            const LayoutObject& object) {
  DCHECK(!RecordedTooManyNodes());
  DCHECK_GT(visual_size, 0u);
  std::unique_ptr<ImageRecord> record = std::make_unique<ImageRecord>();
  record->node_id = node_id;
#ifndef NDEBUG
  record->image_url = GetImageUrl(object);
#endif
  // Mind that first_size has to be assigned at the push of
  // |size_ordered_set_| since it's the sorting key.
  record->first_size = visual_size;
  size_ordered_set_.insert(record->AsWeakPtr());
  visible_node_map_.insert(node_id, std::move(record));
}

// In the context of FCP++, we define contentful background image as one that
// satisfies all of the following conditions:
// * has image reources attached to style of the object, i.e.,
//  { background-image: url('example.gif') }
// * not attached to <body> or <html>
// This function contains the above heuristics.
//
// static
bool ImagePaintTimingDetector::IsBackgroundImageContentful(
    const LayoutObject& object,
    const Image& image) {
  // Background images attached to <body> or <html> are likely for background
  // purpose, so we rule them out.
  if (object.IsLayoutView()) {
    return false;
  }
  // Generated images are excluded here, as they are likely to serve for
  // background purpose.
  if (!image.IsBitmapImage() && !image.IsStaticBitmapImage() &&
      !image.IsSVGImage() && !image.IsPlaceholderImage())
    return false;
  return true;
}

void ImagePaintTimingDetector::StopRecordEntries() {
  is_recording_ = false;
}

ImageRecord* ImageRecordsManager::FindLargestPaintCandidate() {
  DCHECK_EQ(visible_node_map_.size(), size_ordered_set_.size());
  for (auto it = size_ordered_set_.begin(); it != size_ordered_set_.end();
       ++it) {
    if (detached_ids_.Contains((*it)->node_id))
      continue;
    DCHECK(visible_node_map_.Contains((*it)->node_id));
    // If the largest image is still loading, we report nothing and come
    // back later to see if the largest image at that time has finished
    // loading.
    if (!(*it)->loaded)
      return nullptr;
    return (*it).get();
  }
  return nullptr;
}

void ImagePaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}
}  // namespace blink
