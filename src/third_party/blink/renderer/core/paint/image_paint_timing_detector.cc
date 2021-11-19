// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"

#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
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
// * |visual_size| refers to the size of the |displayed_image_size| after
// clipping and transforming. The size is in the main-frame's coordinate.
// * |intrinsic_image_size| refers to the the image object's original size
// before scaling. The size is in the image object's coordinate.
// * |displayed_image_size| refers to the paint size in the image object's
// coordinate.
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

bool ShouldReportAnimatedImages() {
  return (RuntimeEnabledFeatures::LCPAnimatedImagesWebExposedEnabled() ||
          base::FeatureList::IsEnabled(features::kLCPAnimatedImagesReporting));
}

}  // namespace

static bool LargeImageFirst(const base::WeakPtr<ImageRecord>& a,
                            const base::WeakPtr<ImageRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different |ImageRecord|s with the same |first_size|
  // wouldn't be merged in the |size_ordered_set_|.
  return a->insertion_index < b->insertion_index;
}

ImagePaintTimingDetector::ImagePaintTimingDetector(
    LocalFrameView* frame_view,
    PaintTimingCallbackManager* callback_manager)
    : uses_page_viewport_(
          base::FeatureList::IsEnabled(features::kUsePageViewportInLCP)),
      records_manager_(frame_view),
      frame_view_(frame_view),
      callback_manager_(callback_manager) {}

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
  if (first_image_paint.lcp_rect_info_) {
    first_image_paint.lcp_rect_info_->OutputToTraceValue(value);
  }
}

void ImagePaintTimingDetector::ReportCandidateToTrace(
    ImageRecord& largest_image_record) {
  if (!PaintTimingDetector::IsTracing())
    return;
  DCHECK(!largest_image_record.paint_time.is_null());
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_image_record);
  // TODO(yoav): Report first animated frame times as well.
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

ImageRecord* ImagePaintTimingDetector::UpdateCandidate() {
  ImageRecord* largest_image_record =
      records_manager_.FindLargestPaintCandidate();
  base::TimeTicks time = largest_image_record ? largest_image_record->paint_time
                                              : base::TimeTicks();
  // This doesn't use ShouldReportAnimatedImages(), as it should only update the
  // record when the base::Feature is enabled, regardless of the runtime-enabled
  // flag.
  if (base::FeatureList::IsEnabled(features::kLCPAnimatedImagesReporting) &&
      largest_image_record &&
      !largest_image_record->first_animated_frame_time.is_null()) {
    time = largest_image_record->first_animated_frame_time;
  }
  const uint64_t size =
      largest_image_record ? largest_image_record->first_size : 0;
  PaintTimingDetector& detector = frame_view_->GetPaintTimingDetector();
  // Calling NotifyIfChangedLargestImagePaint only has an impact on
  // PageLoadMetrics, and not on the web exposed metrics.
  //
  // Two different candidates are rare to have the same time and size.
  // So when they are unchanged, the candidate is considered unchanged.
  bool changed = detector.NotifyIfChangedLargestImagePaint(
      time, size, records_manager_.LargestRemovedImagePaintTime(),
      records_manager_.LargestRemovedImageSize());
  if (changed) {
    if (!time.is_null() && largest_image_record->loaded) {
      ReportCandidateToTrace(*largest_image_record);
    } else {
      ReportNoCandidateToTrace();
    }
  }
  return largest_image_record;
}

void ImagePaintTimingDetector::OnPaintFinished() {
  frame_index_++;
  viewport_size_ = absl::nullopt;
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    frame_view_->GetPaintTimingDetector()
        .UpdateLargestContentfulPaintCandidate();
  }

  if (!records_manager_.HasUnregisteredRecordsInQueue(
          last_registered_frame_index_))
    return;

  last_registered_frame_index_ = frame_index_ - 1;
  RegisterNotifyPresentationTime();
}

void ImagePaintTimingDetector::NotifyImageRemoved(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  RecordId record_id = std::make_pair(&object, cached_image);
  records_manager_.RemoveImageTimeRecords(record_id);
  records_manager_.RemoveInvisibleRecordIfNeeded(record_id);
  if (!records_manager_.IsRecordedVisibleImage(record_id))
    return;
  records_manager_.RemoveVisibleRecord(record_id);
  need_update_timing_at_frame_end_ = true;
}

