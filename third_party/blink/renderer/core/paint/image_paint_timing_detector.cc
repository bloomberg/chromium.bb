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
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {
#ifndef NDEBUG
String GetImageUrl(const LayoutObject& object) {
  if (object.IsImage()) {
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
  DCHECK(ImagePaintTimingDetector::HasContentfulBackgroundImage(object));
  const ComputedStyle* style = object.Style();
  StringBuilder concatenated_result;
  for (const FillLayer* bg_layer = &style->BackgroundLayers(); bg_layer;
       bg_layer = bg_layer->Next()) {
    StyleImage* bg_image = bg_layer->GetImage();
    if (!bg_image || !bg_image->IsImageResource())
      continue;
    const StyleFetchedImage* fetched_image = ToStyleFetchedImage(bg_image);
    const String url = fetched_image->Url().StrippedForUseAsReferrer();
    concatenated_result.Append(url.Utf8().data(), url.length());
  }
  return concatenated_result.ToString();
}
#endif

bool AttachedBackgroundImagesAllLoaded(const LayoutObject& object) {
  DCHECK(ImagePaintTimingDetector::HasContentfulBackgroundImage(object));
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

bool IsLoaded(const LayoutObject& object) {
  if (object.IsImage()) {
    const ImageResourceContent* cached_image =
        ToLayoutImage(&object)->CachedImage();
    return cached_image ? cached_image->IsLoaded() : false;
  }
  if (object.IsVideo()) {
    const ImageResourceContent* cached_image =
        ToLayoutVideo(&object)->CachedImage();
    return cached_image ? cached_image->IsLoaded() : false;
  }
  if (object.IsSVGImage()) {
    const LayoutImageResource* image_resource =
        ToLayoutSVGImage(&object)->ImageResource();
    const ImageResourceContent* cached_image = image_resource->CachedImage();
    return cached_image ? cached_image->IsLoaded() : false;
  }
  DCHECK(ImagePaintTimingDetector::HasContentfulBackgroundImage(object));
  return AttachedBackgroundImagesAllLoaded(object);
}
}  // namespace

// Set a big enough limit for the number of nodes to ensure memory usage is
// capped. Exceeding such limit will make the detactor stops recording entries.
constexpr size_t kImageNodeNumberLimit = 5000;

static bool LargeImageFirst(const base::WeakPtr<ImageRecord>& a,
                            const base::WeakPtr<ImageRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  return a->first_size > b->first_size;
}

static bool LateImageFirst(const base::WeakPtr<ImageRecord>& a,
                           const base::WeakPtr<ImageRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  return a->first_paint_index > b->first_paint_index;
}

ImagePaintTimingDetector::ImagePaintTimingDetector(LocalFrameView* frame_view)
    : size_ordered_set_(&LargeImageFirst),
      time_ordered_set_(&LateImageFirst),
      frame_view_(frame_view) {}

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
}

void ImagePaintTimingDetector::OnLargestImagePaintDetected(
    ImageRecord* largest_image_record) {
  DCHECK(largest_image_record);
  DCHECK(!largest_image_record->first_paint_time_after_loaded.is_null());
  largest_image_paint_ = largest_image_record;
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  PopulateTraceValue(*value, *largest_image_record,
                     ++largest_image_candidate_index_max_);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "loading", "LargestImagePaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
      largest_image_record->first_paint_time_after_loaded, "data",
      std::move(value));
}

void ImagePaintTimingDetector::OnLastImagePaintDetected(
    ImageRecord* last_image_record) {
  DCHECK(last_image_record);
  DCHECK(!last_image_record->first_paint_time_after_loaded.is_null());
  last_image_paint_ = last_image_record;
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  PopulateTraceValue(*value, *last_image_record,
                     ++last_image_candidate_index_max_);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "loading", "LastImagePaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
      last_image_record->first_paint_time_after_loaded, "data",
      std::move(value));
  frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
}

