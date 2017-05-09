/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009, 2013 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef LayoutTableSection_h
#define LayoutTableSection_h

#include "core/CoreExport.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableBoxComponent.h"
#include "platform/wtf/Vector.h"

namespace blink {

// This variable is used to balance the memory consumption vs the paint
// invalidation time on big tables.
const float kGMaxAllowedOverflowingCellRatioForFastPaintPath = 0.1f;

// Helper class for paintObject.
class CellSpan {
  STACK_ALLOCATED();

 public:
  CellSpan(unsigned start, unsigned end) : start_(start), end_(end) {}

  unsigned Start() const { return start_; }
  unsigned end() const { return end_; }

  void DecreaseStart() { --start_; }
  void IncreaseEnd() { ++end_; }

  void EnsureConsistency(const unsigned);

 private:
  unsigned start_;
  unsigned end_;
};

inline bool operator==(const CellSpan& s1, const CellSpan& s2) {
  return s1.Start() == s2.Start() && s1.end() == s2.end();
}
inline bool operator!=(const CellSpan& s1, const CellSpan& s2) {
  return !(s1 == s2);
}

class LayoutTableCell;
class LayoutTableRow;

// LayoutTableSection is used to represent table row group (display:
// table-row-group), header group (display: table-header-group) and footer group
// (display: table-footer-group).
//
// The object holds the internal representation of the rows (m_grid). See
// recalcCells() below for some extra explanation.
//
// A lot of the complexity in this class is related to handling rowspan, colspan
// or just non-regular tables.
//
// Example of rowspan / colspan leading to overlapping cells (rowspan and
// colspan are overlapping):
// <table>
//   <tr>
//       <td>first row</td>
//       <td rowspan="2">rowspan</td>
//     </tr>
//    <tr>
//        <td colspan="2">colspan</td>
//     </tr>
// </table>
//
// Example of non-regular table (missing one cell in the first row):
// <!DOCTYPE html>
// <table>
//   <tr><td>First row only child.</td></tr>
//   <tr>
//     <td>Second row first child</td>
//     <td>Second row second child</td>
//   </tr>
// </table>
//
// LayoutTableSection is responsible for laying out LayoutTableRows and
// LayoutTableCells (see layoutRows()). However it is not their containing
// block, the enclosing LayoutTable (this object's parent()) is. This is why
// this class inherits from LayoutTableBoxComponent and not LayoutBlock.
class CORE_EXPORT LayoutTableSection final : public LayoutTableBoxComponent {
 public:
  explicit LayoutTableSection(Element*);
  ~LayoutTableSection() override;

  LayoutTableRow* FirstRow() const;
  LayoutTableRow* LastRow() const;

  void AddChild(LayoutObject* child,
                LayoutObject* before_child = nullptr) override;

  int FirstLineBoxBaseline() const override;

  void AddCell(LayoutTableCell*, LayoutTableRow*);

  int VBorderSpacingBeforeFirstRow() const;
  int CalcRowLogicalHeight();
  void LayoutRows();
  void ComputeOverflowFromDescendants();
  bool RecalcChildOverflowAfterStyleChange();

  void MarkAllCellsWidthsDirtyAndOrNeedsLayout(LayoutTable::WhatToMarkAllCells);

  LayoutTable* Table() const { return ToLayoutTable(Parent()); }

  typedef Vector<LayoutTableCell*, 2> SpanningLayoutTableCells;

  // CellStruct represents the cells that occupy an (N, M) position in the
  // table grid.
  struct CellStruct {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    // All the cells that fills this grid "slot".
    // Due to colspan / rowpsan, it is possible to have overlapping cells
    // (see class comment about an example).
    // This Vector is sorted in DOM order.
    Vector<LayoutTableCell*, 1> cells;
    bool in_col_span;  // true for columns after the first in a colspan

    CellStruct();
    ~CellStruct();