void ImagePaintTimingDetector::StopRecordEntries() {
  // Clear the records queued for presentation callback to ensure no new updates
  // occur.
  records_manager_.ClearImagesQueuedForPaintTime();
  if (frame_view_->GetFrame().IsMainFrame()) {
    DCHECK(frame_view_->GetFrame().GetDocument());
    ukm::builders::Blink_PaintTiming(
        frame_view_->GetFrame().GetDocument()->UkmSourceID())
        .SetLCPDebugging_HasViewportImage(contains_full_viewport_image_)
        .Record(ukm::UkmRecorder::Get());
  }
}

void ImagePaintTimingDetector::RegisterNotifyPresentationTime() {
  auto callback = WTF::Bind(&ImagePaintTimingDetector::ReportPresentationTime,
                            WrapCrossThreadWeakPersistent(this),
                            last_registered_frame_index_);
  callback_manager_->RegisterCallback(std::move(callback));
}

void ImagePaintTimingDetector::ReportPresentationTime(
    unsigned last_queued_frame_index,
    base::TimeTicks timestamp) {
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  records_manager_.AssignPaintTimeToRegisteredQueuedRecords(
      timestamp, last_queued_frame_index);
}

void ImageRecordsManager::AssignPaintTimeToRegisteredQueuedRecords(
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
    if (record->loaded) {
      record->paint_time = timestamp;
    }
    if (record->queue_animated_paint) {
      record->first_animated_frame_time = timestamp;
      record->queue_animated_paint = false;
    }
    images_queued_for_paint_time_.pop_front();
  }
}

void ImagePaintTimingDetector::RecordImage(
    const LayoutObject& object,
    const gfx::Size& intrinsic_size,
    const ImageResourceContent& cached_image,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const StyleFetchedImage* style_image,
    const gfx::Rect& image_border) {
  Node* node = object.GetNode();

  if (!node)
    return;

  // Before the image resource starts loading, <img> has no size info. We wait
  // until the size is known.
  if (image_border.IsEmpty())
    return;

  RecordId record_id = std::make_pair(&object, &cached_image);

  if (!base::FeatureList::IsEnabled(
          features::kIncludeInitiallyInvisibleImagesInLCP) &&
      records_manager_.IsRecordedInvisibleImage(record_id)) {
    // Ignore images that are initially invisible, even if they later become
    // visible. This is done as an optimization, to reduce LCP calculation
    // costs.
    // Note that this results in correctness issues:
    // https://crbug.com/1249622
    return;
  }
  bool is_recorded_visible_image =
      records_manager_.IsRecordedVisibleImage(record_id);

  if (int depth = IgnorePaintTimingScope::IgnoreDepth()) {
    // Record the largest loaded image that is hidden due to documentElement
    // being invisible but by no other reason (i.e. IgnoreDepth() needs to be
    // 1).
    if (depth == 1 && IgnorePaintTimingScope::IsDocumentElementInvisible() &&
        !is_recorded_visible_image && cached_image.IsLoaded()) {
      gfx::RectF mapped_visual_rect =
          frame_view_->GetPaintTimingDetector().CalculateVisualRect(
              image_border, current_paint_chunk_properties);
      uint64_t rect_size = ComputeImageRectSize(
          image_border, mapped_visual_rect, intrinsic_size,
          current_paint_chunk_properties, object, cached_image);
      records_manager_.MaybeUpdateLargestIgnoredImage(
          record_id, rect_size, image_border, mapped_visual_rect);
    }
    return;
  }

  if (is_recorded_visible_image) {
    if (ShouldReportAnimatedImages() &&
        cached_image.IsAnimatedImageWithPaintedFirstFrame()) {
      need_update_timing_at_frame_end_ |=
          records_manager_.OnFirstAnimatedFramePainted(record_id, frame_index_);
    }
    if (!records_manager_.IsVisibleImageLoaded(record_id) &&
        cached_image.IsLoaded()) {
      records_manager_.OnImageLoaded(record_id, frame_index_, style_image);
      need_update_timing_at_frame_end_ = true;
      if (absl::optional<PaintTimingVisualizer>& visualizer =
              frame_view_->GetPaintTimingDetector().Visualizer()) {
        gfx::RectF mapped_visual_rect =
            frame_view_->GetPaintTimingDetector().CalculateVisualRect(
                image_border, current_paint_chunk_properties);
        visualizer->DumpImageDebuggingRect(object, mapped_visual_rect,
                                           cached_image);
      }
    }
    return;
  }

  gfx::RectF mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          image_border, current_paint_chunk_properties);
  uint64_t rect_size = ComputeImageRectSize(
      image_border, mapped_visual_rect, intrinsic_size,
      current_paint_chunk_properties, object, cached_image);
  if (rect_size == 0) {
    records_manager_.RecordInvisible(record_id);
  } else {
    records_manager_.RecordVisible(record_id, rect_size, image_border,
                                   mapped_visual_rect);
    if (ShouldReportAnimatedImages() &&
        cached_image.IsAnimatedImageWithPaintedFirstFrame()) {
      need_update_timing_at_frame_end_ |=
          records_manager_.OnFirstAnimatedFramePainted(record_id, frame_index_);
    }
    if (cached_image.IsLoaded()) {
      records_manager_.OnImageLoaded(record_id, frame_index_, style_image);
      need_update_timing_at_frame_end_ = true;
    }
  }
}

