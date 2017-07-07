/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2009, 2013 Apple Inc.
 *               All rights reserved.
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

#ifndef LayoutTableCell_h
#define LayoutTableCell_h

#include <memory>
#include "core/CoreExport.h"
#include "core/layout/CollapsedBorderValue.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTableSection.h"
#include "platform/LengthFunctions.h"
#include "platform/text/WritingModeUtils.h"

namespace blink {

#define BITS_OF_ABSOLUTE_COLUMN_INDEX 28
static const unsigned kUnsetColumnIndex =
    (1u << BITS_OF_ABSOLUTE_COLUMN_INDEX) - 1;
static const unsigned kMaxColumnIndex = kUnsetColumnIndex - 1;

class SubtreeLayoutScope;

// LayoutTableCell is used to represent a table cell (display: table-cell).
//
// Because rows are as tall as the tallest cell, cells need to be aligned into
// the enclosing row space. To achieve this, LayoutTableCell introduces the
// concept of 'intrinsic padding'. Those 2 paddings are used to shift the box
// into the row as follows:
//
//        --------------------------------
//        ^  ^
//        |  |
//        |  |    cell's border before
//        |  |
//        |  v
//        |  ^
//        |  |
//        |  | m_intrinsicPaddingBefore
//        |  |
//        |  v
//        |  -----------------------------
//        |  |                           |
// row    |  |   cell's padding box      |
// height |  |                           |
//        |  -----------------------------
//        |  ^
//        |  |
//        |  | m_intrinsicPaddingAfter
//        |  |
//        |  v
//        |  ^
//        |  |
//        |  |    cell's border after
//        |  |
//        v  v
//        ---------------------------------
//
// Note that this diagram is not impacted by collapsing or separate borders
// (see 'border-collapse').
// Also there is no margin on table cell (or any internal table element).
//
// LayoutTableCell is positioned with respect to the enclosing
// LayoutTableSection. See callers of
// LayoutTableSection::setLogicalPositionForCell() for when it is placed.
class CORE_EXPORT LayoutTableCell final : public LayoutBlockFlow {
 public:
  explicit LayoutTableCell(Element*);

  unsigned ColSpan() const {
    if (!has_col_span_)
      return 1;
    return ParseColSpanFromDOM();
  }
  unsigned RowSpan() const {
    if (!has_row_span_)
      return 1;
    return ParseRowSpanFromDOM();
  }

  // Called from HTMLTableCellElement.
  void ColSpanOrRowSpanChanged();

  void SetAbsoluteColumnIndex(unsigned column) {
    CHECK_LE(column, kMaxColumnIndex);
    absolute_column_index_ = column;
  }

  bool HasSetAbsoluteColumnIndex() const {
    return absolute_column_index_ != kUnsetColumnIndex;
  }

  unsigned AbsoluteColumnIndex() const {
    DCHECK(HasSetAbsoluteColumnIndex());
    return absolute_column_index_;
  }

  LayoutTableRow* Row() const { return ToLayoutTableRow(Parent()); }
  LayoutTableSection* Section() const {
    return ToLayoutTableSection(Parent()->Parent());
  }
  LayoutTable* Table() const {
    return ToLayoutTable(Parent()->Parent()->Parent());
  }

  LayoutTableCell* PreviousCell() const;
  LayoutTableCell* NextCell() const;

  unsigned RowIndex() const {
    // This function shouldn't be called on a detached cell.
    DCHECK(Row());
    return Row()->RowIndex();
  }

  Length StyleOrColLogicalWidth() const {
    Length style_width = Style()->LogicalWidth();
    if (!style_width.IsAuto())
      return style_width;
    if (LayoutTableCol* first_column =
            Table()
                ->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
                .InnermostColOrColGroup())
      return LogicalWidthFromColumns(first_column, style_width);
    return style_width;
  }

  int LogicalHeightFromStyle() const {
    Length height = Style()->LogicalHeight();
    int style_logical_height =
        height.IsIntrinsicOrAuto()
            ? 0
            : ValueForLength(height, LayoutUnit()).ToInt();

    // In strict mode, box-sizing: content-box do the right thing and actually
    // add in the border and padding.
    // Call computedCSSPadding* directly to avoid including implicitPadding.
    if (!GetDocument().InQuirksMode() &&
        Style()->BoxSizing() != EBoxSizing::kBorderBox) {
      style_logical_height +=
          (ComputedCSSPaddingBefore() + ComputedCSSPaddingAfter()).Floor() +
          (BorderBefore() + BorderAfter()).Floor();
    }
    return style_logical_height;
  }

