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
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

PaintTimingDetector::PaintTimingDetector(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      text_paint_timing_detector_(
          MakeGarbageCollected<TextPaintTimingDetector>(frame_view)),
      image_paint_timing_detector_(
          MakeGarbageCollected<ImagePaintTimingDetector>(frame_view)) {}

void PaintTimingDetector::NotifyPrePaintFinished() {
  text_paint_timing_detector_->OnPrePaintFinished();
  image_paint_timing_detector_->OnPrePaintFinished();
}

void PaintTimingDetector::NotifyObjectPrePaint(
    const LayoutObject& object,
    const PaintLayer& painting_layer) {
  // Todo(maxlg): incoperate iframe's statistics
  if (!frame_view_->GetFrame().IsMainFrame())
    return;

  if (object.IsLayoutImage() || object.IsVideo() || object.IsSVGImage() ||
      ImagePaintTimingDetector::HasContentfulBackgroundImage(object)) {
    image_paint_timing_detector_->RecordImage(object, painting_layer);
  }
  // Todo(maxlg): add other detectors here.
}

void PaintTimingDetector::NotifyTextPaint(
    const Node* node,
    const PropertyTreeState& current_paint_chunk_properties) {
  if (!node)
    return;
  LayoutObject* object = node->GetLayoutObject();
  if (!object)
    return;
  NotifyTextPaint(*object, current_paint_chunk_properties);
}

void PaintTimingDetector::NotifyTextPaint(
    const LayoutObject& object,
    const PropertyTreeState& current_paint_chunk_properties) {
  LocalFrameView* frame_view = object.GetFrameView();
  if (!frame_view || !frame_view->GetFrame().IsMainFrame())
    return;
  PaintTimingDetector& detector = frame_view->GetPaintTimingDetector();
  detector.GetTextPaintTimingDetector().RecordText(
      object, current_paint_chunk_properties);
}

void PaintTimingDetector::NotifyNodeRemoved(const LayoutObject& object) {
  if (!object.GetNode())
    return;
  text_paint_timing_detector_->NotifyNodeRemoved(
      DOMNodeIds::IdForNode(object.GetNode()));
  image_paint_timing_detector_->NotifyNodeRemoved(
      DOMNodeIds::IdForNode(object.GetNode()));
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
    const LayoutRect& invalidated_rect,
    const PaintLayer& painting_layer) const {
  return CalculateVisualSize(invalidated_rect, painting_layer.GetLayoutObject()
                                                   .FirstFragment()
                                                   .LocalBorderBoxProperties());
}

uint64_t PaintTimingDetector::CalculateVisualSize(
    const LayoutRect& invalidated_rect,
    const PropertyTreeState& current_paint_chunk_properties) const {
  // This case should be dealt with outside the function.
  DCHECK(!invalidated_rect.IsEmpty());

  // As Layout objects live in different transform spaces, the object's rect
  // should be projected to the viewport's transform space.
  FloatClipRect visual_rect = FloatClipRect(FloatRect(invalidated_rect));
  GeometryMapper::LocalToAncestorVisualRect(
      current_paint_chunk_properties, PropertyTreeState::Root(), visual_rect);
  FloatRect& visual_rect_float = visual_rect.Rect();

  // A visual rect means the part of the rect that's visible within
  // the viewport. We define the size of it as visual size.
  ScrollableArea* scrollable_area = frame_view_->GetScrollableArea();
  DCHECK(scrollable_area);
  IntRect viewport = scrollable_area->VisibleContentRect();
  // Use saturated rect to avoid integer-overflow.

  visual_rect_float.Intersect(SaturatedRect(viewport));
  return visual_rect_float.Size().Area();
}

void PaintTimingDetector::Dispose() {
  text_paint_timing_detector_->Dispose();
}

void PaintTimingDetector::Trace(Visitor* visitor) {
  visitor->Trace(text_paint_timing_detector_);
  visitor->Trace(image_paint_timing_detector_);
  visitor->Trace(frame_view_);
}
}  // namespace blink