    // This is the cell in the grid "slot" that is on top of the others
    // (aka the last cell in DOM order for this slot).
    //
    // Multiple grid slots can have the same primary cell if the cell spans
    // into the grid slots. The slot having the smallest row index and
    // smallest effective column index is the originating slot of the cell.
    //
    // The concept of a primary cell is dubious at most as it doesn't
    // correspond to a DOM or rendering concept. Also callers should be
    // careful about assumptions about it. For example, even though the
    // primary cell is visibly the top most, it is not guaranteed to be
    // the only one visible for this slot due to different visual
    // overflow rectangles.
    LayoutTableCell* PrimaryCell() {
      return HasCells() ? cells[cells.size() - 1] : 0;
    }

    const LayoutTableCell* PrimaryCell() const {
      return HasCells() ? cells[cells.size() - 1] : 0;
    }

    bool HasCells() const { return cells.size() > 0; }
  };

  // The index is effective column index.
  typedef Vector<CellStruct> Row;

  struct RowStruct {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

   public:
    RowStruct() : row_layout_object(nullptr), baseline(-1) {}

    Row row;
    LayoutTableRow* row_layout_object;
    int baseline;
    Length logical_height;
  };

  struct SpanningRowsHeight {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(SpanningRowsHeight);

   public:
    SpanningRowsHeight()
        : total_rows_height(0),
          spanning_cell_height_ignoring_border_spacing(0),
          is_any_row_with_only_spanning_cells(false) {}

    Vector<int> row_height;
    int total_rows_height;
    int spanning_cell_height_ignoring_border_spacing;
    bool is_any_row_with_only_spanning_cells;
  };

  BorderValue BorderAdjoiningTableStart() const {
    if (HasSameDirectionAs(Table()))
      return Style()->BorderStart();

    return Style()->BorderEnd();
  }

  BorderValue BorderAdjoiningTableEnd() const {
    if (HasSameDirectionAs(Table()))
      return Style()->BorderEnd();

    return Style()->BorderStart();
  }

  BorderValue BorderAdjoiningStartCell(const LayoutTableCell*) const;
  BorderValue BorderAdjoiningEndCell(const LayoutTableCell*) const;

  const LayoutTableCell* FirstRowCellAdjoiningTableStart() const;
  const LayoutTableCell* FirstRowCellAdjoiningTableEnd() const;

  CellStruct& CellAt(unsigned row, unsigned effective_column) {
    return grid_[row].row[effective_column];
  }
  const CellStruct& CellAt(unsigned row, unsigned effective_column) const {
    return grid_[row].row[effective_column];
  }
  LayoutTableCell* PrimaryCellAt(unsigned row, unsigned effective_column) {
    Row& row_vector = grid_[row].row;
    if (effective_column >= row_vector.size())
      return nullptr;
    return row_vector[effective_column].PrimaryCell();
  }
  const LayoutTableCell* PrimaryCellAt(unsigned row,
                                       unsigned effective_column) const {
    return const_cast<LayoutTableSection*>(this)->PrimaryCellAt(
        row, effective_column);
  }

  // Returns the primary cell at (row, effectiveColumn) if the cell exists and
  // originates from (instead of spanning into) the grid slot, or nullptr.
  LayoutTableCell* OriginatingCellAt(unsigned row, unsigned effective_column);
  const LayoutTableCell* OriginatingCellAt(unsigned row,
                                           unsigned effective_column) const {
    return const_cast<LayoutTableSection*>(this)->OriginatingCellAt(
        row, effective_column);
  }

  unsigned NumCols(unsigned row) const { return grid_[row].row.size(); }

  // Returns null for cells with a rowspan that exceed the last row. Possibly
  // others.
  LayoutTableRow* RowLayoutObjectAt(unsigned row) {
    return grid_[row].row_layout_object;
  }
  const LayoutTableRow* RowLayoutObjectAt(unsigned row) const {
    return grid_[row].row_layout_object;
  }

  void AppendEffectiveColumn(unsigned pos);
  void SplitEffectiveColumn(unsigned pos, unsigned first);

  enum BlockBorderSide { kBorderBefore, kBorderAfter };
  int CalcBlockDirectionOuterBorder(BlockBorderSide) const;
  enum InlineBorderSide { kBorderStart, kBorderEnd };
  int CalcInlineDirectionOuterBorder(InlineBorderSide) const;
  void RecalcOuterBorder();

  int OuterBorderBefore() const { return outer_border_before_; }
  int OuterBorderAfter() const { return outer_border_after_; }
  int OuterBorderStart() const { return outer_border_start_; }
  int OuterBorderEnd() const { return outer_border_end_; }