  int LogicalHeightForRowSizing() const {
    // FIXME: This function does too much work, and is very hot during table
    // layout!
    int adjusted_logical_height =
        PixelSnappedLogicalHeight() -
        (IntrinsicPaddingBefore() + IntrinsicPaddingAfter());
    int style_logical_height = LogicalHeightFromStyle();
    return max(style_logical_height, adjusted_logical_height);
  }

  void SetCellLogicalWidth(int constrained_logical_width, SubtreeLayoutScope&);

  LayoutUnit BorderLeft() const override;
  LayoutUnit BorderRight() const override;
  LayoutUnit BorderTop() const override;
  LayoutUnit BorderBottom() const override;
  LayoutUnit BorderStart() const override;
  LayoutUnit BorderEnd() const override;
  LayoutUnit BorderBefore() const override;
  LayoutUnit BorderAfter() const override;

  void UpdateLayout() override;

  void Paint(const PaintInfo&, const LayoutPoint&) const override;

  int CellBaselinePosition() const;
  bool IsBaselineAligned() const {
    EVerticalAlign va = Style()->VerticalAlign();
    return va == EVerticalAlign::kBaseline ||
           va == EVerticalAlign::kTextBottom ||
           va == EVerticalAlign::kTextTop || va == EVerticalAlign::kSuper ||
           va == EVerticalAlign::kSub || va == EVerticalAlign::kLength;
  }

  // Align the cell in the block direction. This is done by calculating an
  // intrinsic padding before and after the cell contents, so that all cells in
  // the row get the same logical height.
  void ComputeIntrinsicPadding(int row_height,
                               EVerticalAlign,
                               SubtreeLayoutScope&);

  void ClearIntrinsicPadding() { SetIntrinsicPadding(0, 0); }

  int IntrinsicPaddingBefore() const { return intrinsic_padding_before_; }
  int IntrinsicPaddingAfter() const { return intrinsic_padding_after_; }

  LayoutUnit PaddingTop() const override;
  LayoutUnit PaddingBottom() const override;
  LayoutUnit PaddingLeft() const override;
  LayoutUnit PaddingRight() const override;

  // FIXME: For now we just assume the cell has the same block flow direction as
  // the table. It's likely we'll create an extra anonymous LayoutBlock to
  // handle mixing directionality anyway, in which case we can lock the block
  // flow directionality of the cells to the table's directionality.
  LayoutUnit PaddingBefore() const override;
  LayoutUnit PaddingAfter() const override;

  void SetOverrideLogicalContentHeightFromRowHeight(LayoutUnit);

  void ScrollbarsChanged(bool horizontal_scrollbar_changed,
                         bool vertical_scrollbar_changed,
                         ScrollbarChangeContext = kLayout) override;

  bool CellWidthChanged() const { return cell_width_changed_; }
  void SetCellWidthChanged(bool b = true) { cell_width_changed_ = b; }

  static LayoutTableCell* CreateAnonymous(Document*);
  static LayoutTableCell* CreateAnonymousWithParent(const LayoutObject*);
  LayoutBox* CreateAnonymousBoxWithSameTypeAs(
      const LayoutObject* parent) const override {
    return CreateAnonymousWithParent(parent);
  }

  // The table's style determines cell order and cell adjacency in the table.
  // Collapsed borders also use in table's inline and block directions.
  const ComputedStyle& TableStyle() const { return Table()->StyleRef(); }

  BorderValue BorderStartInTableDirection() const {
    return StyleRef().BorderStartUsing(TableStyle());
  }
  BorderValue BorderEndInTableDirection() const {
    return StyleRef().BorderEndUsing(TableStyle());
  }
  BorderValue BorderBeforeInTableDirection() const {
    return StyleRef().BorderBeforeUsing(TableStyle());
  }
  BorderValue BorderAfterInTableDirection() const {
    return StyleRef().BorderAfterUsing(TableStyle());
  }

  const char* GetName() const override { return "LayoutTableCell"; }

  bool BackgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const override;

  const CollapsedBorderValues* GetCollapsedBorderValues() const {
    UpdateCollapsedBorderValues();
    return collapsed_border_values_.get();
  }
  void InvalidateCollapsedBorderValues() {
    collapsed_border_values_valid_ = false;
  }

  LayoutRect DebugRect() const override;

  void AdjustChildDebugRect(LayoutRect&) const override;

  // A table cell's location is relative to its containing section.
  LayoutBox* LocationContainer() const override { return Section(); }

  bool HasLineIfEmpty() const override;

  static bool CompareInDOMOrder(const LayoutTableCell* cell1,
                                const LayoutTableCell* cell2) {
    DCHECK(cell1->Section() == cell2->Section());
    if (cell1->RowIndex() == cell2->RowIndex())
      return cell1->absolute_column_index_ < cell2->absolute_column_index_;
    return cell1->RowIndex() < cell2->RowIndex();
  }

