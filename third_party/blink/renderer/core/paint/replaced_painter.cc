// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/replaced_painter.h"

#include "base/optional.h"
#include "third_party/blink/renderer/core/layout/api/selection_state.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_info_with_offset.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/rounded_inner_rect_clipper.h"
#include "third_party/blink/renderer/core/paint/scrollable_area_painter.h"
#include "third_party/blink/renderer/core/paint/selection_painting_utils.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_cache_skipper.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

void ReplacedPainter::Paint(const PaintInfo& paint_info) {
  // TODO(crbug.com/797779): For now embedded contents don't know whether
  // they are painted in a fragmented context and may do something bad in a
  // fragmented context, e.g. creating subsequences. Skip cache to avoid that.
  // This will be unnecessary when the contents are fragment aware.
  base::Optional<DisplayItemCacheSkipper> cache_skipper;
  if (layout_replaced_.IsLayoutEmbeddedContent()) {
    DCHECK(layout_replaced_.HasLayer());
    if (layout_replaced_.Layer()->EnclosingPaginationLayer())
      cache_skipper.emplace(paint_info.context);
  }

  PaintInfoWithOffset paint_info_with_offset(layout_replaced_, paint_info);
  if (!ShouldPaint(paint_info_with_offset))
    return;

  const auto& local_paint_info = paint_info_with_offset.GetPaintInfo();
  auto paint_offset = paint_info_with_offset.PaintOffset();
  LayoutRect border_rect(paint_offset, layout_replaced_.Size());

  if (ShouldPaintSelfBlockBackground(local_paint_info.phase)) {
    if (layout_replaced_.StyleRef().Visibility() == EVisibility::kVisible &&
        layout_replaced_.HasBoxDecorationBackground()) {
      if (layout_replaced_.HasLayer() &&
          layout_replaced_.Layer()->GetCompositingState() ==
              kPaintsIntoOwnBacking &&
          layout_replaced_.Layer()
              ->GetCompositedLayerMapping()
              ->DrawsBackgroundOntoContentLayer())
        return;

      BoxPainter(layout_replaced_)
          .PaintBoxDecorationBackground(local_paint_info, paint_offset);
    }
    // We're done. We don't bother painting any children.
    if (local_paint_info.phase == PaintPhase::kSelfBlockBackgroundOnly)
      return;
  }

  if (local_paint_info.phase == PaintPhase::kMask) {
    BoxPainter(layout_replaced_).PaintMask(local_paint_info, paint_offset);
    return;
  }

  if (ShouldPaintSelfOutline(local_paint_info.phase)) {
    ObjectPainter(layout_replaced_)
        .PaintOutline(local_paint_info, paint_offset);
    return;
  }

  if (local_paint_info.phase != PaintPhase::kForeground &&
      local_paint_info.phase != PaintPhase::kSelection &&
      !layout_replaced_.CanHaveChildren())
    return;

  if (local_paint_info.phase == PaintPhase::kSelection &&
      layout_replaced_.GetSelectionState() == SelectionState::kNone)
    return;

  bool skip_clip = layout_replaced_.IsSVGRoot() &&
                   !ToLayoutSVGRoot(layout_replaced_).ShouldApplyViewportClip();
  if (skip_clip || !layout_replaced_.ContentBoxRect().IsEmpty()) {
    PaintInfo transformed_paint_info = local_paint_info;
    base::Optional<ScopedPaintChunkProperties> chunk_properties;
    if (const auto* fragment = paint_info.FragmentToPaint(layout_replaced_)) {
      if (const auto* paint_properties = fragment->PaintProperties()) {
        PropertyTreeState new_properties =
            local_paint_info.context.GetPaintController()
                .CurrentPaintChunkProperties();
        bool property_changed = false;
        if (paint_properties->ReplacedContentTransform()) {
          new_properties.SetTransform(
              paint_properties->ReplacedContentTransform());
          DCHECK(paint_properties->ReplacedContentTransform()
                     ->Matrix()
                     .IsAffine());
          transformed_paint_info.UpdateCullRect(
              paint_properties->ReplacedContentTransform()
                  ->Matrix()
                  .ToAffineTransform());
          property_changed = true;
        }
        if (paint_properties->OverflowClip() &&
            (!layout_replaced_.IsLayoutImage() ||
             layout_replaced_.StyleRef().HasBorderRadius())) {
          new_properties.SetClip(paint_properties->OverflowClip());
          property_changed = true;
        }
        // Check filter for optimized image policy violation highlights, which
        // may be applied locally.
        if (paint_properties->Filter() &&
            (!layout_replaced_.HasLayer() ||
             !layout_replaced_.Layer()->IsSelfPaintingLayer())) {
          new_properties.SetEffect(paint_properties->Filter());
          property_changed = true;
        }
        if (property_changed) {
          chunk_properties.emplace(
              local_paint_info.context.GetPaintController(), new_properties,
              layout_replaced_, paint_info.DisplayItemTypeForClipping());
        }
      }
    }

    layout_replaced_.PaintReplaced(transformed_paint_info, paint_offset);
  }

  if (layout_replaced_.CanResize()) {
    ScrollableAreaPainter(*layout_replaced_.Layer()->GetScrollableArea())
        .PaintResizer(local_paint_info.context, RoundedIntPoint(paint_offset),
                      local_paint_info.GetCullRect());
  }

  // The selection tint never gets clipped by border-radius rounding, since we
  // want it to run right up to the edges of surrounding content.
  bool draw_selection_tint =
      local_paint_info.phase == PaintPhase::kForeground &&
      IsSelected(layout_replaced_.GetSelectionState()) &&
      !local_paint_info.IsPrinting();
  if (draw_selection_tint && !DrawingRecorder::UseCachedDrawingIfPossible(
                                 local_paint_info.context, layout_replaced_,
                                 DisplayItem::kSelectionTint)) {
    LayoutRect selection_painting_rect = layout_replaced_.LocalSelectionRect();
    selection_painting_rect.MoveBy(paint_offset);
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
    const PaintInfoWithOffset& paint_info_with_offset) const {
  const auto& paint_info = paint_info_with_offset.GetPaintInfo();
  if (paint_info.phase != PaintPhase::kForeground &&
      !ShouldPaintSelfOutline(paint_info.phase) &&
      paint_info.phase != PaintPhase::kSelection &&
      paint_info.phase != PaintPhase::kMask &&
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

  LayoutRect local_rect(layout_replaced_.VisualOverflowRect());
  local_rect.Unite(layout_replaced_.LocalSelectionRect());
  if (!paint_info_with_offset.LocalRectIntersectsCullRect(local_rect))
    return false;

  return true;
}

}  // namespace blink
