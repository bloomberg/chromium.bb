// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableRowPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/BoxPainterBase.h"
#include "core/paint/CollapsedBorderPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"

namespace blink {

void TableRowPainter::Paint(const PaintInfo& paint_info,
                            const LayoutPoint& paint_offset) {
  ObjectPainter(layout_table_row_).CheckPaintOffset(paint_info, paint_offset);
  DCHECK(layout_table_row_.HasSelfPaintingLayer());

  // TODO(crbug.com/577282): This painting order is inconsistent with other
  // outlines.
  if (ShouldPaintSelfOutline(paint_info.phase))
    PaintOutline(paint_info, paint_offset);
  if (paint_info.phase == kPaintPhaseSelfOutlineOnly)
    return;

  if (ShouldPaintSelfBlockBackground(paint_info.phase)) {
    PaintBoxDecorationBackground(
        paint_info, paint_offset,
        layout_table_row_.Section()->FullTableEffectiveColumnSpan());
  }

  if (paint_info.phase == kPaintPhaseSelfBlockBackgroundOnly)
    return;

  PaintInfo paint_info_for_cells = paint_info.ForDescendants();
  for (LayoutTableCell* cell = layout_table_row_.FirstCell(); cell;
       cell = cell->NextCell()) {
    if (!cell->HasSelfPaintingLayer())
      cell->Paint(paint_info_for_cells, paint_offset);
  }
}

void TableRowPainter::PaintOutline(const PaintInfo& paint_info,
                                   const LayoutPoint& paint_offset) {
  DCHECK(ShouldPaintSelfOutline(paint_info.phase));
  LayoutPoint adjusted_paint_offset =
      paint_offset + layout_table_row_.Location();
  ObjectPainter(layout_table_row_)
      .PaintOutline(paint_info, adjusted_paint_offset);
}

void TableRowPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const CellSpan& dirtied_columns) {
  bool has_background = layout_table_row_.StyleRef().HasBackground();
  bool has_box_shadow = layout_table_row_.StyleRef().BoxShadow();
  if (!has_background && !has_box_shadow)
    return;

  const auto* section = layout_table_row_.Section();
  PaintResult paint_result =
      dirtied_columns == section->FullTableEffectiveColumnSpan()
          ? kFullyPainted
          : kMayBeClippedByPaintDirtyRect;
  layout_table_row_.GetMutableForPainting().UpdatePaintResult(
      paint_result, paint_info.GetCullRect());

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_row_,
          DisplayItem::kBoxDecorationBackground))
    return;

  LayoutPoint adjusted_paint_offset =
      paint_offset + layout_table_row_.Location();
  LayoutRect bounds =
      BoxPainter(layout_table_row_)
          .BoundsForDrawingRecorder(paint_info, adjusted_paint_offset);
  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_table_row_,
                                       DisplayItem::kBoxDecorationBackground,
                                       bounds);
  LayoutRect paint_rect(adjusted_paint_offset, layout_table_row_.Size());

  if (has_box_shadow) {
    BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect,
                                         layout_table_row_.StyleRef());
  }

  if (has_background) {
    PaintInfo paint_info_for_cells = paint_info.ForDescendants();
    for (auto c = dirtied_columns.Start(); c < dirtied_columns.End(); c++) {
      if (const auto* cell =
              section->OriginatingCellAt(layout_table_row_.RowIndex(), c))
        PaintBackgroundBehindCell(*cell, paint_info_for_cells, paint_offset);
    }
  }

  if (has_box_shadow) {
    // TODO(wangxianzhu): Calculate the inset shadow bounds by insetting
    // paintRect by half widths of collapsed borders.
    BoxPainterBase::PaintInsetBoxShadow(paint_info, paint_rect,
                                        layout_table_row_.StyleRef());
  }
}

void TableRowPainter::PaintBackgroundBehindCell(
    const LayoutTableCell& cell,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  DCHECK(layout_table_row_.StyleRef().HasBackground());
  LayoutPoint cell_point = paint_offset;
  // If the row is self painting, paintOffset is in row's coordinates, so
  // doesn't need to flip in section's blocks direction. A row doesn't have
  // flipped blocks direction.
  if (!layout_table_row_.HasSelfPaintingLayer()) {
    cell_point = layout_table_row_.Section()->FlipForWritingModeForChild(
        &cell, cell_point);
  }
  TableCellPainter(cell).PaintContainerBackgroundBehindCell(
      paint_info, cell_point, layout_table_row_);
}

void TableRowPainter::PaintCollapsedBorders(const PaintInfo& paint_info,
                                            const LayoutPoint& paint_offset,
                                            const CellSpan& dirtied_columns) {
  Optional<LayoutObjectDrawingRecorder> recorder;

  if (LIKELY(!layout_table_row_.Table()->ShouldPaintAllCollapsedBorders())) {
    if (DrawingRecorder::UseCachedDrawingIfPossible(
            paint_info.context, layout_table_row_,
            DisplayItem::kTableCollapsedBorders))
      return;

    LayoutRect bounds =
        BoxPainter(layout_table_row_)
            .BoundsForDrawingRecorder(
                paint_info, paint_offset + layout_table_row_.Location());
    recorder.emplace(paint_info.context, layout_table_row_,
                     DisplayItem::kTableCollapsedBorders, bounds);
  }
  // Otherwise TablePainter should have created the drawing recorder.

  const auto* section = layout_table_row_.Section();
  unsigned row = layout_table_row_.RowIndex();
  for (unsigned c = std::min(dirtied_columns.End(), section->NumCols(row));
       c > dirtied_columns.Start(); c--) {
    if (const auto* cell = section->OriginatingCellAt(row, c - 1)) {
      LayoutPoint cell_point =
          section->FlipForWritingModeForChild(cell, paint_offset);
      CollapsedBorderPainter(*cell).PaintCollapsedBorders(paint_info,
                                                          cell_point);
    }
  }
}

}  // namespace blink
