// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"

#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

namespace {

// In the context of FCP++, we define contentful background image as one that
// satisfies all of the following conditions:
// * has image reources attached to style of the object, i.e.,
//  { background-image: url('example.gif') }
// * not attached to <body> or <html>
// This function contains the above heuristics.
bool IsBackgroundImageContentful(const LayoutObject& object,
                                 const Image& image) {
  // Background images attached to <body> or <html> are likely for background
  // purpose, so we rule them out.
  if (object.IsLayoutView() || object.IsBody() || object.IsDocumentElement()) {
    return false;
  }
  // Generated images are excluded here, as they are likely to serve for
  // background purpose.
  if (!image.IsBitmapImage() && !image.IsStaticBitmapImage() &&
      !image.IsSVGImage() && !image.IsPlaceholderImage())
    return false;
  return true;
}

}  // namespace

PaintTimingDetector::PaintTimingDetector(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      text_paint_timing_detector_(
          MakeGarbageCollected<TextPaintTimingDetector>(frame_view)),
      image_paint_timing_detector_(
          MakeGarbageCollected<ImagePaintTimingDetector>(frame_view)) {}

void PaintTimingDetector::NotifyPaintFinished() {
  if (text_paint_timing_detector_) {
    text_paint_timing_detector_->OnPaintFinished();
    if (text_paint_timing_detector_->FinishedReportingText())
      text_paint_timing_detector_ = nullptr;
  }
  if (image_paint_timing_detector_) {
    image_paint_timing_detector_->OnPaintFinished();
    if (image_paint_timing_detector_->FinishedReportingImages())
      image_paint_timing_detector_ = nullptr;
  }
}

// static
void PaintTimingDetector::NotifyBackgroundImagePaint(
    const Node* node,
    const Image* image,
    const ImageResourceContent* cached_image,
    const PropertyTreeState& current_paint_chunk_properties) {
  DCHECK(image);
  DCHECK(cached_image);
  if (!node)
    return;
  LayoutObject* object = node->GetLayoutObject();
  if (!object)
    return;
  LocalFrameView* frame_view = object->GetFrameView();
  if (!frame_view)
    return;
  PaintTimingDetector& detector = frame_view->GetPaintTimingDetector();
  if (!detector.GetImagePaintTimingDetector())
    return;
  if (!IsBackgroundImageContentful(*object, *image))
    return;
  detector.GetImagePaintTimingDetector()->RecordBackgroundImage(
      *object, image->Size(), *cached_image, current_paint_chunk_properties);
}

// static
void PaintTimingDetector::NotifyImagePaint(
    const LayoutObject& object,
    const IntSize& intrinsic_size,
    const ImageResourceContent* cached_image,
    const PropertyTreeState& current_paint_chunk_properties) {
  LocalFrameView* frame_view = object.GetFrameView();
  if (!frame_view)
    return;
  if (!cached_image)
    return;
  PaintTimingDetector& detector = frame_view->GetPaintTimingDetector();
  if (detector.GetImagePaintTimingDetector()) {
    detector.GetImagePaintTimingDetector()->RecordImage(
        object, intrinsic_size, *cached_image, current_paint_chunk_properties);
  }
}

void PaintTimingDetector::NotifyNodeRemoved(const LayoutObject& object) {
  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(object.GetNode());
  if (node_id == kInvalidDOMNodeId)
    return;

  if (text_paint_timing_detector_)
    text_paint_timing_detector_->NotifyNodeRemoved(node_id);
  if (image_paint_timing_detector_)
    image_paint_timing_detector_->NotifyNodeRemoved(node_id);
}

void PaintTimingDetector::NotifyBackgroundImageRemoved(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(object.GetNode());
  if (node_id == kInvalidDOMNodeId)
    return;
  if (image_paint_timing_detector_) {
    image_paint_timing_detector_->NotifyBackgroundImageRemoved(node_id,
                                                               cached_image);
  }
}

void PaintTimingDetector::NotifyInputEvent(WebInputEvent::Type type) {
  if (type == WebInputEvent::kMouseMove || type == WebInputEvent::kMouseEnter ||
      type == WebInputEvent::kMouseLeave ||
      WebInputEvent::IsPinchGestureEventType(type)) {
    return;
  }
  DCHECK(frame_view_);
  if (text_paint_timing_detector_) {
    text_paint_timing_detector_->StopRecordingLargestTextPaint();
    if (!RuntimeEnabledFeatures::ElementTimingEnabled(
            frame_view_->GetFrame().GetDocument())) {
      text_paint_timing_detector_->StopRecordEntries();
    }
  }
  if (image_paint_timing_detector_)
    image_paint_timing_detector_->StopRecordEntries();
}

void PaintTimingDetector::NotifyScroll(ScrollType scroll_type) {
  if (scroll_type != kUserScroll && scroll_type != kCompositorScroll)
    return;
  DCHECK(frame_view_);
  if (text_paint_timing_detector_) {
    text_paint_timing_detector_->StopRecordingLargestTextPaint();
    if (!RuntimeEnabledFeatures::ElementTimingEnabled(
            frame_view_->GetFrame().GetDocument())) {
      text_paint_timing_detector_->StopRecordEntries();
    }
  }
  if (image_paint_timing_detector_)
    image_paint_timing_detector_->StopRecordEntries();
}

bool PaintTimingDetector::NeedToNotifyInputOrScroll() const {
  return (text_paint_timing_detector_ &&
          text_paint_timing_detector_->IsRecording()) ||
         (image_paint_timing_detector_ &&
          image_paint_timing_detector_->IsRecording());
}

bool PaintTimingDetector::NotifyIfChangedLargestImagePaint(
    base::TimeTicks image_paint_time,
    uint64_t image_paint_size) {
  if (!HasLargestImagePaintChanged(image_paint_time, image_paint_size))
    return false;
  largest_image_paint_time_ = image_paint_time;
  largest_image_paint_size_ = image_paint_size;
  DidChangePerformanceTiming();
  return true;
}

bool PaintTimingDetector::NotifyIfChangedLargestTextPaint(
    base::TimeTicks text_paint_time,
    uint64_t text_paint_size) {
  if (!HasLargestTextPaintChanged(text_paint_time, text_paint_size))
    return false;
  largest_text_paint_time_ = text_paint_time;
  largest_text_paint_size_ = text_paint_size;
  DidChangePerformanceTiming();
  return true;
}

bool PaintTimingDetector::HasLargestImagePaintChanged(
    base::TimeTicks largest_image_paint_time,
    uint64_t largest_image_paint_size) const {
  return largest_image_paint_time != largest_image_paint_time_ ||
         largest_image_paint_size != largest_image_paint_size_;
}

bool PaintTimingDetector::HasLargestTextPaintChanged(
    base::TimeTicks largest_text_paint_time,
    uint64_t largest_text_paint_size) const {
  return largest_text_paint_time != largest_text_paint_time_ ||
         largest_text_paint_size != largest_text_paint_size_;
}

void PaintTimingDetector::DidChangePerformanceTiming() {
  Document* document = frame_view_->GetFrame().GetDocument();
  if (!document)
    return;
  DocumentLoader* loader = document->Loader();
  if (!loader)
    return;
  loader->DidChangePerformanceTiming();
}

FloatRect PaintTimingDetector::CalculateVisualRect(
    const IntRect& visual_rect,
    const PropertyTreeState& current_paint_chunk_properties) const {
  // This case should be dealt with outside the function.
  DCHECK(!visual_rect.IsEmpty());

  // As Layout objects live in different transform spaces, the object's rect
  // should be projected to the viewport's transform space.
  FloatClipRect float_clip_visual_rect = FloatClipRect(FloatRect(visual_rect));
  GeometryMapper::LocalToAncestorVisualRect(current_paint_chunk_properties,
                                            PropertyTreeState::Root(),
                                            float_clip_visual_rect);
  FloatRect& float_visual_rect = float_clip_visual_rect.Rect();
  if (frame_view_->GetFrame().LocalFrameRoot().IsMainFrame())
    return float_visual_rect;
  // OOPIF. The final rect lives in the iframe's root frame space. We need to
  // project it to the top frame space.
  auto layout_visual_rect = PhysicalRect::EnclosingRect(float_visual_rect);
  frame_view_->GetFrame()
      .LocalFrameRoot()
      .View()
      ->MapToVisualRectInTopFrameSpace(layout_visual_rect);
  return FloatRect(layout_visual_rect);
}

ScopedPaintTimingDetectorBlockPaintHook*
    ScopedPaintTimingDetectorBlockPaintHook::top_ = nullptr;

void ScopedPaintTimingDetectorBlockPaintHook::EmplaceIfNeeded(
    const LayoutBoxModelObject& aggregator,
    const PropertyTreeState& property_tree_state) {
  TextPaintTimingDetector* detector = aggregator.GetFrameView()
                                          ->GetPaintTimingDetector()
                                          .GetTextPaintTimingDetector();
  if (!detector || !detector->ShouldWalkObject(aggregator))
    return;
  data_.emplace(aggregator, property_tree_state, detector);
}

ScopedPaintTimingDetectorBlockPaintHook::Data::Data(
    const LayoutBoxModelObject& aggregator,
    const PropertyTreeState& property_tree_state,
    TextPaintTimingDetector* detector)
    : aggregator_(aggregator),
      property_tree_state_(property_tree_state),
      detector_(detector) {}

ScopedPaintTimingDetectorBlockPaintHook::
    ~ScopedPaintTimingDetectorBlockPaintHook() {
  DCHECK_EQ(top_, this);
  if (!data_ || data_->aggregated_visual_rect_.IsEmpty())
    return;
  data_->detector_->RecordAggregatedText(data_->aggregator_,
                                         data_->aggregated_visual_rect_,
                                         data_->property_tree_state_);
}

void PaintTimingDetector::Trace(Visitor* visitor) {
  visitor->Trace(text_paint_timing_detector_);
  visitor->Trace(image_paint_timing_detector_);
  visitor->Trace(frame_view_);
}

}  // namespace blink
