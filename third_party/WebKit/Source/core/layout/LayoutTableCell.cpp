/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc.
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

#include "core/layout/LayoutTableCell.h"

#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/editing/EditingUtilities.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/TableCellPainter.h"
#include "core/style/CollapsedBorderValue.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/TransformState.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

using namespace HTMLNames;

struct SameSizeAsLayoutTableCell : public LayoutBlockFlow {
  unsigned bitfields;
  int paddings[2];
  void* pointer1;
};

static_assert(sizeof(LayoutTableCell) == sizeof(SameSizeAsLayoutTableCell),
              "LayoutTableCell should stay small");
static_assert(sizeof(CollapsedBorderValue) == 8,
              "CollapsedBorderValue should stay small");

LayoutTableCell::LayoutTableCell(Element* element)
    : LayoutBlockFlow(element),
      absolute_column_index_(kUnsetColumnIndex),
      cell_width_changed_(false),
      intrinsic_padding_before_(0),
      intrinsic_padding_after_(0) {
  // We only update the flags when notified of DOM changes in
  // colSpanOrRowSpanChanged() so we need to set their initial values here in
  // case something asks for colSpan()/rowSpan() before then.
  UpdateColAndRowSpanFlags();
}

LayoutTableCell::CollapsedBorderValues::CollapsedBorderValues(
    const LayoutTableCell& layout_table_cell,
    const CollapsedBorderValue& start_border,
    const CollapsedBorderValue& end_border,
    const CollapsedBorderValue& before_border,
    const CollapsedBorderValue& after_border)
    : layout_table_cell_(layout_table_cell),
      start_border_(start_border),
      end_border_(end_border),
      before_border_(before_border),
      after_border_(after_border) {}

void LayoutTableCell::CollapsedBorderValues::SetCollapsedBorderValues(
    const CollapsedBorderValues& other) {
  start_border_ = other.StartBorder();
  end_border_ = other.EndBorder();
  before_border_ = other.BeforeBorder();
  after_border_ = other.AfterBorder();
}

String LayoutTableCell::CollapsedBorderValues::DebugName() const {
  return "CollapsedBorderValues";
}

LayoutRect LayoutTableCell::CollapsedBorderValues::VisualRect() const {
  return layout_table_cell_.Table()->VisualRect();
}

void LayoutTableCell::WillBeRemovedFromTree() {
  LayoutBlockFlow::WillBeRemovedFromTree();

  Section()->SetNeedsCellRecalc();

  // When borders collapse, removing a cell can affect the the width of
  // neighboring cells.
  LayoutTable* enclosing_table = Table();
  DCHECK(enclosing_table);
  if (!enclosing_table->CollapseBorders())
    return;
  if (PreviousCell()) {
    // TODO(dgrogan): Should this be setChildNeedsLayout or setNeedsLayout?
    // remove-cell-with-border-box.html only passes with setNeedsLayout but
    // other places use setChildNeedsLayout.
    PreviousCell()->SetNeedsLayout(LayoutInvalidationReason::kTableChanged);
    PreviousCell()->SetPreferredLogicalWidthsDirty();
  }
  if (NextCell()) {
    // TODO(dgrogan): Same as above re: setChildNeedsLayout vs setNeedsLayout.
    NextCell()->SetNeedsLayout(LayoutInvalidationReason::kTableChanged);
    NextCell()->SetPreferredLogicalWidthsDirty();
  }
}

unsigned LayoutTableCell::ParseColSpanFromDOM() const {
  DCHECK(GetNode());
  // TODO(dgrogan): HTMLTableCellElement::colSpan() already clamps to something
  // smaller than maxColumnIndex; can we just DCHECK here?
  if (IsHTMLTableCellElement(*GetNode()))
    return std::min<unsigned>(ToHTMLTableCellElement(*GetNode()).colSpan(),
                              kMaxColumnIndex);
  return 1;
}

unsigned LayoutTableCell::ParseRowSpanFromDOM() const {
  DCHECK(GetNode());
  if (IsHTMLTableCellElement(*GetNode()))
    return std::min<unsigned>(ToHTMLTableCellElement(*GetNode()).rowSpan(),
                              kMaxRowIndex);
  return 1;
}

void LayoutTableCell::UpdateColAndRowSpanFlags() {
  // The vast majority of table cells do not have a colspan or rowspan,
  // so we keep a bool to know if we need to bother reading from the DOM.
  has_col_span_ = GetNode() && ParseColSpanFromDOM() != 1;
  has_row_span_ = GetNode() && ParseRowSpanFromDOM() != 1;
}

void LayoutTableCell::ColSpanOrRowSpanChanged() {
  DCHECK(GetNode());
  DCHECK(IsHTMLTableCellElement(*GetNode()));

  UpdateColAndRowSpanFlags();

  SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
      LayoutInvalidationReason::kAttributeChanged);
  if (Parent() && Section())
    Section()->SetNeedsCellRecalc();
}

Length LayoutTableCell::LogicalWidthFromColumns(
    LayoutTableCol* first_col_for_this_cell,
    Length width_from_style) const {
  DCHECK(first_col_for_this_cell);
  DCHECK_EQ(first_col_for_this_cell,
            Table()
                ->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
                .InnermostColOrColGroup());
  LayoutTableCol* table_col = first_col_for_this_cell;

  unsigned col_span_count = ColSpan();
  int col_width_sum = 0;
  for (unsigned i = 1; i <= col_span_count; i++) {
    Length col_width = table_col->Style()->LogicalWidth();

    // Percentage value should be returned only for colSpan == 1.
    // Otherwise we return original width for the cell.
    if (!col_width.IsFixed()) {
      if (col_span_count > 1)
        return width_from_style;
      return col_width;
    }

    col_width_sum += col_width.Value();
    table_col = table_col->NextColumn();
    // If no next <col> tag found for the span we just return what we have for
    // now.
    if (!table_col)
      break;
  }

  // Column widths specified on <col> apply to the border box of the cell, see
  // bug 8126.
  // FIXME: Why is border/padding ignored in the negative width case?
  if (col_width_sum > 0)
    return Length(
        std::max(0, col_width_sum - BorderAndPaddingLogicalWidth().Ceil()),
        kFixed);
  return Length(col_width_sum, kFixed);
}

void LayoutTableCell::ComputePreferredLogicalWidths() {
  // The child cells rely on the grids up in the sections to do their
  // computePreferredLogicalWidths work.  Normally the sections are set up
  // early, as table cells are added, but relayout can cause the cells to be
  // freed, leaving stale pointers in the sections' grids. We must refresh those
  // grids before the child cells try to use them.
  Table()->RecalcSectionsIfNeeded();

  LayoutBlockFlow::ComputePreferredLogicalWidths();
  if (GetNode() && Style()->AutoWrap()) {
    // See if nowrap was set.
    Length w = StyleOrColLogicalWidth();
    const AtomicString& nowrap = ToElement(GetNode())->getAttribute(nowrapAttr);
    if (!nowrap.IsNull() && w.IsFixed()) {
      // Nowrap is set, but we didn't actually use it because of the fixed width
      // set on the cell. Even so, it is a WinIE/Moz trait to make the minwidth
      // of the cell into the fixed width. They do this even in strict mode, so
      // do not make this a quirk. Affected the top of hiptop.com.
      min_preferred_logical_width_ =
          std::max(LayoutUnit(w.Value()), min_preferred_logical_width_);
    }
  }
}