uint64_t ImagePaintTimingDetector::ComputeImageRectSize(
    const gfx::Rect& image_border,
    const gfx::RectF& mapped_visual_rect,
    const gfx::Size& intrinsic_size,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const LayoutObject& object,
    const ImageResourceContent& cached_image) {
  if (absl::optional<PaintTimingVisualizer>& visualizer =
          frame_view_->GetPaintTimingDetector().Visualizer()) {
    visualizer->DumpImageDebuggingRect(object, mapped_visual_rect,
                                       cached_image);
  }
  uint64_t rect_size = mapped_visual_rect.size().GetArea();
  // Transform visual rect to window before calling downscale.
  gfx::RectF float_visual_rect =
      frame_view_->GetPaintTimingDetector().BlinkSpaceToDIPs(
          gfx::RectF(image_border));
  if (!viewport_size_.has_value()) {
    // If the flag to use page viewport is enabled, we use the page viewport
    // (aka the main frame viewport) for all frames, including iframes. This
    // prevents us from discarding images with size equal to the size of its
    // embedding iframe.
    gfx::Rect viewport_int_rect = ToGfxRect(
        uses_page_viewport_
            ? frame_view_->GetPage()->GetVisualViewport().VisibleContentRect()
            : frame_view_->GetScrollableArea()->VisibleContentRect());
    gfx::RectF viewport =
        frame_view_->GetPaintTimingDetector().BlinkSpaceToDIPs(
            gfx::RectF(viewport_int_rect));
    viewport_size_ = viewport.size().GetArea();
  }
  // An SVG image size is computed with respect to the virtual viewport of the
  // SVG, so |rect_size| can be larger than |*viewport_size| in edge cases. If
  // the rect occupies the whole viewport, disregard this candidate by saying
  // the size is 0.
  if (rect_size >= *viewport_size_) {
    contains_full_viewport_image_ = true;
    return 0;
  }

  rect_size = DownScaleIfIntrinsicSizeIsSmaller(
      rect_size, intrinsic_size.Area64(), float_visual_rect.size().GetArea());
  return rect_size;
}

void ImagePaintTimingDetector::NotifyImageFinished(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  RecordId record_id = std::make_pair(&object, cached_image);
  records_manager_.NotifyImageFinished(record_id);
}

void ImagePaintTimingDetector::ReportLargestIgnoredImage() {
  need_update_timing_at_frame_end_ = true;
  records_manager_.ReportLargestIgnoredImage(frame_index_);
}

ImageRecordsManager::ImageRecordsManager(LocalFrameView* frame_view)
    : size_ordered_set_(&LargeImageFirst), frame_view_(frame_view) {}

