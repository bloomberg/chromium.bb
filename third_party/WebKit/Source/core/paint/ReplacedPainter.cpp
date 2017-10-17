// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ReplacedPainter.h"

#include "core/layout/LayoutReplaced.h"
#include "core/layout/api/SelectionState.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/SelectionPaintingUtils.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "platform/graphics/paint/ScopedPaintChunkProperties.h"
#include "platform/wtf/Optional.h"

namespace blink {

static bool ShouldApplyViewportClip(const LayoutReplaced& layout_replaced) {
  return !layout_replaced.IsSVGRoot() ||
         ToLayoutSVGRoot(&layout_replaced)->ShouldApplyViewportClip();
}

bool ReplacedPainter::ShouldAdjustForPaintOffsetTranslation(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) const {
  if (!RuntimeEnabledFeatures::SlimmingPaintV175Enabled())
    return false;
  if (layout_replaced_.HasSelfPaintingLayer())
    return false;
  if (!layout_replaced_.FirstFragment())
    return false;
  auto* paint_properties = layout_replaced_.FirstFragment()->PaintProperties();
  if (!paint_properties)
    return false;
  if (!paint_properties->PaintOffsetTranslation())
    return false;

  return true;
}

void ReplacedPainter::Paint(const PaintInfo& paint_info,
                            const LayoutPoint& paint_offset) {
  Optional<ScopedPaintChunkProperties> scoped_contents_properties;
  LayoutPoint adjusted_paint_offset;
  PaintInfo local_paint_info(paint_info);
  if (ShouldAdjustForPaintOffsetTranslation(local_paint_info, paint_offset)) {
    auto* paint_properties =
        layout_replaced_.FirstFragment()->PaintProperties();
    const auto* local_border_box_properties =
        layout_replaced_.FirstFragment()->LocalBorderBoxProperties();
    PaintChunkProperties chunk_properties(
        paint_info.context.GetPaintController().CurrentPaintChunkProperties());
    chunk_properties.property_tree_state = *local_border_box_properties;
    scoped_contents_properties.emplace(paint_info.context.GetPaintController(),
                                       layout_replaced_, chunk_properties);

    adjusted_paint_offset = layout_replaced_.PaintOffset();
    local_paint_info.UpdateCullRect(paint_properties->PaintOffsetTranslation()
                                        ->Matrix()
                                        .ToAffineTransform());
  } else {
    ObjectPainter(layout_replaced_).CheckPaintOffset(paint_info, paint_offset);
    adjusted_paint_offset = paint_offset + layout_replaced_.Location();
  }

  if (!ShouldPaint(paint_info, adjusted_paint_offset))
    return;

  LayoutRect border_rect(adjusted_paint_offset, layout_replaced_.Size());

  if (ShouldPaintSelfBlockBackground(paint_info.phase)) {
    if (layout_replaced_.Style()->Visibility() == EVisibility::kVisible &&
        layout_replaced_.HasBoxDecorationBackground()) {
      if (layout_replaced_.HasLayer() &&
          layout_replaced_.Layer()->GetCompositingState() ==
              kPaintsIntoOwnBacking &&
          layout_replaced_.Layer()
              ->GetCompositedLayerMapping()
              ->DrawsBackgroundOntoContentLayer())
        return;

      layout_replaced_.PaintBoxDecorationBackground(paint_info,
                                                    adjusted_paint_offset);
    }
    // We're done. We don't bother painting any children.
    if (paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (paint_info.phase == PaintPhase::kMask) {
    layout_replaced_.PaintMask(paint_info, adjusted_paint_offset);
    return;
  }

  if (paint_info.phase == PaintPhase::kClippingMask &&
      (!layout_replaced_.HasLayer() ||
       !layout_replaced_.Layer()->HasCompositedClippingMask()))
    return;

  if (ShouldPaintSelfOutline(paint_info.phase)) {
    ObjectPainter(layout_replaced_)
        .PaintOutline(paint_info, adjusted_paint_offset);
    return;
  }

  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kSelection &&
      !layout_replaced_.CanHaveChildren() &&
      paint_info.phase != PaintPhase::kClippingMask)
    return;

  if (paint_info.phase == PaintPhase::kSelection)
    if (layout_replaced_.GetSelectionState() == SelectionState::kNone)
      return;

  {
    Optional<RoundedInnerRectClipper> clipper;
    bool completely_clipped_out = false;
    if (layout_replaced_.Style()->HasBorderRadius()) {
      if (border_rect.IsEmpty()) {
        completely_clipped_out = true;
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

        clipper.emplace(layout_replaced_, paint_info, border_rect,
                        rounded_inner_rect, kApplyToDisplayList);
      }
    }

    if (!completely_clipped_out) {
      if (paint_info.phase == PaintPhase::kClippingMask) {
        BoxPainter(layout_replaced_)
            .PaintClippingMask(paint_info, adjusted_paint_offset);
      } else {
        layout_replaced_.PaintReplaced(paint_info, adjusted_paint_offset);
      }
    }
  }

  // The selection tint never gets clipped by border-radius rounding, since we
  // want it to run right up to the edges of surrounding content.
  bool draw_selection_tint =
      paint_info.phase == PaintPhase::kForeground &&
      layout_replaced_.GetSelectionState() != SelectionState::kNone &&
      !paint_info.IsPrinting();
  if (draw_selection_tint &&
      !LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_replaced_, DisplayItem::kSelectionTint)) {
    LayoutRect selection_painting_rect = layout_replaced_.LocalSelectionRect();
    selection_painting_rect.MoveBy(adjusted_paint_offset);
    IntRect selection_painting_int_rect =
        PixelSnappedIntRect(selection_painting_rect);

    LayoutObjectDrawingRecorder drawing_recorder(
        paint_info.context, layout_replaced_, DisplayItem::kSelectionTint,
        selection_painting_int_rect);
    Color selection_bg = SelectionPaintingUtils::SelectionBackgroundColor(
        layout_replaced_.GetDocument(), layout_replaced_.StyleRef(),
        layout_replaced_.GetNode());
    paint_info.context.FillRect(selection_painting_int_rect, selection_bg);
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