void LayoutTableCell::AddLayerHitTestRects(
    LayerHitTestRects& layer_rects,
    const PaintLayer* current_layer,
    const LayoutPoint& layer_offset,
    const LayoutRect& container_rect) const {
  LayoutPoint adjusted_layer_offset = layer_offset;
  // LayoutTableCell's location includes the offset of it's containing
  // LayoutTableRow, so we need to subtract that again here (as for
  // LayoutTableCell::offsetFromContainer.
  if (Parent())
    adjusted_layer_offset -= ParentBox()->LocationOffset();
  LayoutBox::AddLayerHitTestRects(layer_rects, current_layer,
                                  adjusted_layer_offset, container_rect);
}

void LayoutTableCell::ComputeIntrinsicPadding(int row_height,
                                              EVerticalAlign vertical_align,
                                              SubtreeLayoutScope& layouter) {
  int old_intrinsic_padding_before = IntrinsicPaddingBefore();
  int old_intrinsic_padding_after = IntrinsicPaddingAfter();
  int logical_height_without_intrinsic_padding = PixelSnappedLogicalHeight() -
                                                 old_intrinsic_padding_before -
                                                 old_intrinsic_padding_after;

  int intrinsic_padding_before = 0;
  switch (vertical_align) {
    case EVerticalAlign::kSub:
    case EVerticalAlign::kSuper:
    case EVerticalAlign::kTextTop:
    case EVerticalAlign::kTextBottom:
    case EVerticalAlign::kLength:
    case EVerticalAlign::kBaseline: {
      int baseline = CellBaselinePosition();
      if (baseline > BorderBefore() + PaddingBefore())
        intrinsic_padding_before = Section()->RowBaseline(RowIndex()) -
                                   (baseline - old_intrinsic_padding_before);
      break;
    }
    case EVerticalAlign::kTop:
      break;
    case EVerticalAlign::kMiddle:
      intrinsic_padding_before =
          (row_height - logical_height_without_intrinsic_padding) / 2;
      break;
    case EVerticalAlign::kBottom:
      intrinsic_padding_before =
          row_height - logical_height_without_intrinsic_padding;
      break;
    case EVerticalAlign::kBaselineMiddle:
      break;
  }

  int intrinsic_padding_after = row_height -
                                logical_height_without_intrinsic_padding -
                                intrinsic_padding_before;
  SetIntrinsicPaddingBefore(intrinsic_padding_before);
  SetIntrinsicPaddingAfter(intrinsic_padding_after);

  // FIXME: Changing an intrinsic padding shouldn't trigger a relayout as it
  // only shifts the cell inside the row but doesn't change the logical height.
  if (intrinsic_padding_before != old_intrinsic_padding_before ||
      intrinsic_padding_after != old_intrinsic_padding_after)
    layouter.SetNeedsLayout(this, LayoutInvalidationReason::kPaddingChanged);
}

void LayoutTableCell::UpdateLogicalWidth() {}

void LayoutTableCell::SetCellLogicalWidth(int table_layout_logical_width,
                                          SubtreeLayoutScope& layouter) {
  if (table_layout_logical_width == LogicalWidth())
    return;

  layouter.SetNeedsLayout(this, LayoutInvalidationReason::kSizeChanged);

  SetLogicalWidth(LayoutUnit(table_layout_logical_width));
  SetCellWidthChanged(true);
}

void LayoutTableCell::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  int old_cell_baseline = CellBaselinePosition();
  UpdateBlockLayout(CellWidthChanged());

  // If we have replaced content, the intrinsic height of our content may have
  // changed since the last time we laid out. If that's the case the intrinsic
  // padding we used for layout (the padding required to push the contents of
  // the cell down to the row's baseline) is included in our new height and
  // baseline and makes both of them wrong. So if our content's intrinsic height
  // has changed push the new content up into the intrinsic padding and relayout
  // so that the rest of table and row layout can use the correct baseline and
  // height for this cell.
  if (IsBaselineAligned() && Section()->RowBaseline(RowIndex()) &&
      CellBaselinePosition() > Section()->RowBaseline(RowIndex())) {
    int new_intrinsic_padding_before =
        std::max(IntrinsicPaddingBefore() -
                     std::max(CellBaselinePosition() - old_cell_baseline, 0),
                 0);
    SetIntrinsicPaddingBefore(new_intrinsic_padding_before);
    SubtreeLayoutScope layouter(*this);
    layouter.SetNeedsLayout(this, LayoutInvalidationReason::kTableChanged);
    UpdateBlockLayout(CellWidthChanged());
  }

  // FIXME: This value isn't the intrinsic content logical height, but we need
  // to update the value as its used by flexbox layout. crbug.com/367324
  SetIntrinsicContentLogicalHeight(ContentLogicalHeight());

  SetCellWidthChanged(false);
}

LayoutUnit LayoutTableCell::PaddingTop() const {
  LayoutUnit result = ComputedCSSPaddingTop();
  if (IsHorizontalWritingMode())
    result += (blink::IsHorizontalWritingMode(Style()->GetWritingMode())
                   ? IntrinsicPaddingBefore()
                   : IntrinsicPaddingAfter());
  // TODO(leviw): The floor call should be removed when Table is sub-pixel
  // aware. crbug.com/377847
  return LayoutUnit(result.Floor());
}

LayoutUnit LayoutTableCell::PaddingBottom() const {
  LayoutUnit result = ComputedCSSPaddingBottom();
  if (IsHorizontalWritingMode())
    result += (blink::IsHorizontalWritingMode(Style()->GetWritingMode())
                   ? IntrinsicPaddingAfter()
                   : IntrinsicPaddingBefore());
  // TODO(leviw): The floor call should be removed when Table is sub-pixel
  // aware. crbug.com/377847
  return LayoutUnit(result.Floor());
}

LayoutUnit LayoutTableCell::PaddingLeft() const {
  LayoutUnit result = ComputedCSSPaddingLeft();
  if (!IsHorizontalWritingMode())
    result += (IsFlippedLinesWritingMode(Style()->GetWritingMode())
                   ? IntrinsicPaddingBefore()
                   : IntrinsicPaddingAfter());
  // TODO(leviw): The floor call should be removed when Table is sub-pixel
  // aware. crbug.com/377847
  return LayoutUnit(result.Floor());
}

