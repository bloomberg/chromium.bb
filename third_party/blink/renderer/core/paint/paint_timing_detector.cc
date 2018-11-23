// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"

namespace blink {

PaintTimingDetector::PaintTimingDetector(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      text_paint_timing_detector_(new TextPaintTimingDetector(frame_view)),
      image_paint_timing_detector_(new ImagePaintTimingDetector(frame_view)){};

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

  if (object.IsText()) {
    text_paint_timing_detector_->RecordText(object, painting_layer);
  }
  if (object.IsImage() || object.IsVideo() || object.IsSVGImage() ||
      ImagePaintTimingDetector::HasContentfulBackgroundImage(object)) {
    image_paint_timing_detector_->RecordImage(object, painting_layer);
  }
  // Todo(maxlg): add other detectors here.
}

void PaintTimingDetector::NotifyNodeRemoved(const LayoutObject& object) {
  if (!object.GetNode())
    return;
  text_paint_timing_detector_->NotifyNodeRemoved(
      DOMNodeIds::IdForNode(object.GetNode()));
  image_paint_timing_detector_->NotifyNodeRemoved(
      DOMNodeIds::IdForNode(object.GetNode()));
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

unsigned PaintTimingDetector::CalculateVisualSize(
    const LayoutRect& invalidated_rect,
    const PaintLayer& painting_layer) const {
  // This case should be dealt with outside the function.
  DCHECK(!invalidated_rect.IsEmpty());

  // As Layout objects live in different transform spaces, the object's rect
  // should be projected to the viewport's transform space.
  IntRect visual_rect = EnclosedIntRect(invalidated_rect);
  painting_layer.GetLayoutObject().FirstFragment().MapRectToFragment(
      painting_layer.GetLayoutObject().View()->FirstFragment(), visual_rect);

  // A visual rect means the part of the rect that's visible within
  // the viewport. We define the size of it as visual size.
  ScrollableArea* scrollable_area = frame_view_->GetScrollableArea();
  DCHECK(scrollable_area);
  IntRect viewport = scrollable_area->VisibleContentRect();
  visual_rect.Intersect(viewport);
  return visual_rect.Height() * visual_rect.Width();
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