void ImagePaintTimingDetector::Analyze() {
  // These conditions represents the following scenarios:
  // 1. candiate being nullptr: no loaded image is found.
  // 2. candidate's first paint being null: largest/last image is still pending
  //   for timing. We discard the candidate and wait for the next analysis.
  // 3. new candidate equals to old candidate: we don't need to update the
  //   result unless it's a new candidate.
  ImageRecord* largest_image_record = FindLargestPaintCandidate();
  bool new_candidate_detected = false;
  if (largest_image_record &&
      !largest_image_record->first_paint_time_after_loaded.is_null() &&
      largest_image_record != largest_image_paint_) {
    new_candidate_detected = true;
    OnLargestImagePaintDetected(largest_image_record);
  }
  ImageRecord* last_image_record = FindLastPaintCandidate();
  if (last_image_record &&
      !last_image_record->first_paint_time_after_loaded.is_null() &&
      last_image_record != last_image_paint_) {
    new_candidate_detected = true;
    OnLastImagePaintDetected(last_image_record);
  }
  if (new_candidate_detected) {
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
  }
}

void ImagePaintTimingDetector::OnPrePaintFinished() {
  frame_index_++;
  if (records_pending_timing_.size() <= 0)
    return;
  // If the last frame index of queue has changed, it means there are new
  // records pending timing.
  DOMNodeId node_id = records_pending_timing_.back();
  // As we never remove nodes from |id_record_map_|, all of |id_record_map_|
  // must exist.
  DCHECK(id_record_map_.Contains(node_id));
  unsigned last_frame_index = id_record_map_.at(node_id)->frame_index;
  if (last_frame_index_queued_for_timing_ >= last_frame_index)
    return;

  last_frame_index_queued_for_timing_ = last_frame_index;
  // Records with frame index up to last_frame_index_queued_for_timing_ will be
  // queued for swap time.
  RegisterNotifySwapTime();
}