LayoutUnit LayoutTableCell::PaddingRight() const {
  LayoutUnit result = ComputedCSSPaddingRight();
  if (!IsHorizontalWritingMode())
    result += (IsFlippedLinesWritingMode(Style()->GetWritingMode())
                   ? IntrinsicPaddingAfter()
                   : IntrinsicPaddingBefore());
  // TODO(leviw): The floor call should be removed when Table is sub-pixel
  // aware. crbug.com/377847
  return LayoutUnit(result.Floor());
}

LayoutUnit LayoutTableCell::PaddingBefore() const {
  return LayoutUnit(ComputedCSSPaddingBefore().Floor() +
                    IntrinsicPaddingBefore());
}

LayoutUnit LayoutTableCell::PaddingAfter() const {
  return LayoutUnit(ComputedCSSPaddingAfter().Floor() +
                    IntrinsicPaddingAfter());
}

void LayoutTableCell::SetOverrideLogicalContentHeightFromRowHeight(
    LayoutUnit row_height) {
  ClearIntrinsicPadding();
  SetOverrideLogicalContentHeight(
      (row_height - CollapsedBorderAndCSSPaddingLogicalHeight())
          .ClampNegativeToZero());
}

LayoutSize LayoutTableCell::OffsetFromContainer(const LayoutObject* o) const {
  DCHECK_EQ(o, Container());

  LayoutSize offset = LayoutBlockFlow::OffsetFromContainer(o);
  if (Parent())
    offset -= ParentBox()->LocationOffset();

  return offset;
}

LayoutRect LayoutTableCell::LocalVisualRect() const {
  // If the table grid is dirty, we cannot get reliable information about
  // adjoining cells, so we ignore outside borders. This should not be a problem
  // because it means that the table is going to recalculate the grid, relayout
  // and issue a paint invalidation of its current rect, which includes any
  // outside borders of this cell.
  if (!Table()->CollapseBorders() || Table()->NeedsSectionRecalc())
    return LayoutBlockFlow::LocalVisualRect();

  bool rtl = !StyleForCellFlow().IsLeftToRightDirection();
  LayoutUnit outline_outset(Style()->OutlineOutsetExtent());
  LayoutUnit left(std::max(BorderHalfLeft(true), outline_outset));
  LayoutUnit right(std::max(BorderHalfRight(true), outline_outset));
  LayoutUnit top(std::max(BorderHalfTop(true), outline_outset));
  LayoutUnit bottom(std::max(BorderHalfBottom(true), outline_outset));
  if ((left && !rtl) || (right && rtl)) {
    if (LayoutTableCell* before = Table()->CellBefore(this)) {
      top = std::max(top, before->BorderHalfTop(true));
      bottom = std::max(bottom, before->BorderHalfBottom(true));
    }
  }
  if ((left && rtl) || (right && !rtl)) {
    if (LayoutTableCell* after = Table()->CellAfter(this)) {
      top = std::max(top, after->BorderHalfTop(true));
      bottom = std::max(bottom, after->BorderHalfBottom(true));
    }
  }
  if (top) {
    if (LayoutTableCell* above = Table()->CellAbove(this)) {
      left = std::max(left, above->BorderHalfLeft(true));
      right = std::max(right, above->BorderHalfRight(true));
    }
  }
  if (bottom) {
    if (LayoutTableCell* below = Table()->CellBelow(this)) {
      left = std::max(left, below->BorderHalfLeft(true));
      right = std::max(right, below->BorderHalfRight(true));
    }
  }

  LayoutRect self_visual_overflow_rect = this->SelfVisualOverflowRect();
  LayoutPoint location(
      std::max(LayoutUnit(left), -self_visual_overflow_rect.X()),
      std::max(LayoutUnit(top), -self_visual_overflow_rect.Y()));
  return LayoutRect(-location.X(), -location.Y(),
                    location.X() + std::max(Size().Width() + right,
                                            self_visual_overflow_rect.MaxX()),
                    location.Y() + std::max(Size().Height() + bottom,
                                            self_visual_overflow_rect.MaxY()));
}

int LayoutTableCell::CellBaselinePosition() const {
  // <http://www.w3.org/TR/2007/CR-CSS21-20070719/tables.html#height-layout>:
  // The baseline of a cell is the baseline of the first in-flow line box in the
  // cell, or the first in-flow table-row in the cell, whichever comes first. If
  // there is no such line box or table-row, the baseline is the bottom of
  // content edge of the cell box.
  int first_line_baseline = FirstLineBoxBaseline();
  if (first_line_baseline != -1)
    return first_line_baseline;
  return (BorderBefore() + PaddingBefore() + ContentLogicalHeight()).ToInt();
}

void LayoutTableCell::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  DCHECK_EQ(Style()->Display(), EDisplay::kTableCell);

  LayoutBlockFlow::StyleDidChange(diff, old_style);
  SetHasBoxDecorationBackground(true);

  if (Parent() && Section() && old_style &&
      Style()->Height() != old_style->Height())
    Section()->RowLogicalHeightChanged(Row());

  // Our intrinsic padding pushes us down to align with the baseline of other
  // cells on the row. If our vertical-align has changed then so will the
  // padding needed to align with other cells - clear it so we can recalculate
  // it from scratch.
  if (old_style && Style()->VerticalAlign() != old_style->VerticalAlign())
    ClearIntrinsicPadding();

  // If border was changed, notify table.
  if (!Parent())
    return;
  LayoutTable* table = this->Table();
  if (!table)
    return;
  if (!table->SelfNeedsLayout() && !table->NormalChildNeedsLayout() &&
      old_style && old_style->Border() != Style()->Border())
    table->InvalidateCollapsedBorders();

  if (LayoutTableBoxComponent::DoCellsHaveDirtyWidth(*this, *table, diff,
                                                     *old_style)) {
    if (PreviousCell()) {
      // TODO(dgrogan) Add a layout test showing that setChildNeedsLayout is
      // needed instead of setNeedsLayout.
      PreviousCell()->SetChildNeedsLayout();
      PreviousCell()->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
    }
    if (NextCell()) {
      // TODO(dgrogan) Add a layout test showing that setChildNeedsLayout is
      // needed instead of setNeedsLayout.
      NextCell()->SetChildNeedsLayout();
      NextCell()->SetPreferredLogicalWidthsDirty(kMarkOnlyThis);
    }
  }
}

