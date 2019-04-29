// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
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
  text_paint_timing_detector_->OnPaintFinished();
  image_paint_timing_detector_->OnPaintFinished();
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
  if (!IsBackgroundImageContentful(*object, *image))
    return;
  LocalFrameView* frame_view = object->GetFrameView();
  if (!frame_view)
    return;
  if (!cached_image)
    return;
  PaintTimingDetector& detector = frame_view->GetPaintTimingDetector();
  detector.GetImagePaintTimingDetector().RecordBackgroundImage(
      *object, image->Size(), cached_image, current_paint_chunk_properties);
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
  detector.GetImagePaintTimingDetector().RecordImage(
      object, intrinsic_size, cached_image, current_paint_chunk_properties);
}

// static
void PaintTimingDetector::NotifyTextPaint(
    const LayoutObject& object,
    const PropertyTreeState& current_paint_chunk_properties) {
  LocalFrameView* frame_view = object.GetFrameView();
  if (!frame_view)
    return;
  PaintTimingDetector& detector = frame_view->GetPaintTimingDetector();
  detector.GetTextPaintTimingDetector().RecordText(
      object, current_paint_chunk_properties);
}

void PaintTimingDetector::NotifyNodeRemoved(const LayoutObject& object) {
  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(object.GetNode());
  if (node_id == kInvalidDOMNodeId)
    return;
  text_paint_timing_detector_->NotifyNodeRemoved(node_id);
  image_paint_timing_detector_->NotifyNodeRemoved(node_id);
}

void PaintTimingDetector::NotifyBackgroundImageRemoved(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(object.GetNode());
  if (node_id == kInvalidDOMNodeId)
    return;
  image_paint_timing_detector_->NotifyBackgroundImageRemoved(node_id,
                                                             cached_image);
}

void PaintTimingDetector::NotifyInputEvent(WebInputEvent::Type type) {
  if (type == WebInputEvent::kMouseMove || type == WebInputEvent::kMouseEnter ||
      type == WebInputEvent::kMouseLeave ||
      WebInputEvent::IsPinchGestureEventType(type)) {
    return;
  }
  text_paint_timing_detector_->StopRecordEntries();
  image_paint_timing_detector_->StopRecordEntries();
}

void PaintTimingDetector::NotifyScroll(ScrollType scroll_type) {
  if (scroll_type != kUserScroll && scroll_type != kCompositorScroll)
    return;
  text_paint_timing_detector_->StopRecordEntries();
  image_paint_timing_detector_->StopRecordEntries();
}

bool PaintTimingDetector::NeedToNotifyInputOrScroll() {
  return text_paint_timing_detector_->IsRecording() ||
         image_paint_timing_detector_->IsRecording();
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

uint64_t PaintTimingDetector::CalculateVisualSize(
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
    return float_visual_rect.Size().Area();
  // OOPIF. The final rect lives in the iframe's root frame space. We need to
  // project it to the top frame space.
  LayoutRect layout_visual_rect(float_visual_rect);
  frame_view_->GetFrame()
      .LocalFrameRoot()
      .View()
      ->MapToVisualRectInTopFrameSpace(layout_visual_rect);
  return (layout_visual_rect.Size().Width() *
          layout_visual_rect.Size().Height())
      .ToUnsigned();
}

void PaintTimingDetector::Trace(Visitor* visitor) {
  visitor->Trace(text_paint_timing_detector_);
  visitor->Trace(image_paint_timing_detector_);
  visitor->Trace(frame_view_);
}
}  // namespace blink