  unsigned NumRows() const {
    DCHECK(!NeedsCellRecalc());
    return grid_.size();
  }
  unsigned NumEffectiveColumns() const;

  // recalcCells() is used when we are not sure about the section's structure
  // and want to do an expensive (but safe) reconstruction of m_grid from
  // scratch.
  // An example of this is inserting a new cell in the middle of an existing
  // row or removing a row.
  //
  // Accessing m_grid when m_needsCellRecalc is set is UNSAFE as pointers can
  // be left dangling. Thus care should be taken in the code to check
  // m_needsCellRecalc before accessing m_grid.
  void RecalcCells();
  void RecalcCellsIfNeeded() {
    if (needs_cell_recalc_)
      RecalcCells();
  }

  bool NeedsCellRecalc() const { return needs_cell_recalc_; }
  void SetNeedsCellRecalc();

  int RowBaseline(unsigned row) { return grid_[row].baseline; }

  void RowLogicalHeightChanged(LayoutTableRow*);

  // distributeExtraLogicalHeightToRows methods return the *consumed* extra
  // logical height.
  // FIXME: We may want to introduce a structure holding the in-flux layout
  // information.
  int DistributeExtraLogicalHeightToRows(int extra_logical_height);

  static LayoutTableSection* CreateAnonymousWithParent(const LayoutObject*);
  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override {
    return CreateAnonymousWithParent(parent);
  }

  void Paint(const PaintInfo&, const LayoutPoint&) const override;

  // Flip the rect so it aligns with the coordinates used by the rowPos and
  // columnPos vectors.
  LayoutRect LogicalRectForWritingModeAndDirection(const LayoutRect&) const;

  // Returns a row or column span covering all grid slots from each of which
  // a primary cell intersecting |visualRect| originates.
  CellSpan DirtiedRows(const LayoutRect& visual_rect) const;
  CellSpan DirtiedEffectiveColumns(const LayoutRect& visual_rect) const;

  const HashSet<const LayoutTableCell*>& OverflowingCells() const {
    return overflowing_cells_;
  }
  bool HasMultipleCellLevels() const { return has_multiple_cell_levels_; }

  const char* GetName() const override { return "LayoutTableSection"; }

  // Whether a section has opaque background depends on many factors, e.g.
  // border spacing, border collapsing, missing cells, etc. For simplicity,
  // just conservatively assume all table sections are not opaque.
  bool ForegroundIsKnownToBeOpaqueInRect(const LayoutRect&,
                                         unsigned) const override {
    return false;
  }
  bool BackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const override {
    return false;
  }

  int PaginationStrutForRow(LayoutTableRow*, LayoutUnit logical_offset) const;

  bool MapToVisualRectInAncestorSpaceInternal(
      const LayoutBoxModelObject* ancestor,
      TransformState&,
      VisualRectFlags = kDefaultVisualRectFlags) const override;

  bool IsRepeatingHeaderGroup() const;

  void UpdateLayout() override;

