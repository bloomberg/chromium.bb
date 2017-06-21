// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableCellPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
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

  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(
      paint_offset + layout_table_cell_.Location());
  PaintBackground(paint_info, paint_rect, background_object);
}

void TableCellPainter::PaintBackground(const PaintInfo& paint_info,
                                       const LayoutRect& paint_rect,
                                       const LayoutObject& background_object) {
  if (layout_table_cell_.BackgroundStolenForBeingBody())
    return;

  Color c = background_object.ResolveColor(CSSPropertyBackgroundColor);
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
    BoxPainter(layout_table_cell_)
        .PaintFillLayers(paint_info, c, bg_layer, paint_rect,
                         kBackgroundBleedNone, SkBlendMode::kSrcOver,
                         &background_object);
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

  bool needs_to_paint_border =
      style.HasBorderDecoration() && !table->ShouldCollapseBorders();
  if (!style.HasBackground() && !style.BoxShadow() && !needs_to_paint_border)
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_cell_,
          DisplayItem::kBoxDecorationBackground))
    return;

  LayoutRect visual_overflow_rect = layout_table_cell_.VisualOverflowRect();
  visual_overflow_rect.MoveBy(paint_offset);
  // TODO(chrishtr): the pixel-snapping here is likely incorrect.
  LayoutObjectDrawingRecorder recorder(
      paint_info.context, layout_table_cell_,
      DisplayItem::kBoxDecorationBackground,
      PixelSnappedIntRect(visual_overflow_rect));

  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);

  BoxPainter::PaintNormalBoxShadow(paint_info, paint_rect, style);
  PaintBackground(paint_info, paint_rect, layout_table_cell_);
  // TODO(wangxianzhu): Calculate the inset shadow bounds by insetting paintRect
  // by half widths of collapsed borders.
  BoxPainter::PaintInsetBoxShadow(paint_info, paint_rect, style);

  if (!needs_to_paint_border)
    return;

  BoxPainter::PaintBorder(layout_table_cell_, layout_table_cell_.GetDocument(),
                          layout_table_cell_.GeneratingNode(), paint_info,
                          paint_rect, style);
}

void TableCellPainter::PaintMask(const PaintInfo& paint_info,
                                 const LayoutPoint& paint_offset) {
  if (layout_table_cell_.Style()->Visibility() != EVisibility::kVisible ||
      paint_info.phase != kPaintPhaseMask)
    return;

  LayoutTable* table_elt = layout_table_cell_.Table();
  if (!table_elt->ShouldCollapseBorders() &&
      layout_table_cell_.Style()->EmptyCells() == EEmptyCells::kHide &&
      !layout_table_cell_.FirstChild())
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_cell_, paint_info.phase))
    return;

  LayoutRect paint_rect = PaintRectNotIncludingVisualOverflow(paint_offset);
  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_table_cell_,
                                       paint_info.phase, paint_rect);
  BoxPainter(layout_table_cell_).PaintMaskImages(paint_info, paint_rect);
}

LayoutRect TableCellPainter::PaintRectNotIncludingVisualOverflow(
    const LayoutPoint& paint_offset) {
  return LayoutRect(paint_offset,
                    LayoutSize(layout_table_cell_.PixelSnappedSize()));
}

}  // namespace blink