bool ImageRecordsManager::OnFirstAnimatedFramePainted(
    const RecordId& record_id,
    unsigned current_frame_index) {
  base::WeakPtr<ImageRecord> record = FindVisibleRecord(record_id);
  DCHECK(record);
  if (record->first_animated_frame_time.is_null()) {
    record->queue_animated_paint = true;
    QueueToMeasurePaintTime(record, current_frame_index);
    return true;
  }
  return false;
}

void ImageRecordsManager::OnImageLoaded(const RecordId& record_id,
                                        unsigned current_frame_index,
                                        const StyleFetchedImage* style_image) {
  base::WeakPtr<ImageRecord> record = FindVisibleRecord(record_id);
  DCHECK(record);
  if (!style_image) {
    auto it = image_finished_times_.find(record_id);
    if (it != image_finished_times_.end()) {
      record->load_time = it->value;
      DCHECK(!record->load_time.is_null());
    }
  } else {
    Document* document = frame_view_->GetFrame().GetDocument();
    if (document && document->domWindow()) {
      record->load_time = ImageElementTiming::From(*document->domWindow())
                              .GetBackgroundImageLoadTime(style_image);
    }
  }
  OnImageLoadedInternal(record, current_frame_index);
}

void ImageRecordsManager::ReportLargestIgnoredImage(
    unsigned current_frame_index) {
  if (!largest_ignored_image_)
    return;
  base::WeakPtr<ImageRecord> record = largest_ignored_image_->AsWeakPtr();
  Node* node = DOMNodeIds::NodeForId(largest_ignored_image_->node_id);
  if (!node || !node->GetLayoutObject() ||
      !largest_ignored_image_->cached_image) {
    // The image has been removed, so we have no content to report.
    largest_ignored_image_.reset();
    return;
  }
  RecordId record_id = std::make_pair(node->GetLayoutObject(),
                                      largest_ignored_image_->cached_image);
  size_ordered_set_.insert(record);
  visible_images_.insert(record_id, std::move(largest_ignored_image_));
  OnImageLoadedInternal(record, current_frame_index);
}

void ImageRecordsManager::OnImageLoadedInternal(
    base::WeakPtr<ImageRecord>& record,
    unsigned current_frame_index) {
  SetLoaded(record);
  QueueToMeasurePaintTime(record, current_frame_index);
}

void ImageRecordsManager::MaybeUpdateLargestIgnoredImage(
    const RecordId& record_id,
    const uint64_t& visual_size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  if (visual_size && (!largest_ignored_image_ ||
                      visual_size > largest_ignored_image_->first_size)) {
    largest_ignored_image_ =
        CreateImageRecord(*record_id.first, record_id.second, visual_size,
                          frame_visual_rect, root_visual_rect);
    largest_ignored_image_->load_time = base::TimeTicks::Now();
  }
}

void ImageRecordsManager::RecordVisible(const RecordId& record_id,
                                        const uint64_t& visual_size,
                                        const gfx::Rect& frame_visual_rect,
                                        const gfx::RectF& root_visual_rect) {
  std::unique_ptr<ImageRecord> record =
      CreateImageRecord(*record_id.first, record_id.second, visual_size,
                        frame_visual_rect, root_visual_rect);
  size_ordered_set_.insert(record->AsWeakPtr());
  visible_images_.insert(record_id, std::move(record));
}

std::unique_ptr<ImageRecord> ImageRecordsManager::CreateImageRecord(
    const LayoutObject& object,
    const ImageResourceContent* cached_image,
    const uint64_t& visual_size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  DCHECK_GT(visual_size, 0u);
  Node* node = object.GetNode();
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  std::unique_ptr<ImageRecord> record = std::make_unique<ImageRecord>(
      node_id, cached_image, visual_size, frame_visual_rect, root_visual_rect);
  return record;
}

ImageRecord* ImageRecordsManager::FindLargestPaintCandidate() const {
  DCHECK_EQ(visible_images_.size(), size_ordered_set_.size());
  if (size_ordered_set_.size() == 0)
    return nullptr;
  return size_ordered_set_.begin()->get();
}

void ImageRecordsManager::ClearImagesQueuedForPaintTime() {
  images_queued_for_paint_time_.clear();
}

void ImageRecordsManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
}

void ImagePaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(records_manager_);
  visitor->Trace(frame_view_);
  visitor->Trace(callback_manager_);
}
}  // namespace blink