  // For the following methods, the 'start', 'end', 'before', 'after' directions
  // are all in the table's inline and block directions.
  unsigned CollapsedOuterBorderBefore() const {
    return CollapsedBorderHalfBefore(true);
  }
  unsigned CollapsedOuterBorderAfter() const {
    return CollapsedBorderHalfAfter(true);
  }
  unsigned CollapsedOuterBorderStart() const {
    return CollapsedBorderHalfStart(true);
  }
  unsigned CollapsedOuterBorderEnd() const {
    return CollapsedBorderHalfEnd(true);
  }
  unsigned CollapsedInnerBorderBefore() const {
    return CollapsedBorderHalfBefore(false);
  }
  unsigned CollapsedInnerBorderAfter() const {
    return CollapsedBorderHalfAfter(false);
  }
  unsigned CollapsedInnerBorderStart() const {
    return CollapsedBorderHalfStart(false);
  }
  unsigned CollapsedInnerBorderEnd() const {
    return CollapsedBorderHalfEnd(false);
  }

  bool StartsAtSameColumn(const LayoutTableCell* other) const {
    return other && AbsoluteColumnIndex() == other->AbsoluteColumnIndex();
  }
  bool EndsAtSameColumn(const LayoutTableCell* other) const {
    return other && AbsoluteColumnIndex() + ColSpan() ==
                        other->AbsoluteColumnIndex() + other->ColSpan();
  }
  bool StartsAtSameRow(const LayoutTableCell* other) const {
    return other && RowIndex() == other->RowIndex();
  }
  bool EndsAtSameRow(const LayoutTableCell* other) const {
    return other &&
           RowIndex() + RowSpan() == other->RowIndex() + other->RowSpan();
  }

 protected:
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;
  void ComputePreferredLogicalWidths() override;

  void AddLayerHitTestRects(LayerHitTestRects&,
                            const PaintLayer* current_composited_layer,
                            const LayoutPoint& layer_offset,
                            const LayoutRect& container_rect) const override;

  PaintInvalidationReason InvalidatePaint(
      const PaintInvalidatorContext&) const override;

 private:
  friend class LayoutTableCellTest;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectTableCell || LayoutBlockFlow::IsOfType(type);
  }

  void WillBeRemovedFromTree() override;

