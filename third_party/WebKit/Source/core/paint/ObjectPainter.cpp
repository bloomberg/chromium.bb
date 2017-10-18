// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ObjectPainter.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/paint/PaintInfo.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void ObjectPainter::PaintOutline(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));

  const ComputedStyle& style_to_use = layout_object_.StyleRef();
  if (!style_to_use.HasOutline() ||
      style_to_use.Visibility() != EVisibility::kVisible)
    return;

  // Only paint the focus ring by hand if the theme isn't able to draw the focus
  // ring.
  if (style_to_use.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(
          layout_object_.GetNode(), style_to_use)) {
    return;
  }

  Vector<LayoutRect> outline_rects;
  layout_object_.AddOutlineRects(
      outline_rects, paint_offset,
      layout_object_.OutlineRectsShouldIncludeBlockVisualOverflow());
  if (outline_rects.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_object_, paint_info.phase))
    return;

  // The result rects are in coordinates of m_layoutObject's border box.
  // Block flipping is not applied yet if !m_layoutObject.isBox().
  if (!layout_object_.IsBox() &&
      layout_object_.StyleRef().IsFlippedBlocksWritingMode()) {
    LayoutBlock* container = layout_object_.ContainingBlock();
    if (container) {
      layout_object_.LocalToAncestorRects(outline_rects, container,
                                          -paint_offset, paint_offset);
      if (outline_rects.IsEmpty())
        return;
    }
  }

  PaintOutlineRects(paint_info, outline_rects, style_to_use, layout_object_);
}

void ObjectPainter::PaintInlineChildrenOutlines(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintDescendantOutlines(paint_info.phase));

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  for (LayoutObject* child = layout_object_.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLayoutInline() &&
        !ToLayoutInline(child)->HasSelfPaintingLayer())
      child->Paint(paint_info_for_descendants, paint_offset);
  }
}

void ObjectPainter::AddPDFURLRectIfNeeded(const PaintInfo& paint_info,
                                          const LayoutPoint& paint_offset) {
  DCHECK(paint_info.IsPrinting());
  if (layout_object_.IsElementContinuation() || !layout_object_.GetNode() ||
      !layout_object_.GetNode()->IsLink() ||
      layout_object_.StyleRef().Visibility() != EVisibility::kVisible)
    return;

  KURL url = ToElement(layout_object_.GetNode())->HrefURL();
  if (!url.IsValid())
    return;

  Vector<LayoutRect> visual_overflow_rects;
  layout_object_.AddElementVisualOverflowRects(visual_overflow_rects,
                                               paint_offset);
  IntRect rect = PixelSnappedIntRect(UnionRect(visual_overflow_rects));
  if (rect.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_object_,
          DisplayItem::kPrintedContentPDFURLRect))
    return;

  DrawingRecorder recorder(paint_info.context, layout_object_,
                           DisplayItem::kPrintedContentPDFURLRect, rect);
  if (url.HasFragmentIdentifier() &&
      EqualIgnoringFragmentIdentifier(url,
                                      layout_object_.GetDocument().BaseURL())) {
    String fragment_name = url.FragmentIdentifier();
    if (layout_object_.GetDocument().FindAnchor(fragment_name))
      paint_info.context.SetURLFragmentForRect(fragment_name, rect);
    return;
  }
  paint_info.context.SetURLForRect(url, rect);
}

void ObjectPainter::PaintAllPhasesAtomically(const PaintInfo& paint_info,
                                             const LayoutPoint& paint_offset) {
  // Pass PaintPhaseSelection and PaintPhaseTextClip to the descendants so that
  // they will paint for selection and text clip respectively. We don't need
  // complete painting for these phases.
  if (paint_info.phase == PaintPhase::kSelection ||
      paint_info.phase == PaintPhase::kTextClip) {
    layout_object_.Paint(paint_info, paint_offset);
    return;
  }

  if (paint_info.phase != PaintPhase::kForeground)
    return;

  PaintInfo info(paint_info);
  info.phase = PaintPhase::kBlockBackground;
  layout_object_.Paint(info, paint_offset);
  info.phase = PaintPhase::kFloat;
  layout_object_.Paint(info, paint_offset);
  info.phase = PaintPhase::kForeground;
  layout_object_.Paint(info, paint_offset);
  info.phase = PaintPhase::kOutline;
  layout_object_.Paint(info, paint_offset);
}

#if DCHECK_IS_ON()
void ObjectPainter::DoCheckPaintOffset(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

  // TODO(pdr): Let painter and paint property tree builder generate the same
  // paint offset for LayoutScrollbarPart. crbug.com/664249.
  if (layout_object_.IsLayoutScrollbarPart())
    return;

  LayoutPoint adjusted_paint_offset = paint_offset;
  if (layout_object_.IsBox())
    adjusted_paint_offset += ToLayoutBox(layout_object_).Location();
  DCHECK(layout_object_.PaintOffset() == adjusted_paint_offset)
      << " Paint offset mismatch: " << layout_object_.DebugName()
      << " from PaintPropertyTreeBuilder: "
      << layout_object_.PaintOffset().ToString()
      << " from painter: " << adjusted_paint_offset.ToString();
}
#endif

}  // namespace blink
