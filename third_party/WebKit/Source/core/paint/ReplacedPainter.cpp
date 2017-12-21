// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ReplacedPainter.h"

#include "core/layout/LayoutReplaced.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/AdjustPaintOffsetScope.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/SelectionPaintingUtils.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/wtf/Optional.h"

namespace blink {

static bool ShouldApplyViewportClip(const LayoutReplaced& layout_replaced) {
  return !layout_replaced.IsSVGRoot() ||
         ToLayoutSVGRoot(&layout_replaced)->ShouldApplyViewportClip();
}

void ReplacedPainter::Paint(const PaintInfo& paint_info,
                            const LayoutPoint& paint_offset) {
  AdjustPaintOffsetScope adjustment(layout_replaced_, paint_info, paint_offset);
  const auto& local_paint_info = adjustment.GetPaintInfo();
  auto adjusted_paint_offset = adjustment.AdjustedPaintOffset();

  if (!ShouldPaint(local_paint_info, adjusted_paint_offset))
    return;

  LayoutRect border_rect(adjusted_paint_offset, layout_replaced_.Size());

  if (ShouldPaintSelfBlockBackground(local_paint_info.phase)) {
    if (layout_replaced_.Style()->Visibility() == EVisibility::kVisible &&
        layout_replaced_.HasBoxDecorationBackground()) {
      if (layout_replaced_.HasLayer() &&
          layout_replaced_.Layer()->GetCompositingState() ==
              kPaintsIntoOwnBacking &&
          layout_replaced_.Layer()
              ->GetCompositedLayerMapping()
              ->DrawsBackgroundOntoContentLayer())
        return;

      layout_replaced_.PaintBoxDecorationBackground(local_paint_info,
                                                    adjusted_paint_offset);
    }
    // We're done. We don't bother painting any children.
    if (local_paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (local_paint_info.phase == PaintPhase::kMask) {
    layout_replaced_.PaintMask(local_paint_info, adjusted_paint_offset);
    return;
  }

  if (local_paint_info.phase == PaintPhase::kClippingMask &&
      (!layout_replaced_.HasLayer() ||
       !layout_replaced_.Layer()->HasCompositedClippingMask()))
    return;

  if (ShouldPaintSelfOutline(local_paint_info.phase)) {
    ObjectPainter(layout_replaced_)
        .PaintOutline(local_paint_info, adjusted_paint_offset);
    return;
  }

  if (local_paint_info.phase != PaintPhase::kForeground &&
      local_paint_info.phase != PaintPhase::kSelection &&
      !layout_replaced_.CanHaveChildren() &&
      local_paint_info.phase != PaintPhase::kClippingMask)
    return;

  if (local_paint_info.phase == PaintPhase::kSelection &&
      layout_replaced_.GetSelectionState() == SelectionState::kNone)
    return;

  {
    Optional<RoundedInnerRectClipper> clipper;
    Optional<ScopedPaintChunkProperties> chunk_properties;
    bool completely_clipped_out = false;
    if (layout_replaced_.Style()->HasBorderRadius()) {
      if (border_rect.IsEmpty()) {
        completely_clipped_out = true;
      } else if (RuntimeEnabledFeatures::SlimmingPaintV175Enabled()) {
        if (!layout_replaced_.IsSVGRoot()) {
          if (const auto* fragment =
                  paint_info.FragmentToPaint(layout_replaced_)) {
            const auto* properties = fragment->PaintProperties();
            DCHECK(properties && properties->InnerBorderRadiusClip());
            chunk_properties.emplace(
                local_paint_info.context.GetPaintController(),
                properties->InnerBorderRadiusClip(), layout_replaced_,
                DisplayItem::PaintPhaseToDrawingType(local_paint_info.phase));
          }
        }
      } else if (ShouldApplyViewportClip(layout_replaced_)) {
        // Push a clip if we have a border radius, since we want to round the
        // foreground content that gets painted.
        FloatRoundedRect rounded_inner_rect =
            layout_replaced_.Style()->GetRoundedInnerBorderFor(
                border_rect,
                LayoutRectOutsets(-(layout_replaced_.PaddingTop() +
                                    layout_replaced_.BorderTop()),
                                  -(layout_replaced_.PaddingRight() +
                                    layout_replaced_.BorderRight()),
                                  -(layout_replaced_.PaddingBottom() +
                                    layout_replaced_.BorderBottom()),
                                  -(layout_replaced_.PaddingLeft() +
                                    layout_replaced_.BorderLeft())),
                true, true);

        clipper.emplace(layout_replaced_, local_paint_info, border_rect,
                        rounded_inner_rect, kApplyToDisplayList);
      }
    }

    if (!completely_clipped_out) {
      if (local_paint_info.phase == PaintPhase::kClippingMask) {
        BoxPainter(layout_replaced_)
            .PaintClippingMask(local_paint_info, adjusted_paint_offset);
      } else {
        layout_replaced_.PaintReplaced(local_paint_info, adjusted_paint_offset);
      }
    }
  }

  // The selection tint never gets clipped by border-radius rounding, since we
  // want it to run right up to the edges of surrounding content.
  bool draw_selection_tint =
      local_paint_info.phase == PaintPhase::kForeground &&
      layout_replaced_.GetSelectionState() != SelectionState::kNone &&
      !local_paint_info.IsPrinting();
  if (draw_selection_tint && !DrawingRecorder::UseCachedDrawingIfPossible(
                                 local_paint_info.context, layout_replaced_,
                                 DisplayItem::kSelectionTint)) {
    LayoutRect selection_painting_rect = layout_replaced_.LocalSelectionRect();
    selection_painting_rect.MoveBy(adjusted_paint_offset);
    IntRect selection_painting_int_rect =
        PixelSnappedIntRect(selection_painting_rect);

    DrawingRecorder recorder(local_paint_info.context, layout_replaced_,
                             DisplayItem::kSelectionTint);
    Color selection_bg = SelectionPaintingUtils::SelectionBackgroundColor(
        layout_replaced_.GetDocument(), layout_replaced_.StyleRef(),
        layout_replaced_.GetNode());
    local_paint_info.context.FillRect(selection_painting_int_rect,
                                      selection_bg);
  }
}

bool ReplacedPainter::ShouldPaint(
    const PaintInfo& paint_info,
    const LayoutPoint& adjusted_paint_offset) const {
  if (paint_info.phase != PaintPhase::kForeground &&
      !ShouldPaintSelfOutline(paint_info.phase) &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kMask &&
      paint_info.phase != PaintPhase::kClippingMask &&
      !ShouldPaintSelfBlockBackground(paint_info.phase))
    return false;

  if (layout_replaced_.IsTruncated())
    return false;

  // If we're invisible or haven't received a layout yet, just bail.
  // But if it's an SVG root, there can be children, so we'll check visibility
  // later.
  if (!layout_replaced_.IsSVGRoot() &&
      layout_replaced_.Style()->Visibility() != EVisibility::kVisible)
    return false;

  LayoutRect paint_rect(layout_replaced_.VisualOverflowRect());
  paint_rect.Unite(layout_replaced_.LocalSelectionRect());
  paint_rect.MoveBy(adjusted_paint_offset);

  if (!paint_info.GetCullRect().IntersectsCullRect(paint_rect))
    return false;

  return true;
}

}  // namespace blink
