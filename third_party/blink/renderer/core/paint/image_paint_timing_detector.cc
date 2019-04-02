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

bool IsLoaded(const LayoutObject& object) {
  if (object.IsLayoutImage()) {
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
  DCHECK(ImagePaintTimingDetector::HasBackgroundImage(object));
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
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different nodes with the same |first_size| wouldn't
  // be merged in the set.
  return a->node_id > b->node_id;
}

ImagePaintTimingDetector::ImagePaintTimingDetector(LocalFrameView* frame_view)
    : size_ordered_set_(&LargeImageFirst), frame_view_(frame_view) {}

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
  PopulateTraceValue(*value, *largest_image_record,
                     ++largest_image_candidate_index_max_);
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
  ImageRecord* largest_image_record = FindLargestPaintCandidate();
  if (largest_image_record && !largest_image_record->paint_time.is_null() &&
      largest_image_record != largest_image_paint_) {
    OnLargestImagePaintDetected(largest_image_record);
    frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
  }
}

void ImagePaintTimingDetector::OnPaintFinished() {
  frame_index_++;
  if (images_queued_for_paint_time_.empty())
    return;
  // If the last frame index of queue has changed, it means there are new
  // records pending timing.
  base::WeakPtr<ImageRecord>& record = images_queued_for_paint_time_.back();
  // As we never remove nodes from |visible_node_map_|, all of
  // |visible_node_map_| must exist.
  DCHECK(visible_node_map_.Contains(record->node_id));
  unsigned new_frame_index = record->frame_index;
  DCHECK(last_queued_frame_index_ <= new_frame_index);
  if (last_queued_frame_index_ == new_frame_index)
    return;

  last_queued_frame_index_ = new_frame_index;
  // Records with frame index up to |last_queued_frame_index_| will be
  // queued for swap time.
  RegisterNotifySwapTime();
}

void ImagePaintTimingDetector::NotifyNodeRemoved(DOMNodeId node_id) {
  if (!is_recording_)
    return;
  if (visible_node_map_.Contains(node_id)) {
    // We assume that the removed node's id wouldn't be recycled, so we don't
    // bother to remove these records from |size_ordered_set_| to reduce
    // computation.
    detached_ids_.insert(node_id);

    if (visible_node_map_.size() - detached_ids_.size() == 0) {
      // If |largest_image_paint_| will change to nullptr, update
      // performance timing.
      if (!largest_image_paint_)
        return;
      largest_image_paint_ = nullptr;
      frame_view_->GetPaintTimingDetector().DidChangePerformanceTiming();
    }
  }
}

void ImagePaintTimingDetector::RegisterNotifySwapTime() {
  WebLayerTreeView::ReportTimeCallback callback =
      WTF::Bind(&ImagePaintTimingDetector::ReportSwapTime,
                WrapWeakPersistent(this), last_queued_frame_index_);
  if (notify_swap_time_override_for_testing_) {
    // Run is not to run the |callback|, but to queue it.
    notify_swap_time_override_for_testing_.Run(std::move(callback));
    return;
  }
  // |ReportSwapTime| on |layerTreeView| will queue a swap-promise, the callback
  // is called when the swap for current render frame completes or fails to
  // happen.
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
    unsigned last_queued_frame_index,
    WebLayerTreeView::SwapResult result,
    base::TimeTicks timestamp) {
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  DCHECK(!images_queued_for_paint_time_.empty());
  while (!images_queued_for_paint_time_.empty()) {
    base::WeakPtr<ImageRecord>& record = images_queued_for_paint_time_.front();
    DCHECK(visible_node_map_.Contains(record->node_id));
    if (record->frame_index > last_queued_frame_index)
      break;
    record->paint_time = timestamp;
    images_queued_for_paint_time_.pop();
  }
  Analyze();
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
    Image* image,
    const PropertyTreeState& current_paint_chunk_properties) {
  // TODO(crbug.com/933479): Use LayoutObject::GeneratingNode() to include
  // anonymous objects' rect.
  Node* node = object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  DCHECK_NE(node_id, kInvalidDOMNodeId);
  if (invisible_node_ids_.Contains(node_id))
    return;
  // The node is reattached.
  if (visible_node_map_.Contains(node_id) && detached_ids_.Contains(node_id))
    detached_ids_.erase(node_id);

  if (!visible_node_map_.Contains(node_id) && is_recording_) {
    LayoutRect visual_rect = object.FragmentsVisualRectBoundingBox();
    // Before the image resource is loaded, <img> has size 0, so we do not
    // record the first size until the invalidated rect's size becomes
    // non-empty.
    if (visual_rect.IsEmpty())
      return;
    uint64_t rect_size =
        frame_view_->GetPaintTimingDetector().CalculateVisualSize(
            visual_rect, current_paint_chunk_properties);
    rect_size = DownScaleIfIntrinsicSizeIsSmaller(
        rect_size, image->Size().Area(),
        (visual_rect.Width() * visual_rect.Height()).ToUnsigned());

    DVLOG(2) << "Node id (" << node_id << "): size=" << rect_size
             << ", type=" << object.DebugName();
    if (rect_size == 0) {
      // When rect_size == 0, it either means the image is size 0 or the image
      // is out of viewport. Either way, we don't track this image anymore, to
      // reduce computation.
      invisible_node_ids_.insert(node_id);
      return;
    }
    // Non-trivial image is found.
    std::unique_ptr<ImageRecord> record = std::make_unique<ImageRecord>();
    record->node_id = node_id;
#ifndef NDEBUG
    record->image_url = GetImageUrl(object);
#endif
    // Mind that first_size has to be assigned at the push of
    // |size_ordered_set_| since it's the sorting key.
    record->first_size = rect_size;
    size_ordered_set_.emplace(record->AsWeakPtr());
    visible_node_map_.insert(node_id, std::move(record));
    if (visible_node_map_.size() + invisible_node_ids_.size() >
        kImageNodeNumberLimit) {
      TRACE_EVENT_INSTANT2("loading", "ImagePaintTimingDetector::OverNodeLimit",
                           TRACE_EVENT_SCOPE_THREAD, "recorded_node_count",
                           visible_node_map_.size(), "size_zero_node_count",
                           invisible_node_ids_.size());
      StopRecordEntries();
    }
  }
  if (visible_node_map_.Contains(node_id) &&
      !visible_node_map_.at(node_id)->loaded && IsLoaded(object)) {
    // The image is just loaded.
    DCHECK(visible_node_map_.Contains(node_id));
    std::unique_ptr<ImageRecord>& record =
        visible_node_map_.find(node_id)->value;
    images_queued_for_paint_time_.emplace(record->AsWeakPtr());
    record->frame_index = frame_index_;
    record->loaded = true;
  }
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

ImageRecord* ImagePaintTimingDetector::FindLargestPaintCandidate() {
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