  CellSpan FullSectionRowSpan() const { return CellSpan(0, grid_.size()); }
  CellSpan FullTableEffectiveColumnSpan() const {
    return CellSpan(0, Table()->NumEffectiveColumns());
  }

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) override;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTableSection || LayoutBox::IsOfType(type);
  }

  void WillBeRemovedFromTree() override;

  int BorderSpacingForRow(unsigned row) const {
    return grid_[row].row_layout_object ? Table()->VBorderSpacing() : 0;
  }

  void EnsureRows(unsigned num_rows) {
    if (num_rows > grid_.size())
      grid_.Grow(num_rows);
  }

  void EnsureCols(unsigned row_index, unsigned num_cols) {
    if (num_cols > this->NumCols(row_index))
      grid_[row_index].row.Grow(num_cols);
  }

  bool RowHasOnlySpanningCells(unsigned);
  unsigned CalcRowHeightHavingOnlySpanningCells(unsigned,
                                                int&,
                                                unsigned,
                                                unsigned&,
                                                Vector<int>&);
  void UpdateRowsHeightHavingOnlySpanningCells(LayoutTableCell*,
                                               struct SpanningRowsHeight&,
                                               unsigned&,
                                               Vector<int>&);

  void PopulateSpanningRowsHeightFromCell(LayoutTableCell*,
                                          struct SpanningRowsHeight&);
  void DistributeExtraRowSpanHeightToPercentRows(LayoutTableCell*,
                                                 float,
                                                 int&,
                                                 Vector<int>&);
  void DistributeWholeExtraRowSpanHeightToPercentRows(LayoutTableCell*,
                                                      float,
                                                      int&,
                                                      Vector<int>&);
  void DistributeExtraRowSpanHeightToAutoRows(LayoutTableCell*,
                                              int,
                                              int&,
                                              Vector<int>&);
  void DistributeExtraRowSpanHeightToRemainingRows(LayoutTableCell*,
                                                   int,
                                                   int&,
                                                   Vector<int>&);
  void DistributeRowSpanHeightToRows(SpanningLayoutTableCells& row_span_cells);

  void DistributeExtraLogicalHeightToPercentRows(int& extra_logical_height,
                                                 int total_percent);
  void DistributeExtraLogicalHeightToAutoRows(int& extra_logical_height,
                                              unsigned auto_rows_count);
  void DistributeRemainingExtraLogicalHeight(int& extra_logical_height);

  void UpdateBaselineForCell(LayoutTableCell*,
                             unsigned row,
                             int& baseline_descent);

  bool HasOverflowingCell() const {
    return overflowing_cells_.size() ||
           force_slow_paint_path_with_overflowing_cell_;
  }

  // These two functions take a rectangle as input that has been flipped by
  // logicalRectForWritingModeAndDirection.
  // The returned span of rows or columns is end-exclusive, and empty if
  // start==end.
  CellSpan SpannedRows(const LayoutRect& flipped_rect) const;
  CellSpan SpannedEffectiveColumns(const LayoutRect& flipped_rect) const;

  void SetLogicalPositionForCell(LayoutTableCell*,
                                 unsigned effective_column) const;

  void RelayoutCellIfFlexed(LayoutTableCell&, int row_index, int row_height);

  int LogicalHeightForRow(const LayoutTableRow&) const;

  // Honor breaking restrictions inside the table row, and adjust position and
  // size accordingly.
  void AdjustRowForPagination(LayoutTableRow&, SubtreeLayoutScope&);

  bool PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const override;

  // The representation of the rows and their cells (CellStruct).
  Vector<RowStruct> grid_;

  // The logical offset of each row from the top of the section.
  //
  // Note that this Vector has one more entry than the number of rows so that
  // we can keep track of the final size of the section. That is,
  // m_rowPos[m_grid.size()] is a valid entry.
  //
  // To know a row's height at |rowIndex|, use the formula:
  // m_rowPos[rowIndex + 1] - m_rowPos[rowIndex]
  Vector<int> row_pos_;

  // The current insertion position in the grid.
  // The position is used when inserting a new cell into the section to
  // know where it should be inserted and expand our internal structure.
  //
  // The reason for them is that we process cells as we discover them
  // during parsing or during recalcCells (ie in DOM order). This means
  // that we can discover changes in the structure later (e.g. due to
  // colspans, extra cells, ...).
  //
  // Do not use outside of recalcCells and addChild.
  unsigned c_col_;
  unsigned c_row_;

  int outer_border_start_;
  int outer_border_end_;
  int outer_border_before_;
  int outer_border_after_;

  bool needs_cell_recalc_;

  // This HashSet holds the overflowing cells for faster painting.
  // If we have more than gMaxAllowedOverflowingCellRatio * total cells, it will
  // be empty and m_forceSlowPaintPathWithOverflowingCell will be set to save
  // memory.
  HashSet<const LayoutTableCell*> overflowing_cells_;
  bool force_slow_paint_path_with_overflowing_cell_;

  // This boolean tracks if we have cells overlapping due to rowspan / colspan
  // (see class comment above about when it could appear).
  //
  // The use is to disable a painting optimization where we just paint the
  // invalidated cells.
  bool has_multiple_cell_levels_;

  // Whether any cell spans multiple rows or cols.
  bool has_spanning_cells_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTableSection, IsTableSection());

}  // namespace blink

#endif  // LayoutTableSection_h
