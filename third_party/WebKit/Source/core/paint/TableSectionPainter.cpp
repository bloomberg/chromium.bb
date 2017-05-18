// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableSectionPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableCellPainter.h"
#include "core/paint/TableRowPainter.h"
#include <algorithm>

namespace blink {

void TableSectionPainter::PaintRepeatingHeaderGroup(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const CollapsedBorderValue& current_border_value,
    ItemToPaint item_to_paint) {
  if (!layout_table_section_.IsRepeatingHeaderGroup())
    return;

  LayoutTable* table = layout_table_section_.Table();
  LayoutPoint pagination_offset = paint_offset;
  LayoutUnit page_height = table->PageLogicalHeightForOffset(LayoutUnit());

  LayoutUnit header_group_offset = table->BlockOffsetToFirstRepeatableHeader();
  // The header may have a pagination strut before it so we need to account for
  // that when establishing its position.
  if (LayoutTableRow* row = layout_table_section_.FirstRow())
    header_group_offset += row->PaginationStrut();
  LayoutUnit offset_to_next_page =
      page_height - IntMod(header_group_offset, page_height);
  // Move paginationOffset to the top of the next page.
  pagination_offset.Move(LayoutUnit(), offset_to_next_page);
  // Now move paginationOffset to the top of the page the cull rect starts on.
  if (paint_info.GetCullRect().rect_.Y() > pagination_offset.Y()) {
    pagination_offset.Move(LayoutUnit(),
                           page_height * ((paint_info.GetCullRect().rect_.Y() -
                                           pagination_offset.Y()) /
                                          page_height)
                                             .ToInt());
  }

  // We only want to consider pages where we going to paint a row, so exclude
  // captions and border spacing from the table.
  LayoutRect sections_rect(LayoutPoint(), table->Size());
  table->SubtractCaptionRect(sections_rect);
  LayoutUnit total_height_of_rows =
      sections_rect.Height() - table->VBorderSpacing();
  LayoutUnit bottom_bound =
      std::min(LayoutUnit(paint_info.GetCullRect().rect_.MaxY()),
               paint_offset.Y() + total_height_of_rows);

  while (pagination_offset.Y() < bottom_bound) {
    if (item_to_paint == kPaintCollapsedBorders) {
      PaintCollapsedSectionBorders(paint_info, pagination_offset,
                                   current_border_value);
    } else {
      PaintSection(paint_info, pagination_offset);
    }
    pagination_offset.Move(0, page_height.ToInt());
  }
}

void TableSectionPainter::Paint(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) {
  ObjectPainter(layout_table_section_)
      .CheckPaintOffset(paint_info, paint_offset);
  PaintSection(paint_info, paint_offset);
  LayoutTable* table = layout_table_section_.Table();
  if (table->Header() == layout_table_section_)
    PaintRepeatingHeaderGroup(paint_info, paint_offset, CollapsedBorderValue(),
                              kPaintSection);
}

void TableSectionPainter::PaintSection(const PaintInfo& paint_info,
                                       const LayoutPoint& paint_offset) {
  DCHECK(!layout_table_section_.NeedsLayout());
  // avoid crashing on bugs that cause us to paint with dirty layout
  if (layout_table_section_.NeedsLayout())
    return;

  unsigned total_rows = layout_table_section_.NumRows();
  unsigned total_cols = layout_table_section_.Table()->NumEffectiveColumns();

  if (!total_rows || !total_cols)
    return;

  LayoutPoint adjusted_paint_offset =
      paint_offset + layout_table_section_.Location();

  if (paint_info.phase != kPaintPhaseSelfOutlineOnly) {
    Optional<BoxClipper> box_clipper;
    if (paint_info.phase != kPaintPhaseSelfBlockBackgroundOnly)
      box_clipper.emplace(layout_table_section_, paint_info,
                          adjusted_paint_offset, kForceContentsClip);
    PaintObject(paint_info, adjusted_paint_offset);
  }

  if (ShouldPaintSelfOutline(paint_info.phase))
    ObjectPainter(layout_table_section_)
        .PaintOutline(paint_info, adjusted_paint_offset);
}

void TableSectionPainter::PaintCollapsedBorders(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const CollapsedBorderValue& current_border_value) {
  PaintCollapsedSectionBorders(paint_info, paint_offset, current_border_value);
  LayoutTable* table = layout_table_section_.Table();
  if (table->Header() == layout_table_section_)
    PaintRepeatingHeaderGroup(paint_info, paint_offset, current_border_value,
                              kPaintCollapsedBorders);
}

void TableSectionPainter::PaintCollapsedSectionBorders(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const CollapsedBorderValue& current_border_value) {
  if (!layout_table_section_.NumRows() ||
      !layout_table_section_.Table()->EffectiveColumns().size())
    return;

  LayoutPoint adjusted_paint_offset =
      paint_offset + layout_table_section_.Location();
  BoxClipper box_clipper(layout_table_section_, paint_info,
                         adjusted_paint_offset, kForceContentsClip);

  LayoutRect local_visual_rect = LayoutRect(paint_info.GetCullRect().rect_);
  local_visual_rect.MoveBy(-adjusted_paint_offset);

  LayoutRect table_aligned_rect =
      layout_table_section_.LogicalRectForWritingModeAndDirection(
          local_visual_rect);

  CellSpan dirtied_rows;
  CellSpan dirtied_columns;
  layout_table_section_.DirtiedRowsAndEffectiveColumns(
      table_aligned_rect, dirtied_rows, dirtied_columns);

  if (dirtied_columns.Start() >= dirtied_columns.End())
    return;

  // Collapsed borders are painted from the bottom right to the top left so that
  // precedence due to cell position is respected.
  for (unsigned r = dirtied_rows.End(); r > dirtied_rows.Start(); r--) {
    unsigned row = r - 1;
    unsigned n_cols = layout_table_section_.NumCols(row);
    for (unsigned c = std::min(dirtied_columns.End(), n_cols);
         c > dirtied_columns.Start(); c--) {
      unsigned col = c - 1;
      if (const LayoutTableCell* cell =
              layout_table_section_.OriginatingCellAt(row, col)) {
        LayoutPoint cell_point =
            layout_table_section_.FlipForWritingModeForChild(
                cell, adjusted_paint_offset);
        TableCellPainter(*cell).PaintCollapsedBorders(paint_info, cell_point,
                                                      current_border_value);
      }
    }
  }
}

void TableSectionPainter::PaintObject(const PaintInfo& paint_info,
                                      const LayoutPoint& paint_offset) {
  LayoutRect local_visual_rect = LayoutRect(paint_info.GetCullRect().rect_);
  local_visual_rect.MoveBy(-paint_offset);

  LayoutRect table_aligned_rect =
      layout_table_section_.LogicalRectForWritingModeAndDirection(
          local_visual_rect);

  CellSpan dirtied_rows;
  CellSpan dirtied_columns;
  layout_table_section_.DirtiedRowsAndEffectiveColumns(
      table_aligned_rect, dirtied_rows, dirtied_columns);

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();

  if (ShouldPaintSelfBlockBackground(paint_info.phase)) {
    PaintBoxDecorationBackground(paint_info, paint_offset, dirtied_rows,
                                 dirtied_columns);
  }

  if (paint_info.phase == kPaintPhaseSelfBlockBackgroundOnly)
    return;

  if (ShouldPaintDescendantBlockBackgrounds(paint_info.phase)) {
    for (unsigned r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      const LayoutTableRow* row = layout_table_section_.RowLayoutObjectAt(r);
      // If a row has a layer, we'll paint row background though
      // TableRowPainter::paint().
      if (!row || row->HasSelfPaintingLayer())
        continue;
      TableRowPainter(*row).PaintBoxDecorationBackground(
          paint_info_for_descendants, paint_offset, dirtied_columns);
    }
  }

  // This is tested after background painting because during background painting
  // we need to check validity of the previous background display item based on
  // dirtyRows and dirtyColumns.
  if (dirtied_rows.Start() >= dirtied_rows.End() ||
      dirtied_columns.Start() >= dirtied_columns.End())
    return;

  const auto& overflowing_cells = layout_table_section_.OverflowingCells();
  if (overflowing_cells.IsEmpty()) {
    // This path is for 2 cases:
    // 1. Normal partial paint, without overflowing cells;
    // 2. Full paint, for small sections or big sections with many overflowing
    //    cells.
    // The difference between the normal partial paint and full paint is that
    // whether dirtied_rows and dirtied_columns cover the whole section.
    DCHECK(!layout_table_section_.HasOverflowingCell() ||
           (dirtied_rows == layout_table_section_.FullSectionRowSpan() &&
            dirtied_columns ==
                layout_table_section_.FullTableEffectiveColumnSpan()));

    for (unsigned r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      const LayoutTableRow* row = layout_table_section_.RowLayoutObjectAt(r);
      // TODO(crbug.com/577282): This painting order is inconsistent with other
      // outlines.
      if (row && !row->HasSelfPaintingLayer() &&
          ShouldPaintSelfOutline(paint_info_for_descendants.phase)) {
        TableRowPainter(*row).PaintOutline(paint_info_for_descendants,
                                           paint_offset);
      }
      for (unsigned c = dirtied_columns.Start(); c < dirtied_columns.End();
           c++) {
        if (const LayoutTableCell* cell =
                layout_table_section_.OriginatingCellAt(r, c))
          PaintCell(*cell, paint_info_for_descendants, paint_offset);
      }
    }
  } else {
    // This path paints section with a reasonable number of overflowing cells.
    // This is the "partial paint path" for overflowing cells referred in
    // LayoutTableSection::ComputeOverflowFromDescendants().
    Vector<const LayoutTableCell*> cells;
    CopyToVector(overflowing_cells, cells);

    HashSet<const LayoutTableCell*> spanning_cells;
    for (unsigned r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      const LayoutTableRow* row = layout_table_section_.RowLayoutObjectAt(r);
      // TODO(crbug.com/577282): This painting order is inconsistent with other
      // outlines.
      if (row && !row->HasSelfPaintingLayer() &&
          ShouldPaintSelfOutline(paint_info_for_descendants.phase)) {
        TableRowPainter(*row).PaintOutline(paint_info_for_descendants,
                                           paint_offset);
      }
      unsigned n_cols = layout_table_section_.NumCols(r);
      for (unsigned c = dirtied_columns.Start();
           c < n_cols && c < dirtied_columns.End(); c++) {
        if (const auto* cell = layout_table_section_.OriginatingCellAt(r, c)) {
          if (!overflowing_cells.Contains(cell))
            cells.push_back(cell);
        }
      }
    }

    // Sort the dirty cells by paint order.
    std::sort(cells.begin(), cells.end(), LayoutTableCell::CompareInDOMOrder);
    for (const auto* cell : cells)
      PaintCell(*cell, paint_info_for_descendants, paint_offset);
  }
}

void TableSectionPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const CellSpan& dirtied_rows,
    const CellSpan& dirtied_columns) {
  bool may_have_background = layout_table_section_.Table()->HasColElements() ||
                             layout_table_section_.StyleRef().HasBackground();
  bool has_box_shadow = layout_table_section_.StyleRef().BoxShadow();
  if (!may_have_background && !has_box_shadow)
    return;

  PaintResult paint_result =
      dirtied_columns == layout_table_section_.FullTableEffectiveColumnSpan() &&
              dirtied_rows == layout_table_section_.FullSectionRowSpan()
          ? kFullyPainted
          : kMayBeClippedByPaintDirtyRect;
  layout_table_section_.GetMutableForPainting().UpdatePaintResult(
      paint_result, paint_info.GetCullRect());

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_section_,
          DisplayItem::kBoxDecorationBackground))
    return;

  LayoutRect bounds = BoxPainter(layout_table_section_)
                          .BoundsForDrawingRecorder(paint_info, paint_offset);
  LayoutObjectDrawingRecorder recorder(
      paint_info.context, layout_table_section_,
      DisplayItem::kBoxDecorationBackground, bounds);
  LayoutRect paint_rect(paint_offset, layout_table_section_.Size());

  if (has_box_shadow) {
    BoxPainter::PaintNormalBoxShadow(paint_info, paint_rect,
                                     layout_table_section_.StyleRef());
  }

  if (may_have_background) {
    PaintInfo paint_info_for_cells = paint_info.ForDescendants();
    for (auto r = dirtied_rows.Start(); r < dirtied_rows.End(); r++) {
      for (auto c = dirtied_columns.Start(); c < dirtied_columns.End(); c++) {
        if (const auto* cell = layout_table_section_.OriginatingCellAt(r, c)) {
          PaintBackgroundsBehindCell(*cell, paint_info_for_cells, paint_offset);
        }
      }
    }
  }

  if (has_box_shadow) {
    // TODO(wangxianzhu): Calculate the inset shadow bounds by insetting
    // paintRect by half widths of collapsed borders.
    BoxPainter::PaintInsetBoxShadow(paint_info, paint_rect,
                                    layout_table_section_.StyleRef());
  }
}

