// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ng/ng_fragment_painter.h"

#include "core/layout/LayoutTheme.h"
#include "core/layout/ng/inline/ng_inline_fragment_traversal.h"
#include "core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/ng/ng_paint_fragment.h"
#include "core/paint/ng/ng_paint_fragment_traversal.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/runtime_enabled_features.h"

namespace blink {

void NGFragmentPainter::PaintOutline(const PaintInfo& paint_info,
                                     const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));

  const ComputedStyle& style = paint_fragment_.Style();
  if (!style.HasOutline() || style.Visibility() != EVisibility::kVisible)
    return;

  // Only paint the focus ring by hand if the theme isn't able to draw the focus
  // ring.
  if (style.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(
          paint_fragment_.GetNode(), style)) {
    return;
  }

  Vector<LayoutRect> outline_rects;
  //
  paint_fragment_.AddSelfOutlineRect(&outline_rects, paint_offset);
  if (outline_rects.IsEmpty())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, paint_fragment_, paint_info.phase))
    return;

  PaintOutlineRects(paint_info, outline_rects, style, paint_fragment_);
}

void NGFragmentPainter::CollectDescendantOutlines(
    const LayoutPoint& paint_offset,
    HashMap<const LayoutObject*, NGPaintFragment*>* anchor_fragment_map,
    HashMap<const LayoutObject*, Vector<LayoutRect>>* outline_rect_map) {
  /*
TODO(atotic): Optimize.
We should not traverse all children for every paint, because
in most cases there are no descendants with outlines.
Possible solution is to only call this function if there are
children with outlines.
*/
  // Collect and group outline rects.
  for (auto& paint_descendant :
       NGPaintFragmentTraversal::DescendantsOf(paint_fragment_)) {
    const ComputedStyle& descendant_style = paint_descendant.fragment->Style();
    if (!paint_descendant.fragment->PhysicalFragment().IsBox() ||
        paint_descendant.fragment->PhysicalFragment().IsAtomicInline())
      continue;

    if (!descendant_style.HasOutline() ||
        descendant_style.Visibility() != EVisibility::kVisible)
      continue;
    if (descendant_style.OutlineStyleIsAuto() &&
        !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(
            paint_descendant.fragment->GetNode(), descendant_style))
      continue;

    const LayoutObject* layout_object =
        paint_descendant.fragment->GetLayoutObject();
    Vector<LayoutRect>* outline_rects;
    auto iter = outline_rect_map->find(layout_object);
    if (iter == outline_rect_map->end()) {
      anchor_fragment_map->insert(layout_object, paint_descendant.fragment);
      outline_rects =
          &outline_rect_map->insert(layout_object, Vector<LayoutRect>())
               .stored_value->value;
    } else {
      outline_rects = &iter->value;
    }
    paint_descendant.fragment->AddSelfOutlineRect(
        outline_rects,
        paint_offset + paint_descendant.container_offset.ToLayoutPoint());
  }
}

void NGFragmentPainter::PaintDescendantOutlines(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintDescendantOutlines(paint_info.phase));

  HashMap<const LayoutObject*, NGPaintFragment*> anchor_fragment_map;
  HashMap<const LayoutObject*, Vector<LayoutRect>> outline_rect_map;

  CollectDescendantOutlines(paint_offset, &anchor_fragment_map,
                            &outline_rect_map);

  // Paint all outlines
  for (auto& anchor_iter : anchor_fragment_map) {
    NGPaintFragment* fragment = anchor_iter.value;
    Vector<LayoutRect>* outline_rects =
        &outline_rect_map.find(anchor_iter.key)->value;

    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, *fragment, paint_info.phase))
      continue;
    PaintOutlineRects(paint_info, *outline_rects, fragment->Style(), *fragment);
  }
}

}  // namespace blink
