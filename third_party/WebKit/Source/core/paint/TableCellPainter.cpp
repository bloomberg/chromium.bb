// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableCellPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/paint/AdjustPaintOffsetScope.h"
#include "core/paint/BackgroundImageGeometry.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxModelObjectPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/BoxPainterBase.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void TableCellPainter::Paint(const PaintInfo& paint_info,
                             const LayoutPoint& paint_offset) {
  BlockPainter(layout_table_cell_).Paint(paint_info, paint_offset);
}

void TableCellPainter::PaintContainerBackgroundBehindCell(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutObject& background_object) {
  DCHECK(background_object != layout_table_cell_);

  if (layout_table_cell_.Style()->Visibility() != EVisibility::kVisible)
    return;

  LayoutTable* table = layout_table_cell_.Table();
  if (!table->ShouldCollapseBorders() &&
      layout_table_cell_.Style()->EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  AdjustPaintOffsetScope adjustment(layout_table_cell_, paint_info,
                                    paint_offset);
  auto paint_rect =
      PaintRectNotIncludingVisualOverflow(adjustment.AdjustedPaintOffset());
  PaintBackground(adjustment.GetPaintInfo(), paint_rect, background_object);
}

void TableCellPainter::PaintBackground(const PaintInfo& paint_info,
                                       const LayoutRect& paint_rect,
                                       const LayoutObject& background_object) {
  if (layout_table_cell_.BackgroundStolenForBeingBody())
    return;

  Color c = background_object.ResolveColor(GetCSSPropertyBackgroundColor());
  const FillLayer& bg_layer = background_object.StyleRef().BackgroundLayers();
  if (bg_layer.HasImage() || c.Alpha()) {
    // We have to clip here because the background would paint
    // on top of the borders otherwise.  This only matters for cells and rows.
    bool should_clip = background_object.HasLayer() &&
                       (background_object == layout_table_cell_ ||
                        background_object == layout_table_cell_.Parent()) &&
                       layout_table_cell_.Table()->ShouldCollapseBorders();
    GraphicsContextStateSaver state_saver(paint_info.context, should_clip);
    if (should_clip) {
      LayoutRect clip_rect(paint_rect.Location(), layout_table_cell_.Size());
      clip_rect.Expand(layout_table_cell_.BorderInsets());
      paint_info.context.Clip(PixelSnappedIntRect(clip_rect));
    }
    BackgroundImageGeometry geometry(layout_table_cell_, &background_object);
    BoxModelObjectPainter(layout_table_cell_)
        .PaintFillLayers(paint_info, c, bg_layer, paint_rect, geometry,
                         kBackgroundBleedNone, SkBlendMode::kSrcOver);
  }
}

void TableCellPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  LayoutTable* table = layout_table_cell_.Table();
  const ComputedStyle& style = layout_table_cell_.StyleRef();
  if (!table->ShouldCollapseBorders() &&
      style.EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  bool has_background = style.HasBackground();
  bool has_box_shadow = style.BoxShadow();
  bool needs_to_paint_border =
      style.HasBorderDecoration() && !table->ShouldCollapseBorders();
  if (!has_background && !has_box_shadow && !needs_to_paint_border)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_cell_,
          DisplayItem::kBoxDecorationBackground))
    return;

  // TODO(chrishtr): the pixel-snapping here is likely incorrect.
  DrawingRecorder recorder(paint_info.context, layout_table_cell_,
                           DisplayItem::kBoxDecorationBackground);

  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);

  if (has_box_shadow)
    BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect, style);

  if (has_background)
    PaintBackground(paint_info, paint_rect, layout_table_cell_);

  if (has_box_shadow) {
    // If the table collapses borders, the inner rect is the border box rect
    // inset by inner half widths of collapsed borders (which are returned
    // from the overriden BorderXXX() methods). Otherwise the following code is
    // equivalent to BoxPainterBase::PaintInsetBoxShadowWithBorderRect().
    auto inner_rect = paint_rect;
    inner_rect.ContractEdges(
        layout_table_cell_.BorderTop(), layout_table_cell_.BorderRight(),
        layout_table_cell_.BorderBottom(), layout_table_cell_.BorderLeft());
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(
        paint_info, inner_rect, layout_table_cell_.StyleRef());
  }

  if (!needs_to_paint_border)
    return;

  BoxPainterBase::PaintBorder(
      layout_table_cell_, layout_table_cell_.GetDocument(),
      layout_table_cell_.GeneratingNode(), paint_info, paint_rect, style);
}

void TableCellPainter::PaintMask(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  if (layout_table_cell_.Style()->Visibility() != EVisibility::kVisible ||
      paint_info.phase != PaintPhase::kMask)
    return;

  LayoutTable* table_elt = layout_table_cell_.Table();
  if (!table_elt->ShouldCollapseBorders() &&
      layout_table_cell_.Style()->EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_cell_, paint_info.phase))
    return;

  DrawingRecorder recorder(paint_info.context, layout_table_cell_,
                           paint_info.phase);
  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);
  BoxPainter(layout_table_cell_).PaintMaskImages(paint_info, paint_rect);
}

LayoutRect TableCellPainter::PaintRectNotIncludingVisualOverflow(
    const LayoutPoint& paint_offset) {
  return LayoutRect(paint_offset,
                    LayoutSize(layout_table_cell_.PixelSnappedSize()));
}

}  // namespace blink