void TableSectionPainter::PaintBackgroundsBehindCell(
    const LayoutTableCell& cell,
    const PaintInfo& paint_info_for_cells,
    const LayoutPoint& paint_offset) {
  LayoutPoint cell_point =
      layout_table_section_.FlipForWritingModeForChild(&cell, paint_offset);

  // We need to handle painting a stack of backgrounds. This stack (from bottom
  // to top) consists of the column group, column, row group, row, and then the
  // cell.

  LayoutTable::ColAndColGroup col_and_col_group =
      layout_table_section_.Table()->ColElementAtAbsoluteColumn(
          cell.AbsoluteColumnIndex());
  LayoutTableCol* column = col_and_col_group.col;
  LayoutTableCol* column_group = col_and_col_group.colgroup;
  TableCellPainter table_cell_painter(cell);

  // Column groups and columns first.
  // FIXME: Columns and column groups do not currently support opacity, and they
  // are being painted "too late" in the stack, since we have already opened a
  // transparency layer (potentially) for the table row group.  Note that we
  // deliberately ignore whether or not the cell has a layer, since these
  // backgrounds paint "behind" the cell.
  if (column_group && column_group->StyleRef().HasBackground()) {
    table_cell_painter.PaintContainerBackgroundBehindCell(
        paint_info_for_cells, cell_point, *column_group);
  }
  if (column && column->StyleRef().HasBackground()) {
    table_cell_painter.PaintContainerBackgroundBehindCell(paint_info_for_cells,
                                                          cell_point, *column);
  }

  // Paint the row group next.
  if (layout_table_section_.StyleRef().HasBackground()) {
    table_cell_painter.PaintContainerBackgroundBehindCell(
        paint_info_for_cells, cell_point, layout_table_section_);
  }
}

void TableSectionPainter::PaintCell(const LayoutTableCell& cell,
                                    const PaintInfo& paint_info_for_cells,
                                    const LayoutPoint& paint_offset) {
  if (!cell.HasSelfPaintingLayer() && !cell.Row()->HasSelfPaintingLayer()) {
    LayoutPoint cell_point =
        layout_table_section_.FlipForWritingModeForChild(&cell, paint_offset);
    cell.Paint(paint_info_for_cells, cell_point);
  }
}

}  // namespace blink