void ImagePaintTimingDetector::NotifyNodeRemoved(DOMNodeId node_id) {
  if (!is_recording_)
    return;
  if (id_record_map_.Contains(node_id)) {
    // We assume that the removed node's id wouldn't be recycled, so we don't
    // bother to remove these records from size_ordered_set_ or
    // time_ordered_set_, to reduce computation.
    detached_ids_.insert(node_id);

    if (id_record_map_.size() - detached_ids_.size() == 0) {
      // If either largest_image_paint_ or last_image_paint_ will change to
      // nullptr, update performance timing.
      if (!largest_image_paint_ && !last_image_paint_)
        return;
      largest_image_paint_ = nullptr;
      last_image_paint_ = nullptr;
      frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
    }
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
  // Not guranteed to be non-empty, because records can be removed between
  // callback registration and invocation.
  while (records_pending_timing_.size() > 0) {
    DOMNodeId node_id = records_pending_timing_.front();
    if (!id_record_map_.Contains(node_id)) {
      records_pending_timing_.pop();
      continue;
    }
    ImageRecord* record = id_record_map_.at(node_id);
    if (record->frame_index > max_frame_index_to_time)
      break;
    record->first_paint_time_after_loaded = timestamp;
    records_pending_timing_.pop();
  }

  Analyze();
}

// In the context of FCP++, we define contentful background image as one that
// satisfies all of the following conditions:
// * has image reources attached to style of the object, i.e.,
//  { background-image: url('example.gif') }
// * not attached to <body> or <html>
//
// static
bool ImagePaintTimingDetector::HasContentfulBackgroundImage(
    const LayoutObject& object) {
  Node* node = object.GetNode();
  if (!node)
    return false;
  // Background images attached to <body> or <html> are likely for background
  // purpose, so we rule them out, according to the following rules:
  // * Box model objects includes objects of box model, such as <div>, <body>,
  //  LayoutView, but not includes LayoutText.
  // * BackgroundTransfersToView is true for the <body>, <html>, e.g., that
  //  have transferred their background to LayoutView.
  // * LayoutView has the background transfered by <html> if <html> has
  //  background.
  if (!object.IsBoxModelObject() ||
      ToLayoutBoxModelObject(object).BackgroundTransfersToView())
    return false;
  if (object.IsLayoutView())
    return false;
  const ComputedStyle* style = object.Style();
  if (!style)
    return false;
  if (!style->HasBackgroundImage())
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

void ImagePaintTimingDetector::RecordImage(const LayoutObject& object,
                                           const PaintLayer& painting_layer) {
  Node* node = object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  if (size_zero_ids_.Contains(node_id))
    return;
  // The node is reattached.
  if (id_record_map_.Contains(node_id) && detached_ids_.Contains(node_id))
    detached_ids_.erase(node_id);

  if (!id_record_map_.Contains(node_id) && is_recording_) {
    LayoutRect invalidated_rect = object.FirstFragment().VisualRect();
    // Before the image resource is loaded, <img> has size 0, so we do not
    // record the first size until the invalidated rect's size becomes
    // non-empty.
    if (invalidated_rect.IsEmpty())
      return;
    uint64_t rect_size =
        frame_view_->GetPaintTimingDetector().CalculateVisualSize(
            invalidated_rect, painting_layer);
    if (rect_size == 0) {
      // When rect_size == 0, it either means the image is size 0 or the image
      // is out of viewport. Either way, we don't track this image anymore, to
      // reduce computation.
      size_zero_ids_.insert(node_id);
      return;
    }
    // Non-trivial image is found.
    std::unique_ptr<ImageRecord> record = std::make_unique<ImageRecord>();
    record->node_id = node_id;
#ifndef NDEBUG
    record->image_url = GetImageUrl(object);
#endif
    // Mind that first_size has to be assigned at the push of
    // size_ordered_set_ since it's the sorting key.
    record->first_size = rect_size;
    size_ordered_set_.insert(record->AsWeakPtr());
    id_record_map_.insert(node_id, std::move(record));
    if (id_record_map_.size() + size_zero_ids_.size() > kImageNodeNumberLimit) {
      TRACE_EVENT_INSTANT2("loading", "ImagePaintTimingDetector::OverNodeLimit",
                           TRACE_EVENT_SCOPE_THREAD, "recorded_node_count",
                           id_record_map_.size(), "size_zero_node_count",
                           size_zero_ids_.size());
      StopRecordEntries();
    }
  }

  if (id_record_map_.Contains(node_id) && !id_record_map_.at(node_id)->loaded &&
      IsLoaded(object)) {
    // The image is just loaded.
    records_pending_timing_.push(node_id);
    ImageRecord* record = id_record_map_.at(node_id);
    record->frame_index = frame_index_;
    record->loaded = true;
    // Latest image heap differs from largest image heap in that the former
    // pushes a record when an image is loaded while the latter pushes when an
    // image is attached to DOM. This causes last image paint to base its order
    // on load time other than attachment time.
    // Mind that first_paint_index has to be assigned at the push of
    // time_ordered_set_ since it's the sorting key.
    record->first_paint_index = ++first_paint_index_max_;
    time_ordered_set_.insert(record->AsWeakPtr());
  }
}

void ImagePaintTimingDetector::StopRecordEntries() {
  is_recording_ = false;
}

ImageRecord* ImagePaintTimingDetector::FindLargestPaintCandidate() {
  return FindCandidate(size_ordered_set_);
}

ImageRecord* ImagePaintTimingDetector::FindLastPaintCandidate() {
  return FindCandidate(time_ordered_set_);
}

ImageRecord* ImagePaintTimingDetector::FindCandidate(
    std::set<base::WeakPtr<ImageRecord>,
             bool (*)(const base::WeakPtr<ImageRecord>&,
                      const base::WeakPtr<ImageRecord>&)>& ordered_set) {
  for (auto it = ordered_set.begin(); it != ordered_set.end(); ++it) {
    if (detached_ids_.Contains((*it)->node_id))
      continue;
    DCHECK(id_record_map_.Contains((*it)->node_id));
    // If the largest/latest image is still loading, we report nothing and come
    // back later to see if the largest/latest image at that time has finished
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