// The following rules apply for resolving conflicts and figuring out which
// border to use.
// (1) Borders with the 'border-style' of 'hidden' take precedence over all
//     other conflicting borders. Any border with this value suppresses all
//     borders at this location.
// (2) Borders with a style of 'none' have the lowest priority. Only if the
//     border properties of all the elements meeting at this edge are 'none'
//     will the border be omitted (but note that 'none' is the default value for
//     the border style.)
// (3) If none of the styles are 'hidden' and at least one of them is not
//     'none', then narrow borders are discarded in favor of wider ones. If
//      several have the same 'border-width' then styles are preferred in this
//      order: 'double', 'solid', 'dashed', 'dotted', 'ridge', 'outset',
//     'groove', and the lowest: 'inset'.
// (4) If border styles differ only in color, then a style set on a cell wins
//     over one on a row, which wins over a row group, column, column group and,
//     lastly, table. It is undefined which color is used when two elements of
//     the same type disagree.
static bool CompareBorders(const CollapsedBorderValue& border1,
                           const CollapsedBorderValue& border2) {
  // Sanity check the values passed in. The null border have lowest priority.
  if (!border2.Exists())
    return false;
  if (!border1.Exists())
    return true;

  // Rule #1 above.
  if (border1.Style() == kBorderStyleHidden)
    return false;
  if (border2.Style() == kBorderStyleHidden)
    return true;

  // Rule #2 above.  A style of 'none' has lowest priority and always loses to
  // any other border.
  if (border2.Style() == kBorderStyleNone)
    return false;
  if (border1.Style() == kBorderStyleNone)
    return true;

  // The first part of rule #3 above. Wider borders win.
  if (border1.Width() != border2.Width())
    return border1.Width() < border2.Width();

  // The borders have equal width.  Sort by border style.
  if (border1.Style() != border2.Style())
    return border1.Style() < border2.Style();

  // The border have the same width and style.  Rely on precedence (cell over
  // row over row group, etc.)
  return border1.Precedence() < border2.Precedence();
}

static CollapsedBorderValue ChooseBorder(const CollapsedBorderValue& border1,
                                         const CollapsedBorderValue& border2) {
  return CompareBorders(border1, border2) ? border2 : border1;
}

bool LayoutTableCell::HasStartBorderAdjoiningTable() const {
  bool is_start_column = !AbsoluteColumnIndex();
  bool is_end_column = Table()->AbsoluteColumnToEffectiveColumn(
                           AbsoluteColumnIndex() + ColSpan() - 1) ==
                       Table()->NumEffectiveColumns() - 1;
  bool has_same_direction_as_table = HasSameDirectionAs(Table());

  // The table direction determines the row direction. In mixed directionality,
  // we cannot guarantee that we have a common border with the table (think a
  // ltr table with rtl start cell).
  return (is_start_column && has_same_direction_as_table) ||
         (is_end_column && !has_same_direction_as_table);
}