  void UpdateLogicalWidth() override;

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint&) const override;
  void PaintMask(const PaintInfo&, const LayoutPoint&) const override;

  LayoutSize OffsetFromContainer(const LayoutObject*) const override;

  void ComputeOverflow(LayoutUnit old_client_after_edge,
                       bool recompute_floats = false) override;

  using CollapsedBorderValuesMethod =
      const CollapsedBorderValue& (CollapsedBorderValues::*)() const;
  LogicalToPhysical<CollapsedBorderValuesMethod>
  CollapsedBorderValuesMethodsPhysical() const {
    return LogicalToPhysical<CollapsedBorderValuesMethod>(
        // Collapsed border logical directions are in table's directions.
        TableStyle().GetWritingMode(), TableStyle().Direction(),
        &CollapsedBorderValues::StartBorder, &CollapsedBorderValues::EndBorder,
        &CollapsedBorderValues::BeforeBorder,
        &CollapsedBorderValues::AfterBorder);
  }

  // Give the extra pixel of half collapsed border to top and left.
  static constexpr bool kInnerHalfPixelAsOneTop = true;
  static constexpr bool kInnerHalfPixelAsOneRight = false;
  static constexpr bool kInnerHalfPixelAsOneBottom = false;
  static constexpr bool kInnerHalfPixelAsOneLeft = true;

  PhysicalToLogical<bool> InnerHalfPixelAsOneLogical() const {
    return PhysicalToLogical<bool>(
        // Collapsed border logical directions are in table's directions.
        TableStyle().GetWritingMode(), TableStyle().Direction(),
        kInnerHalfPixelAsOneTop, kInnerHalfPixelAsOneRight,
        kInnerHalfPixelAsOneBottom, kInnerHalfPixelAsOneLeft);
  }

  unsigned CollapsedBorderHalfLeft(bool outer) const {
    return CollapsedBorderHalf(kInnerHalfPixelAsOneLeft ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Left());
  }
  unsigned CollapsedBorderHalfRight(bool outer) const {
    return CollapsedBorderHalf(kInnerHalfPixelAsOneRight ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Right());
  }
  unsigned CollapsedBorderHalfTop(bool outer) const {
    return CollapsedBorderHalf(kInnerHalfPixelAsOneTop ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Top());
  }
  unsigned CollapsedBorderHalfBottom(bool outer) const {
    return CollapsedBorderHalf(kInnerHalfPixelAsOneBottom ^ outer,
                               CollapsedBorderValuesMethodsPhysical().Bottom());
  }

  // For the following methods, the 'start', 'end', 'before', 'after' directions
  // are all in the table's inline and block directions.
  unsigned CollapsedBorderHalfStart(bool outer) const {
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().Start() ^ outer,
                               &CollapsedBorderValues::StartBorder);
  }
  unsigned CollapsedBorderHalfEnd(bool outer) const {
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().End() ^ outer,
                               &CollapsedBorderValues::EndBorder);
  }
  unsigned CollapsedBorderHalfBefore(bool outer) const {
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().Before() ^ outer,
                               &CollapsedBorderValues::BeforeBorder);
  }
  unsigned CollapsedBorderHalfAfter(bool outer) const {
    return CollapsedBorderHalf(InnerHalfPixelAsOneLogical().After() ^ outer,
                               &CollapsedBorderValues::AfterBorder);
  }

  unsigned CollapsedBorderHalf(bool half_pixel_as_one,
                               CollapsedBorderValuesMethod m) const {
    UpdateCollapsedBorderValues();
    if (const auto* values = GetCollapsedBorderValues())
      return ((values->*m)().Width() + (half_pixel_as_one ? 1 : 0)) / 2;
    return 0;
  }

  void SetIntrinsicPaddingBefore(int p) { intrinsic_padding_before_ = p; }
  void SetIntrinsicPaddingAfter(int p) { intrinsic_padding_after_ = p; }
  void SetIntrinsicPadding(int before, int after) {
    SetIntrinsicPaddingBefore(before);
    SetIntrinsicPaddingAfter(after);
  }

  bool IsInStartColumn() const { return AbsoluteColumnIndex() == 0; }
  bool IsInEndColumn() const;

  // These functions implement the CSS collapsing border conflict resolution
  // algorithm http://www.w3.org/TR/CSS2/tables.html#border-conflict-resolution.
  // They are called during UpdateCollapsedBorderValues(). The 'start', 'end',
  // 'before', 'after' directions are all in the table's inline and block
  // directions.
  inline CSSPropertyID ResolveBorderProperty(CSSPropertyID) const;
  CollapsedBorderValue ComputeCollapsedStartBorder() const;
  CollapsedBorderValue ComputeCollapsedEndBorder() const;
  CollapsedBorderValue ComputeCollapsedBeforeBorder() const;
  CollapsedBorderValue ComputeCollapsedAfterBorder() const;

  void UpdateCollapsedBorderValues() const;

  Length LogicalWidthFromColumns(LayoutTableCol* first_col_for_this_cell,
                                 Length width_from_style) const;

  void UpdateColAndRowSpanFlags();

  unsigned ParseRowSpanFromDOM() const;
  unsigned ParseColSpanFromDOM() const;

  void NextSibling() const = delete;
  void PreviousSibling() const = delete;

  unsigned absolute_column_index_ : BITS_OF_ABSOLUTE_COLUMN_INDEX;

  // When adding or removing bits here, we should also adjust
  // BITS_OF_ABSOLUTE_COLUMN_INDEX to use remaining bits of a 32-bit word.
  // Note MSVC will only pack members if they have identical types, hence we use
  // unsigned instead of bool here.
  unsigned cell_width_changed_ : 1;
  unsigned has_col_span_ : 1;
  unsigned has_row_span_ : 1;

  // This is set when collapsed_border_values_ needs recalculation.
  mutable unsigned collapsed_border_values_valid_ : 1;
  mutable std::unique_ptr<CollapsedBorderValues> collapsed_border_values_;

  // The intrinsic padding.
  // See class comment for what they are.
  //
  // Note: Those fields are using non-subpixel units (int)
  // because we don't do fractional arithmetic on tables.
  int intrinsic_padding_before_;
  int intrinsic_padding_after_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTableCell, IsTableCell());

inline LayoutTableCell* LayoutTableCell::PreviousCell() const {
  return ToLayoutTableCell(LayoutObject::PreviousSibling());
}

inline LayoutTableCell* LayoutTableCell::NextCell() const {
  return ToLayoutTableCell(LayoutObject::NextSibling());
}

inline LayoutTableCell* LayoutTableRow::FirstCell() const {
  return ToLayoutTableCell(FirstChild());
}

inline LayoutTableCell* LayoutTableRow::LastCell() const {
  return ToLayoutTableCell(LastChild());
}

}  // namespace blink

#endif  // LayoutTableCell_h