bool LayoutTableCell::HasEndBorderAdjoiningTable() const {
  bool is_start_column = !AbsoluteColumnIndex();
  bool is_end_column = Table()->AbsoluteColumnToEffectiveColumn(
                           AbsoluteColumnIndex() + ColSpan() - 1) ==
                       Table()->NumEffectiveColumns() - 1;
  bool has_same_direction_as_table = HasSameDirectionAs(Table());

  // The table direction determines the row direction. In mixed directionality,
  // we cannot guarantee that we have a common border with the table (think a
  // ltr table with ltr end cell).
  return (is_start_column && !has_same_direction_as_table) ||
         (is_end_column && has_same_direction_as_table);
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedStartBorder(
    IncludeBorderColorOrNot include_color) const {
  LayoutTable* table = this->Table();

  // For the start border, we need to check, in order of precedence:
  // (1) Our start border.
  int start_color_property = include_color
                                 ? CSSProperty::ResolveDirectionAwareProperty(
                                       CSSPropertyWebkitBorderStartColor,
                                       StyleForCellFlow().Direction(),
                                       StyleForCellFlow().GetWritingMode())
                                 : 0;
  int end_color_property =
      include_color
          ? CSSProperty::ResolveDirectionAwareProperty(
                CSSPropertyWebkitBorderEndColor, StyleForCellFlow().Direction(),
                StyleForCellFlow().GetWritingMode())
          : 0;
  CollapsedBorderValue result(
      Style()->BorderStart(),
      include_color ? ResolveColor(start_color_property) : Color(),
      kBorderPrecedenceCell);

  // (2) The end border of the preceding cell.
  LayoutTableCell* cell_before = table->CellBefore(this);
  if (cell_before) {
    CollapsedBorderValue cell_before_adjoining_border = CollapsedBorderValue(
        cell_before->BorderAdjoiningCellAfter(this),
        include_color ? cell_before->ResolveColor(end_color_property) : Color(),
        kBorderPrecedenceCell);
    // |result| should be the 2nd argument as |cellBefore| should win in case of
    // equality per CSS 2.1 (Border conflict resolution, point 4).
    result = ChooseBorder(cell_before_adjoining_border, result);
    if (!result.Exists())
      return result;
  }

  bool start_border_adjoins_table = HasStartBorderAdjoiningTable();
  if (start_border_adjoins_table) {
    // (3) Our row's start border.
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    Row()->BorderAdjoiningStartCell(this),
                    include_color ? Parent()->ResolveColor(start_color_property)
                                  : Color(),
                    kBorderPrecedenceRow));
    if (!result.Exists())
      return result;

    // (4) Our row group's start border.
    result = ChooseBorder(
        result, CollapsedBorderValue(Section()->BorderAdjoiningStartCell(this),
                                     include_color ? Section()->ResolveColor(
                                                         start_color_property)
                                                   : Color(),
                                     kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;
  }

  // (5) Our column and column group's start borders.
  LayoutTable::ColAndColGroup col_and_col_group =
      table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex());
  if (col_and_col_group.colgroup &&
      col_and_col_group.adjoins_start_border_of_col_group) {
    // Only apply the colgroup's border if this cell touches the colgroup edge.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(
            col_and_col_group.colgroup->BorderAdjoiningCellStartBorder(this),
            include_color
                ? col_and_col_group.colgroup->ResolveColor(start_color_property)
                : Color(),
            kBorderPrecedenceColumnGroup));
    if (!result.Exists())
      return result;
  }
  if (col_and_col_group.col) {
    // Always apply the col's border irrespective of whether this cell touches
    // it. This is per HTML5: "For the purposes of the CSS table model, the col
    // element is expected to be treated as if it "was present as many times as
    // its span attribute specifies".
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    col_and_col_group.col->BorderAdjoiningCellStartBorder(this),
                    include_color ? col_and_col_group.col->ResolveColor(
                                        start_color_property)
                                  : Color(),
                    kBorderPrecedenceColumn));
    if (!result.Exists())
      return result;
  }

  // (6) The end border of the preceding column.
  if (cell_before) {
    LayoutTable::ColAndColGroup col_and_col_group =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex() - 1);
    // Only apply the colgroup's border if this cell touches the colgroup edge.
    if (col_and_col_group.colgroup &&
        col_and_col_group.adjoins_end_border_of_col_group) {
      result = ChooseBorder(
          CollapsedBorderValue(
              col_and_col_group.colgroup->BorderAdjoiningCellEndBorder(this),
              include_color
                  ? col_and_col_group.colgroup->ResolveColor(end_color_property)
                  : Color(),
              kBorderPrecedenceColumnGroup),
          result);
      if (!result.Exists())
        return result;
    }
    // Always apply the col's border irrespective of whether this cell touches
    // it. This is per HTML5: "For the purposes of the CSS table model, the col
    // element is expected to be treated as if it "was present as many times as
    // its span attribute specifies".
    if (col_and_col_group.col) {
      result = ChooseBorder(
          CollapsedBorderValue(
              col_and_col_group.col->BorderAdjoiningCellAfter(this),
              include_color
                  ? col_and_col_group.col->ResolveColor(end_color_property)
                  : Color(),
              kBorderPrecedenceColumn),
          result);
      if (!result.Exists())
        return result;
    }
  }

  if (start_border_adjoins_table) {
    // (7) The table's start border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(
            table->TableStartBorderAdjoiningCell(this),
            include_color ? table->ResolveColor(start_color_property) : Color(),
            kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedEndBorder(
    IncludeBorderColorOrNot include_color) const {
  LayoutTable* table = this->Table();
  // Note: We have to use the effective column information instead of whether we
  // have a cell after as a table doesn't have to be regular (any row can have
  // less cells than the total cell count).
  bool is_end_column = table->AbsoluteColumnToEffectiveColumn(
                           AbsoluteColumnIndex() + ColSpan() - 1) ==
                       table->NumEffectiveColumns() - 1;

  // For end border, we need to check, in order of precedence:
  // (1) Our end border.
  int start_color_property = include_color
                                 ? CSSProperty::ResolveDirectionAwareProperty(
                                       CSSPropertyWebkitBorderStartColor,
                                       StyleForCellFlow().Direction(),
                                       StyleForCellFlow().GetWritingMode())
                                 : 0;
  int end_color_property =
      include_color
          ? CSSProperty::ResolveDirectionAwareProperty(
                CSSPropertyWebkitBorderEndColor, StyleForCellFlow().Direction(),
                StyleForCellFlow().GetWritingMode())
          : 0;
  CollapsedBorderValue result = CollapsedBorderValue(
      Style()->BorderEnd(),
      include_color ? ResolveColor(end_color_property) : Color(),
      kBorderPrecedenceCell);

  // (2) The start border of the following cell.
  if (!is_end_column) {
    if (LayoutTableCell* cell_after = table->CellAfter(this)) {
      CollapsedBorderValue cell_after_adjoining_border = CollapsedBorderValue(
          cell_after->BorderAdjoiningCellBefore(this),
          include_color ? cell_after->ResolveColor(start_color_property)
                        : Color(),
          kBorderPrecedenceCell);
      result = ChooseBorder(result, cell_after_adjoining_border);
      if (!result.Exists())
        return result;
    }
  }

  bool end_border_adjoins_table = HasEndBorderAdjoiningTable();
  if (end_border_adjoins_table) {
    // (3) Our row's end border.
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    Row()->BorderAdjoiningEndCell(this),
                    include_color ? Parent()->ResolveColor(end_color_property)
                                  : Color(),
                    kBorderPrecedenceRow));
    if (!result.Exists())
      return result;

    // (4) Our row group's end border.
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    Section()->BorderAdjoiningEndCell(this),
                    include_color ? Section()->ResolveColor(end_color_property)
                                  : Color(),
                    kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;
  }

  // (5) Our column and column group's end borders.
  LayoutTable::ColAndColGroup col_and_col_group =
      table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex() + ColSpan() - 1);
  if (col_and_col_group.colgroup &&
      col_and_col_group.adjoins_end_border_of_col_group) {
    // Only apply the colgroup's border if this cell touches the colgroup edge.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(
            col_and_col_group.colgroup->BorderAdjoiningCellEndBorder(this),
            include_color
                ? col_and_col_group.colgroup->ResolveColor(end_color_property)
                : Color(),
            kBorderPrecedenceColumnGroup));
    if (!result.Exists())
      return result;
  }
  if (col_and_col_group.col) {
    // Always apply the col's border irrespective of whether this cell touches
    // it. This is per HTML5: "For the purposes of the CSS table model, the col
    // element is expected to be treated as if it "was present as many times as
    // its span attribute specifies".
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    col_and_col_group.col->BorderAdjoiningCellEndBorder(this),
                    include_color ? col_and_col_group.col->ResolveColor(
                                        end_color_property)
                                  : Color(),
                    kBorderPrecedenceColumn));
    if (!result.Exists())
      return result;
  }

  // (6) The start border of the next column.
  if (!is_end_column) {
    LayoutTable::ColAndColGroup col_and_col_group =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex() + ColSpan());
    if (col_and_col_group.colgroup &&
        col_and_col_group.adjoins_start_border_of_col_group) {
      // Only apply the colgroup's border if this cell touches the colgroup
      // edge.
      result = ChooseBorder(
          result,
          CollapsedBorderValue(
              col_and_col_group.colgroup->BorderAdjoiningCellStartBorder(this),
              include_color ? col_and_col_group.colgroup->ResolveColor(
                                  start_color_property)
                            : Color(),
              kBorderPrecedenceColumnGroup));
      if (!result.Exists())
        return result;
    }
    if (col_and_col_group.col) {
      // Always apply the col's border irrespective of whether this cell touches
      // it. This is per HTML5: "For the purposes of the CSS table model, the
      // col element is expected to be treated as if it "was present as many
      // times as its span attribute specifies".
      result = ChooseBorder(
          result, CollapsedBorderValue(
                      col_and_col_group.col->BorderAdjoiningCellBefore(this),
                      include_color ? col_and_col_group.col->ResolveColor(
                                          start_color_property)
                                    : Color(),
                      kBorderPrecedenceColumn));
      if (!result.Exists())
        return result;
    }
  }

  if (end_border_adjoins_table) {
    // (7) The table's end border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(
            table->TableEndBorderAdjoiningCell(this),
            include_color ? table->ResolveColor(end_color_property) : Color(),
            kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedBeforeBorder(
    IncludeBorderColorOrNot include_color) const {
  LayoutTable* table = this->Table();

  // For before border, we need to check, in order of precedence:
  // (1) Our before border.
  int before_color_property = include_color
                                  ? CSSProperty::ResolveDirectionAwareProperty(
                                        CSSPropertyWebkitBorderBeforeColor,
                                        StyleForCellFlow().Direction(),
                                        StyleForCellFlow().GetWritingMode())
                                  : 0;
  int after_color_property = include_color
                                 ? CSSProperty::ResolveDirectionAwareProperty(
                                       CSSPropertyWebkitBorderAfterColor,
                                       StyleForCellFlow().Direction(),
                                       StyleForCellFlow().GetWritingMode())
                                 : 0;
  CollapsedBorderValue result = CollapsedBorderValue(
      Style()->BorderBefore(),
      include_color ? ResolveColor(before_color_property) : Color(),
      kBorderPrecedenceCell);

  LayoutTableCell* prev_cell = table->CellAbove(this);
  if (prev_cell) {
    // (2) A before cell's after border.
    result = ChooseBorder(
        CollapsedBorderValue(prev_cell->Style()->BorderAfter(),
                             include_color
                                 ? prev_cell->ResolveColor(after_color_property)
                                 : Color(),
                             kBorderPrecedenceCell),
        result);
    if (!result.Exists())
      return result;
  }

  // (3) Our row's before border.
  result = ChooseBorder(
      result, CollapsedBorderValue(
                  Parent()->Style()->BorderBefore(),
                  include_color ? Parent()->ResolveColor(before_color_property)
                                : Color(),
                  kBorderPrecedenceRow));
  if (!result.Exists())
    return result;

  // (4) The previous row's after border.
  if (prev_cell) {
    LayoutObject* prev_row = nullptr;
    if (prev_cell->Section() == Section())
      prev_row = Parent()->PreviousSibling();
    else
      prev_row = prev_cell->Section()->LastRow();

    if (prev_row) {
      result = ChooseBorder(
          CollapsedBorderValue(
              prev_row->Style()->BorderAfter(),
              include_color ? prev_row->ResolveColor(after_color_property)
                            : Color(),
              kBorderPrecedenceRow),
          result);
      if (!result.Exists())
        return result;
    }
  }

  // Now check row groups.
  LayoutTableSection* curr_section = Section();
  if (!RowIndex()) {
    // (5) Our row group's before border.
    result = ChooseBorder(
        result, CollapsedBorderValue(curr_section->Style()->BorderBefore(),
                                     include_color ? curr_section->ResolveColor(
                                                         before_color_property)
                                                   : Color(),
                                     kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;

    // (6) Previous row group's after border.
    curr_section = table->SectionAbove(curr_section, kSkipEmptySections);
    if (curr_section) {
      result = ChooseBorder(
          CollapsedBorderValue(
              curr_section->Style()->BorderAfter(),
              include_color ? curr_section->ResolveColor(after_color_property)
                            : Color(),
              kBorderPrecedenceRowGroup),
          result);
      if (!result.Exists())
        return result;
    }
  }

  if (!curr_section) {
    // (8) Our column and column group's before borders.
    LayoutTableCol* col_elt =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
            .InnermostColOrColGroup();
    if (col_elt) {
      result = ChooseBorder(
          result,
          CollapsedBorderValue(
              col_elt->Style()->BorderBefore(),
              include_color ? col_elt->ResolveColor(before_color_property)
                            : Color(),
              kBorderPrecedenceColumn));
      if (!result.Exists())
        return result;
      if (LayoutTableCol* enclosing_column_group =
              col_elt->EnclosingColumnGroup()) {
        result = ChooseBorder(
            result, CollapsedBorderValue(
                        enclosing_column_group->Style()->BorderBefore(),
                        include_color ? enclosing_column_group->ResolveColor(
                                            before_color_property)
                                      : Color(),
                        kBorderPrecedenceColumnGroup));
        if (!result.Exists())
          return result;
      }
    }

    // (9) The table's before border.
    result = ChooseBorder(
        result, CollapsedBorderValue(
                    table->Style()->BorderBefore(),
                    include_color ? table->ResolveColor(before_color_property)
                                  : Color(),
                    kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

CollapsedBorderValue LayoutTableCell::ComputeCollapsedAfterBorder(
    IncludeBorderColorOrNot include_color) const {
  LayoutTable* table = this->Table();

  // For after border, we need to check, in order of precedence:
  // (1) Our after border.
  int before_color_property = include_color
                                  ? CSSProperty::ResolveDirectionAwareProperty(
                                        CSSPropertyWebkitBorderBeforeColor,
                                        StyleForCellFlow().Direction(),
                                        StyleForCellFlow().GetWritingMode())
                                  : 0;
  int after_color_property = include_color
                                 ? CSSProperty::ResolveDirectionAwareProperty(
                                       CSSPropertyWebkitBorderAfterColor,
                                       StyleForCellFlow().Direction(),
                                       StyleForCellFlow().GetWritingMode())
                                 : 0;
  CollapsedBorderValue result = CollapsedBorderValue(
      Style()->BorderAfter(),
      include_color ? ResolveColor(after_color_property) : Color(),
      kBorderPrecedenceCell);

  LayoutTableCell* next_cell = table->CellBelow(this);
  if (next_cell) {
    // (2) An after cell's before border.
    result = ChooseBorder(
        result, CollapsedBorderValue(next_cell->Style()->BorderBefore(),
                                     include_color ? next_cell->ResolveColor(
                                                         before_color_property)
                                                   : Color(),
                                     kBorderPrecedenceCell));
    if (!result.Exists())
      return result;
  }

  // (3) Our row's after border. (FIXME: Deal with rowspan!)
  result = ChooseBorder(
      result, CollapsedBorderValue(
                  Parent()->Style()->BorderAfter(),
                  include_color ? Parent()->ResolveColor(after_color_property)
                                : Color(),
                  kBorderPrecedenceRow));
  if (!result.Exists())
    return result;

  // (4) The next row's before border.
  if (next_cell) {
    result = ChooseBorder(
        result,
        CollapsedBorderValue(next_cell->Parent()->Style()->BorderBefore(),
                             include_color ? next_cell->Parent()->ResolveColor(
                                                 before_color_property)
                                           : Color(),
                             kBorderPrecedenceRow));
    if (!result.Exists())
      return result;
  }

  // Now check row groups.
  LayoutTableSection* curr_section = Section();
  if (RowIndex() + RowSpan() >= curr_section->NumRows()) {
    // (5) Our row group's after border.
    result = ChooseBorder(
        result, CollapsedBorderValue(curr_section->Style()->BorderAfter(),
                                     include_color ? curr_section->ResolveColor(
                                                         after_color_property)
                                                   : Color(),
                                     kBorderPrecedenceRowGroup));
    if (!result.Exists())
      return result;

    // (6) Following row group's before border.
    curr_section = table->SectionBelow(curr_section, kSkipEmptySections);
    if (curr_section) {
      result = ChooseBorder(
          result,
          CollapsedBorderValue(
              curr_section->Style()->BorderBefore(),
              include_color ? curr_section->ResolveColor(before_color_property)
                            : Color(),
              kBorderPrecedenceRowGroup));
      if (!result.Exists())
        return result;
    }
  }

  if (!curr_section) {
    // (8) Our column and column group's after borders.
    LayoutTableCol* col_elt =
        table->ColElementAtAbsoluteColumn(AbsoluteColumnIndex())
            .InnermostColOrColGroup();
    if (col_elt) {
      result = ChooseBorder(
          result, CollapsedBorderValue(col_elt->Style()->BorderAfter(),
                                       include_color ? col_elt->ResolveColor(
                                                           after_color_property)
                                                     : Color(),
                                       kBorderPrecedenceColumn));
      if (!result.Exists())
        return result;
      if (LayoutTableCol* enclosing_column_group =
              col_elt->EnclosingColumnGroup()) {
        result = ChooseBorder(
            result, CollapsedBorderValue(
                        enclosing_column_group->Style()->BorderAfter(),
                        include_color ? enclosing_column_group->ResolveColor(
                                            after_color_property)
                                      : Color(),
                        kBorderPrecedenceColumnGroup));
        if (!result.Exists())
          return result;
      }
    }

    // (9) The table's after border.
    result = ChooseBorder(
        result,
        CollapsedBorderValue(
            table->Style()->BorderAfter(),
            include_color ? table->ResolveColor(after_color_property) : Color(),
            kBorderPrecedenceTable));
    if (!result.Exists())
      return result;
  }

  return result;
}

LayoutUnit LayoutTableCell::BorderLeft() const {
  return Table()->CollapseBorders() ? BorderHalfLeft(false)
                                    : LayoutBlockFlow::BorderLeft();
}

LayoutUnit LayoutTableCell::BorderRight() const {
  return Table()->CollapseBorders() ? BorderHalfRight(false)
                                    : LayoutBlockFlow::BorderRight();
}

LayoutUnit LayoutTableCell::BorderTop() const {
  return Table()->CollapseBorders() ? BorderHalfTop(false)
                                    : LayoutBlockFlow::BorderTop();
}

LayoutUnit LayoutTableCell::BorderBottom() const {
  return Table()->CollapseBorders() ? BorderHalfBottom(false)
                                    : LayoutBlockFlow::BorderBottom();
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=46191, make the collapsed
// border drawing work with different block flow values instead of being
// hard-coded to top-to-bottom.
LayoutUnit LayoutTableCell::BorderStart() const {
  return Table()->CollapseBorders() ? BorderHalfStart(false)
                                    : LayoutBlockFlow::BorderStart();
}

LayoutUnit LayoutTableCell::BorderEnd() const {
  return Table()->CollapseBorders() ? BorderHalfEnd(false)
                                    : LayoutBlockFlow::BorderEnd();
}

LayoutUnit LayoutTableCell::BorderBefore() const {
  return Table()->CollapseBorders() ? BorderHalfBefore(false)
                                    : LayoutBlockFlow::BorderBefore();
}

LayoutUnit LayoutTableCell::BorderAfter() const {
  return Table()->CollapseBorders() ? BorderHalfAfter(false)
                                    : LayoutBlockFlow::BorderAfter();
}

LayoutUnit LayoutTableCell::BorderHalfLeft(bool outer) const {
  const ComputedStyle& style_for_cell_flow = this->StyleForCellFlow();
  if (style_for_cell_flow.IsHorizontalWritingMode())
    return style_for_cell_flow.IsLeftToRightDirection() ? BorderHalfStart(outer)
                                                        : BorderHalfEnd(outer);
  return style_for_cell_flow.IsFlippedBlocksWritingMode()
             ? BorderHalfAfter(outer)
             : BorderHalfBefore(outer);
}

LayoutUnit LayoutTableCell::BorderHalfRight(bool outer) const {
  const ComputedStyle& style_for_cell_flow = this->StyleForCellFlow();
  if (style_for_cell_flow.IsHorizontalWritingMode())
    return style_for_cell_flow.IsLeftToRightDirection()
               ? BorderHalfEnd(outer)
               : BorderHalfStart(outer);
  return style_for_cell_flow.IsFlippedBlocksWritingMode()
             ? BorderHalfBefore(outer)
             : BorderHalfAfter(outer);
}

LayoutUnit LayoutTableCell::BorderHalfTop(bool outer) const {
  const ComputedStyle& style_for_cell_flow = this->StyleForCellFlow();
  if (style_for_cell_flow.IsHorizontalWritingMode())
    return style_for_cell_flow.IsFlippedBlocksWritingMode()
               ? BorderHalfAfter(outer)
               : BorderHalfBefore(outer);
  return style_for_cell_flow.IsLeftToRightDirection() ? BorderHalfStart(outer)
                                                      : BorderHalfEnd(outer);
}

LayoutUnit LayoutTableCell::BorderHalfBottom(bool outer) const {
  const ComputedStyle& style_for_cell_flow = this->StyleForCellFlow();
  if (style_for_cell_flow.IsHorizontalWritingMode())
    return style_for_cell_flow.IsFlippedBlocksWritingMode()
               ? BorderHalfBefore(outer)
               : BorderHalfAfter(outer);
  return style_for_cell_flow.IsLeftToRightDirection() ? BorderHalfEnd(outer)
                                                      : BorderHalfStart(outer);
}

LayoutUnit LayoutTableCell::BorderHalfStart(bool outer) const {
  CollapsedBorderValue border =
      ComputeCollapsedStartBorder(kDoNotIncludeBorderColor);
  if (border.Exists()) {
    return LayoutUnit(
        (border.Width() +
         ((StyleForCellFlow().IsLeftToRightDirection() ^ outer) ? 1 : 0)) /
        2);  // Give the extra pixel to top and left.
  }
  return LayoutUnit();
}

LayoutUnit LayoutTableCell::BorderHalfEnd(bool outer) const {
  CollapsedBorderValue border =
      ComputeCollapsedEndBorder(kDoNotIncludeBorderColor);
  if (border.Exists()) {
    return LayoutUnit(
        (border.Width() +
         ((StyleForCellFlow().IsLeftToRightDirection() ^ outer) ? 0 : 1)) /
        2);
  }
  return LayoutUnit();
}

LayoutUnit LayoutTableCell::BorderHalfBefore(bool outer) const {
  CollapsedBorderValue border =
      ComputeCollapsedBeforeBorder(kDoNotIncludeBorderColor);
  if (border.Exists()) {
    return LayoutUnit(
        (border.Width() +
         ((StyleForCellFlow().IsFlippedBlocksWritingMode() ^ outer) ? 0 : 1)) /
        2);  // Give the extra pixel to top and left.
  }
  return LayoutUnit();
}

LayoutUnit LayoutTableCell::BorderHalfAfter(bool outer) const {
  CollapsedBorderValue border =
      ComputeCollapsedAfterBorder(kDoNotIncludeBorderColor);
  if (border.Exists()) {
    return LayoutUnit(
        (border.Width() +
         ((StyleForCellFlow().IsFlippedBlocksWritingMode() ^ outer) ? 1 : 0)) /
        2);
  }
  return LayoutUnit();
}

void LayoutTableCell::Paint(const PaintInfo& paint_info,
                            const LayoutPoint& paint_offset) const {
  TableCellPainter(*this).Paint(paint_info, paint_offset);
}

static void AddBorderStyle(LayoutTable::CollapsedBorderValues& border_values,
                           CollapsedBorderValue border_value) {
  if (!border_value.IsVisible())
    return;
  size_t count = border_values.size();
  for (size_t i = 0; i < count; ++i) {
    if (border_values[i].IsSameIgnoringColor(border_value))
      return;
  }
  border_values.push_back(border_value);
}

void LayoutTableCell::CollectBorderValues(
    LayoutTable::CollapsedBorderValues& border_values) {
  CollapsedBorderValues new_values(
      *this, ComputeCollapsedStartBorder(), ComputeCollapsedEndBorder(),
      ComputeCollapsedBeforeBorder(), ComputeCollapsedAfterBorder());

  bool changed = false;
  if (!new_values.StartBorder().IsVisible() &&
      !new_values.EndBorder().IsVisible() &&
      !new_values.BeforeBorder().IsVisible() &&
      !new_values.AfterBorder().IsVisible()) {
    changed = !!collapsed_border_values_;
    collapsed_border_values_ = nullptr;
  } else if (!collapsed_border_values_) {
    changed = true;
    collapsed_border_values_ = WTF::WrapUnique(new CollapsedBorderValues(
        *this, new_values.StartBorder(), new_values.EndBorder(),
        new_values.BeforeBorder(), new_values.AfterBorder()));
  } else {
    // We check visuallyEquals so that the table cell is invalidated only if a
    // changed collapsed border is visible in the first place.
    changed = !collapsed_border_values_->StartBorder().VisuallyEquals(
                  new_values.StartBorder()) ||
              !collapsed_border_values_->EndBorder().VisuallyEquals(
                  new_values.EndBorder()) ||
              !collapsed_border_values_->BeforeBorder().VisuallyEquals(
                  new_values.BeforeBorder()) ||
              !collapsed_border_values_->AfterBorder().VisuallyEquals(
                  new_values.AfterBorder());
    if (changed)
      collapsed_border_values_->SetCollapsedBorderValues(new_values);
  }

  // If collapsed borders changed, invalidate the cell's display item client on
  // the table's backing.
  // TODO(crbug.com/451090#c5): Need a way to invalidate/repaint the borders
  // only.
  if (changed) {
    ObjectPaintInvalidator(*Table())
        .SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
            *this, kPaintInvalidationStyleChange);
  }

  AddBorderStyle(border_values, new_values.StartBorder());
  AddBorderStyle(border_values, new_values.EndBorder());
  AddBorderStyle(border_values, new_values.BeforeBorder());
  AddBorderStyle(border_values, new_values.AfterBorder());
}

void LayoutTableCell::SortBorderValues(
    LayoutTable::CollapsedBorderValues& border_values) {
  std::sort(border_values.begin(), border_values.end(), CompareBorders);
}

void LayoutTableCell::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) const {
  TableCellPainter(*this).PaintBoxDecorationBackground(paint_info,
                                                       paint_offset);
}

void LayoutTableCell::PaintMask(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) const {
  TableCellPainter(*this).PaintMask(paint_info, paint_offset);
}

void LayoutTableCell::ScrollbarsChanged(bool horizontal_scrollbar_changed,
                                        bool vertical_scrollbar_changed,
                                        ScrollbarChangeContext context) {
  LayoutBlock::ScrollbarsChanged(horizontal_scrollbar_changed,
                                 vertical_scrollbar_changed);
  if (context != kLayout)
    return;

  int scrollbar_height = ScrollbarLogicalHeight();
  // Not sure if we should be doing something when a scrollbar goes away or not.
  if (!scrollbar_height)
    return;

  // We only care if the scrollbar that affects our intrinsic padding has been
  // added.
  if ((IsHorizontalWritingMode() && !horizontal_scrollbar_changed) ||
      (!IsHorizontalWritingMode() && !vertical_scrollbar_changed))
    return;

  // Shrink our intrinsic padding as much as possible to accommodate the
  // scrollbar.
  if (Style()->VerticalAlign() == EVerticalAlign::kMiddle) {
    LayoutUnit total_height = LogicalHeight();
    LayoutUnit height_without_intrinsic_padding =
        total_height - IntrinsicPaddingBefore() - IntrinsicPaddingAfter();
    total_height -= scrollbar_height;
    LayoutUnit new_before_padding =
        (total_height - height_without_intrinsic_padding) / 2;
    LayoutUnit new_after_padding =
        total_height - height_without_intrinsic_padding - new_before_padding;
    SetIntrinsicPaddingBefore(new_before_padding.ToInt());
    SetIntrinsicPaddingAfter(new_after_padding.ToInt());
  } else {
    SetIntrinsicPaddingAfter(IntrinsicPaddingAfter() - scrollbar_height);
  }
}

LayoutTableCell* LayoutTableCell::CreateAnonymous(Document* document) {
  LayoutTableCell* layout_object = new LayoutTableCell(nullptr);
  layout_object->SetDocumentForAnonymous(document);
  return layout_object;
}

LayoutTableCell* LayoutTableCell::CreateAnonymousWithParent(
    const LayoutObject* parent) {
  LayoutTableCell* new_cell =
      LayoutTableCell::CreateAnonymous(&parent->GetDocument());
  RefPtr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(parent->StyleRef(),
                                                     EDisplay::kTableCell);
  new_cell->SetStyle(std::move(new_style));
  return new_cell;
}

bool LayoutTableCell::BackgroundIsKnownToBeOpaqueInRect(
    const LayoutRect& local_rect) const {
  // If this object has layer, the area of collapsed borders should be
  // transparent to expose the collapsed borders painted on the underlying
  // layer.
  if (HasLayer() && Table()->CollapseBorders())
    return false;
  return LayoutBlockFlow::BackgroundIsKnownToBeOpaqueInRect(local_rect);
}

bool LayoutTableCell::UsesCompositedCellDisplayItemClients() const {
  // In certain cases such as collapsed borders for composited table cells we
  // paint content for the cell into the table graphics layer backing and so
  // must use the table's visual rect.
  return (HasLayer() && Layer()->GetCompositingState() != kNotComposited) ||
         RuntimeEnabledFeatures::slimmingPaintV2Enabled();
}

void LayoutTableCell::InvalidateDisplayItemClients(
    PaintInvalidationReason reason) const {
  LayoutBlockFlow::InvalidateDisplayItemClients(reason);

  if (!UsesCompositedCellDisplayItemClients())
    return;

  ObjectPaintInvalidator invalidator(*this);
  if (collapsed_border_values_)
    invalidator.InvalidateDisplayItemClient(*collapsed_border_values_, reason);
}

// TODO(lunalu): Deliberately dump the "inner" box of table cells, since that
// is what current results reflect.  We'd like to clean up the results to dump
// both the outer box and the intrinsic padding so that both bits of information
// are captured by the results.
LayoutRect LayoutTableCell::DebugRect() const {
  LayoutRect rect = LayoutRect(
      Location().X(), Location().Y() + IntrinsicPaddingBefore(), Size().Width(),
      Size().Height() - IntrinsicPaddingBefore() - IntrinsicPaddingAfter());

  LayoutBlock* cb = ContainingBlock();
  if (cb)
    cb->AdjustChildDebugRect(rect);

  return rect;
}

void LayoutTableCell::AdjustChildDebugRect(LayoutRect& r) const {
  r.Move(0, -IntrinsicPaddingBefore());
}

bool LayoutTableCell::HasLineIfEmpty() const {
  if (GetNode() && HasEditableStyle(*GetNode()))
    return true;

  return LayoutBlock::HasLineIfEmpty();
}

}  // namespace blink
