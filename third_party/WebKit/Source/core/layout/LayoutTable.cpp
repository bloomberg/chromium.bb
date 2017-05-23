/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc.
 *               All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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

#include "core/layout/LayoutTable.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLTableElement.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutTableCaption.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableSection.h"
#include "core/layout/LayoutView.h"
#include "core/layout/SubtreeLayoutScope.h"
#include "core/layout/TableLayoutAlgorithmAuto.h"
#include "core/layout/TableLayoutAlgorithmFixed.h"
#include "core/layout/TextAutosizer.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/TablePaintInvalidator.h"
#include "core/paint/TablePainter.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

using namespace HTMLNames;

LayoutTable::LayoutTable(Element* element)
    : LayoutBlock(element),
      head_(nullptr),
      foot_(nullptr),
      first_body_(nullptr),
      collapsed_borders_valid_(false),
      needs_invalidate_collapsed_borders_for_all_cells_(false),
      has_col_elements_(false),
      needs_section_recalc_(false),
      column_logical_width_changed_(false),
      column_layout_objects_valid_(false),
      no_cell_colspan_at_least_(0),
      h_spacing_(0),
      v_spacing_(0),
      border_start_(0),
      border_end_(0) {
  DCHECK(!ChildrenInline());
  effective_column_positions_.Fill(0, 1);
}

LayoutTable::~LayoutTable() {}

void LayoutTable::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  LayoutBlock::StyleDidChange(diff, old_style);

  bool old_fixed_table_layout =
      old_style ? old_style->IsFixedTableLayout() : false;

  // In the collapsed border model, there is no cell spacing.
  h_spacing_ = ShouldCollapseBorders() ? 0 : Style()->HorizontalBorderSpacing();
  v_spacing_ = ShouldCollapseBorders() ? 0 : Style()->VerticalBorderSpacing();

  if (!table_layout_ ||
      Style()->IsFixedTableLayout() != old_fixed_table_layout) {
    if (table_layout_)
      table_layout_->WillChangeTableLayout();

    // According to the CSS2 spec, you only use fixed table layout if an
    // explicit width is specified on the table. Auto width implies auto table
    // layout.
    if (Style()->IsFixedTableLayout())
      table_layout_ = WTF::MakeUnique<TableLayoutAlgorithmFixed>(this);
    else
      table_layout_ = WTF::MakeUnique<TableLayoutAlgorithmAuto>(this);
  }

  if (!old_style)
    return;

  if (old_style->BorderCollapse() != StyleRef().BorderCollapse()) {
    InvalidateCollapsedBorders();
  } else {
    LayoutTableBoxComponent::InvalidateCollapsedBordersOnStyleChange(
        *this, *this, diff, *old_style);
  }

  if (LayoutTableBoxComponent::DoCellsHaveDirtyWidth(*this, *this, diff,
                                                     *old_style))
    MarkAllCellsWidthsDirtyAndOrNeedsLayout(kMarkDirtyAndNeedsLayout);
}

static inline void ResetSectionPointerIfNotBefore(LayoutTableSection*& ptr,
                                                  LayoutObject* before) {
  if (!before || !ptr)
    return;
  LayoutObject* o = before->PreviousSibling();
  while (o && o != ptr)
    o = o->PreviousSibling();
  if (!o)
    ptr = 0;
}

static inline bool NeedsTableSection(LayoutObject* object) {
  // Return true if 'object' can't exist in an anonymous table without being
  // wrapped in a table section box.
  EDisplay display = object->Style()->Display();
  return display != EDisplay::kTableCaption &&
         display != EDisplay::kTableColumnGroup &&
         display != EDisplay::kTableColumn;
}

void LayoutTable::AddChild(LayoutObject* child, LayoutObject* before_child) {
  bool wrap_in_anonymous_section = !child->IsOutOfFlowPositioned();

  if (child->IsTableCaption()) {
    wrap_in_anonymous_section = false;
  } else if (child->IsLayoutTableCol()) {
    has_col_elements_ = true;
    wrap_in_anonymous_section = false;
  } else if (child->IsTableSection()) {
    switch (child->Style()->Display()) {
      case EDisplay::kTableHeaderGroup:
        ResetSectionPointerIfNotBefore(head_, before_child);
        if (!head_) {
          head_ = ToLayoutTableSection(child);
        } else {
          ResetSectionPointerIfNotBefore(first_body_, before_child);
          if (!first_body_)
            first_body_ = ToLayoutTableSection(child);
        }
        wrap_in_anonymous_section = false;
        break;
      case EDisplay::kTableFooterGroup:
        ResetSectionPointerIfNotBefore(foot_, before_child);
        if (!foot_) {
          foot_ = ToLayoutTableSection(child);
          wrap_in_anonymous_section = false;
          break;
        }
      // Fall through.
      case EDisplay::kTableRowGroup:
        ResetSectionPointerIfNotBefore(first_body_, before_child);
        if (!first_body_)
          first_body_ = ToLayoutTableSection(child);
        wrap_in_anonymous_section = false;
        break;
      default:
        NOTREACHED();
    }
  } else {
    wrap_in_anonymous_section = true;
  }

  if (child->IsTableSection())
    SetNeedsSectionRecalc();

  if (!wrap_in_anonymous_section) {
    if (before_child && before_child->Parent() != this)
      before_child = SplitAnonymousBoxesAroundChild(before_child);

    LayoutBox::AddChild(child, before_child);
    return;
  }

  if (!before_child && LastChild() && LastChild()->IsTableSection() &&
      LastChild()->IsAnonymous() && !LastChild()->IsBeforeContent()) {
    LastChild()->AddChild(child);
    return;
  }

  if (before_child && !before_child->IsAnonymous() &&
      before_child->Parent() == this) {
    LayoutObject* section = before_child->PreviousSibling();
    if (section && section->IsTableSection() && section->IsAnonymous()) {
      section->AddChild(child);
      return;
    }
  }

  LayoutObject* last_box = before_child;
  while (last_box && last_box->Parent()->IsAnonymous() &&
         !last_box->IsTableSection() && NeedsTableSection(last_box))
    last_box = last_box->Parent();
  if (last_box && last_box->IsAnonymous() && !IsAfterContent(last_box)) {
    if (before_child == last_box)
      before_child = last_box->SlowFirstChild();
    last_box->AddChild(child, before_child);
    return;
  }

  if (before_child && !before_child->IsTableSection() &&
      NeedsTableSection(before_child))
    before_child = 0;

  LayoutTableSection* section =
      LayoutTableSection::CreateAnonymousWithParent(this);
  AddChild(section, before_child);
  section->AddChild(child);
}

void LayoutTable::AddCaption(const LayoutTableCaption* caption) {
  DCHECK_EQ(captions_.Find(caption), kNotFound);
  captions_.push_back(const_cast<LayoutTableCaption*>(caption));
}

void LayoutTable::RemoveCaption(const LayoutTableCaption* old_caption) {
  size_t index = captions_.Find(old_caption);
  DCHECK_NE(index, kNotFound);
  if (index == kNotFound)
    return;

  captions_.erase(index);
}

void LayoutTable::InvalidateCachedColumns() {
  column_layout_objects_valid_ = false;
  column_layout_objects_.resize(0);
}

void LayoutTable::AddColumn(const LayoutTableCol*) {
  InvalidateCachedColumns();
}

void LayoutTable::RemoveColumn(const LayoutTableCol*) {
  InvalidateCachedColumns();
  // We don't really need to recompute our sections, but we need to update our
  // column count and whether we have a column. Currently, we only have one
  // size-fit-all flag but we may have to consider splitting it.
  SetNeedsSectionRecalc();
}

bool LayoutTable::IsLogicalWidthAuto() const {
  Length style_logical_width = Style()->LogicalWidth();
  return (!style_logical_width.IsSpecified() ||
          !style_logical_width.IsPositive()) &&
         !style_logical_width.IsIntrinsic();
}

void LayoutTable::UpdateLogicalWidth() {
  RecalcSectionsIfNeeded();

  if (IsOutOfFlowPositioned()) {
    LogicalExtentComputedValues computed_values;
    ComputePositionedLogicalWidth(computed_values);
    SetLogicalWidth(computed_values.extent_);
    SetLogicalLeft(computed_values.position_);
    SetMarginStart(computed_values.margins_.start_);
    SetMarginEnd(computed_values.margins_.end_);
  }

  LayoutBlock* cb = ContainingBlock();

  LayoutUnit available_logical_width = ContainingBlockLogicalWidthForContent();
  bool has_perpendicular_containing_block =
      cb->Style()->IsHorizontalWritingMode() !=
      Style()->IsHorizontalWritingMode();
  LayoutUnit container_width_in_inline_direction =
      has_perpendicular_containing_block
          ? PerpendicularContainingBlockLogicalHeight()
          : available_logical_width;

  Length style_logical_width = Style()->LogicalWidth();
  if (!IsLogicalWidthAuto()) {
    SetLogicalWidth(ConvertStyleLogicalWidthToComputedWidth(
        style_logical_width, container_width_in_inline_direction));
  } else {
    // Subtract out any fixed margins from our available width for auto width
    // tables.
    LayoutUnit margin_start =
        MinimumValueForLength(Style()->MarginStart(), available_logical_width);
    LayoutUnit margin_end =
        MinimumValueForLength(Style()->MarginEnd(), available_logical_width);
    LayoutUnit margin_total = margin_start + margin_end;

    // Subtract out our margins to get the available content width.
    LayoutUnit available_content_logical_width =
        (container_width_in_inline_direction - margin_total)
            .ClampNegativeToZero();
    if (ShrinkToAvoidFloats() && cb->IsLayoutBlockFlow() &&
        ToLayoutBlockFlow(cb)->ContainsFloats() &&
        !has_perpendicular_containing_block)
      available_content_logical_width = ShrinkLogicalWidthToAvoidFloats(
          margin_start, margin_end, ToLayoutBlockFlow(cb));

    // Ensure we aren't bigger than our available width.
    LayoutUnit max_width = MaxPreferredLogicalWidth();
    // scaledWidthFromPercentColumns depends on m_layoutStruct in
    // TableLayoutAlgorithmAuto, which maxPreferredLogicalWidth fills in. So
    // scaledWidthFromPercentColumns has to be called after
    // maxPreferredLogicalWidth.
    LayoutUnit scaled_width = table_layout_->ScaledWidthFromPercentColumns() +
                              BordersPaddingAndSpacingInRowDirection();
    max_width = std::max(scaled_width, max_width);
    SetLogicalWidth(LayoutUnit(
        std::min(available_content_logical_width, max_width).Floor()));
  }

  // Ensure we aren't bigger than our max-width style.
  Length style_max_logical_width = Style()->LogicalMaxWidth();
  if ((style_max_logical_width.IsSpecified() &&
       !style_max_logical_width.IsNegative()) ||
      style_max_logical_width.IsIntrinsic()) {
    LayoutUnit computed_max_logical_width =
        ConvertStyleLogicalWidthToComputedWidth(style_max_logical_width,
                                                available_logical_width);
    SetLogicalWidth(LayoutUnit(
        std::min(LogicalWidth(), computed_max_logical_width).Floor()));
  }

  // Ensure we aren't smaller than our min preferred width. This MUST be done
  // after 'max-width' as we ignore it if it means we wouldn't accommodate our
  // content.
  SetLogicalWidth(
      LayoutUnit(std::max(LogicalWidth(), MinPreferredLogicalWidth()).Floor()));

  // Ensure we aren't smaller than our min-width style.
  Length style_min_logical_width = Style()->LogicalMinWidth();
  if ((style_min_logical_width.IsSpecified() &&
       !style_min_logical_width.IsNegative()) ||
      style_min_logical_width.IsIntrinsic()) {
    LayoutUnit computed_min_logical_width =
        ConvertStyleLogicalWidthToComputedWidth(style_min_logical_width,
                                                available_logical_width);
    SetLogicalWidth(LayoutUnit(
        std::max(LogicalWidth(), computed_min_logical_width).Floor()));
  }

  // Finally, with our true width determined, compute our margins for real.
  ComputedMarginValues margin_values;
  ComputeMarginsForDirection(kInlineDirection, cb, available_logical_width,
                             LogicalWidth(), margin_values.start_,
                             margin_values.end_, Style()->MarginStart(),
                             Style()->MarginEnd());
  SetMarginStart(margin_values.start_);
  SetMarginEnd(margin_values.end_);

  // We should NEVER shrink the table below the min-content logical width, or
  // else the table can't accommodate its own content which doesn't match CSS
  // nor what authors expect.
  // FIXME: When we convert to sub-pixel layout for tables we can remove the int
  // conversion. http://crbug.com/241198
  DCHECK_GE(LogicalWidth().Floor(), MinPreferredLogicalWidth().Floor());
}

// This method takes a ComputedStyle's logical width, min-width, or max-width
// length and computes its actual value.
LayoutUnit LayoutTable::ConvertStyleLogicalWidthToComputedWidth(
    const Length& style_logical_width,
    LayoutUnit available_width) const {
  if (style_logical_width.IsIntrinsic())
    return ComputeIntrinsicLogicalWidthUsing(
        style_logical_width, available_width,
        BordersPaddingAndSpacingInRowDirection());

  // HTML tables' width styles already include borders and paddings, but CSS
  // tables' width styles do not.
  LayoutUnit borders;
  bool is_css_table = !isHTMLTableElement(GetNode());
  if (is_css_table && style_logical_width.IsSpecified() &&
      style_logical_width.IsPositive() &&
      Style()->BoxSizing() == EBoxSizing::kContentBox) {
    borders = BorderStart() + BorderEnd() +
              (ShouldCollapseBorders() ? LayoutUnit()
                                       : PaddingStart() + PaddingEnd());
  }

  return MinimumValueForLength(style_logical_width, available_width) + borders;
}

LayoutUnit LayoutTable::ConvertStyleLogicalHeightToComputedHeight(
    const Length& style_logical_height) const {
  LayoutUnit border_and_padding_before =
      BorderBefore() +
      (ShouldCollapseBorders() ? LayoutUnit() : PaddingBefore());
  LayoutUnit border_and_padding_after =
      BorderAfter() + (ShouldCollapseBorders() ? LayoutUnit() : PaddingAfter());
  LayoutUnit border_and_padding =
      border_and_padding_before + border_and_padding_after;
  LayoutUnit computed_logical_height;
  if (style_logical_height.IsFixed()) {
    // HTML tables size as though CSS height includes border/padding, CSS tables
    // do not.
    LayoutUnit borders = LayoutUnit();
    // FIXME: We cannot apply box-sizing: content-box on <table> which other
    // browsers allow.
    if (isHTMLTableElement(GetNode()) ||
        Style()->BoxSizing() == EBoxSizing::kBorderBox) {
      borders = border_and_padding;
    }
    computed_logical_height =
        LayoutUnit(style_logical_height.Value() - borders);
  } else if (style_logical_height.IsPercentOrCalc()) {
    computed_logical_height =
        ComputePercentageLogicalHeight(style_logical_height);
  } else if (style_logical_height.IsIntrinsic()) {
    computed_logical_height = ComputeIntrinsicLogicalContentHeightUsing(
        style_logical_height, LogicalHeight() - border_and_padding,
        border_and_padding);
  } else {
    NOTREACHED();
  }
  return computed_logical_height.ClampNegativeToZero();
}

void LayoutTable::LayoutCaption(LayoutTableCaption& caption,
                                SubtreeLayoutScope& layouter) {
  if (!caption.NeedsLayout())
    MarkChildForPaginationRelayoutIfNeeded(caption, layouter);
  if (caption.NeedsLayout()) {
    // The margins may not be available but ensure the caption is at least
    // located beneath any previous sibling caption so that it does not
    // mistakenly think any floats in the previous caption intrude into it.
    caption.SetLogicalLocation(
        LayoutPoint(caption.MarginStart(),
                    CollapsedMarginBeforeForChild(caption) + LogicalHeight()));
    // If LayoutTableCaption ever gets a layout() function, use it here.
    caption.LayoutIfNeeded();
  }
  // Apply the margins to the location now that they are definitely available
  // from layout
  LayoutUnit caption_logical_top =
      CollapsedMarginBeforeForChild(caption) + LogicalHeight();
  caption.SetLogicalLocation(
      LayoutPoint(caption.MarginStart(), caption_logical_top));
  if (View()->GetLayoutState()->IsPaginated())
    UpdateFragmentationInfoForChild(caption);

  if (!SelfNeedsLayout())
    caption.SetMayNeedPaintInvalidation();

  SetLogicalHeight(LogicalHeight() + caption.LogicalHeight() +
                   CollapsedMarginBeforeForChild(caption) +
                   CollapsedMarginAfterForChild(caption));
}

void LayoutTable::LayoutSection(
    LayoutTableSection& section,
    SubtreeLayoutScope& layouter,
    LayoutUnit logical_left,
    TableHeightChangingValue table_height_changing) {
  section.SetLogicalLocation(LayoutPoint(logical_left, LogicalHeight()));
  if (column_logical_width_changed_)
    layouter.SetChildNeedsLayout(&section);
  if (!section.NeedsLayout())
    MarkChildForPaginationRelayoutIfNeeded(section, layouter);
  bool needed_layout = section.NeedsLayout();
  if (needed_layout)
    section.UpdateLayout();
  if (needed_layout || table_height_changing == kTableHeightChanging)
    section.SetLogicalHeight(LayoutUnit(section.CalcRowLogicalHeight()));

  if (View()->GetLayoutState()->IsPaginated())
    UpdateFragmentationInfoForChild(section);
  SetLogicalHeight(LogicalHeight() + section.LogicalHeight());
}

LayoutUnit LayoutTable::LogicalHeightFromStyle() const {
  LayoutUnit computed_logical_height;
  Length logical_height_length = Style()->LogicalHeight();
  if (logical_height_length.IsIntrinsic() ||
      (logical_height_length.IsSpecified() &&
       logical_height_length.IsPositive())) {
    computed_logical_height =
        ConvertStyleLogicalHeightToComputedHeight(logical_height_length);
  }

  Length logical_max_height_length = Style()->LogicalMaxHeight();
  if (logical_max_height_length.IsIntrinsic() ||
      (logical_max_height_length.IsSpecified() &&
       !logical_max_height_length.IsNegative())) {
    LayoutUnit computed_max_logical_height =
        ConvertStyleLogicalHeightToComputedHeight(logical_max_height_length);
    computed_logical_height =
        std::min(computed_logical_height, computed_max_logical_height);
  }

  Length logical_min_height_length = Style()->LogicalMinHeight();
  if (logical_min_height_length.IsIntrinsic() ||
      (logical_min_height_length.IsSpecified() &&
       !logical_min_height_length.IsNegative())) {
    LayoutUnit computed_min_logical_height =
        ConvertStyleLogicalHeightToComputedHeight(logical_min_height_length);
    computed_logical_height =
        std::max(computed_logical_height, computed_min_logical_height);
  }

  return computed_logical_height;
}

void LayoutTable::DistributeExtraLogicalHeight(int extra_logical_height) {
  if (extra_logical_height <= 0)
    return;

  // FIXME: Distribute the extra logical height between all table sections
  // instead of giving it all to the first one.
  if (LayoutTableSection* section = FirstBody())
    extra_logical_height -=
        section->DistributeExtraLogicalHeightToRows(extra_logical_height);

  // crbug.com/690087: We really would like to enable this ASSERT to ensure that
  // all the extra space has been distributed.
  // However our current distribution algorithm does not round properly and thus
  // we can have some remaining height.
  // DCHECK(!topSection() || !extraLogicalHeight);
}

void LayoutTable::SimplifiedNormalFlowLayout() {
  // FIXME: We should walk through the items in the tree in tree order to do the
  // layout here instead of walking through individual parts of the tree.
  // crbug.com/442737
  for (auto& caption : captions_)
    caption->LayoutIfNeeded();

  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section)) {
    section->LayoutIfNeeded();
    section->LayoutRows();
    section->ComputeOverflowFromDescendants();
    section->UpdateAfterLayout();
    section->AddVisualEffectOverflow();
  }
}

bool LayoutTable::RecalcChildOverflowAfterStyleChange() {
  DCHECK(ChildNeedsOverflowRecalcAfterStyleChange());
  ClearChildNeedsOverflowRecalcAfterStyleChange();

  // If the table sections we keep pointers to have gone away then the table
  // will be rebuilt and overflow will get recalculated anyway so return early.
  if (NeedsSectionRecalc())
    return false;

  bool children_overflow_changed = false;
  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section)) {
    if (!section->ChildNeedsOverflowRecalcAfterStyleChange())
      continue;
    children_overflow_changed =
        section->RecalcChildOverflowAfterStyleChange() ||
        children_overflow_changed;
  }
  return RecalcPositionedDescendantsOverflowAfterStyleChange() ||
         children_overflow_changed;
}

void LayoutTable::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  if (SimplifiedLayout())
    return;

  // Note: LayoutTable is handled differently than other LayoutBlocks and the
  // LayoutScope
  //       must be created before the table begins laying out.
  TextAutosizer::LayoutScope text_autosizer_layout_scope(this);

  RecalcSectionsIfNeeded();
  // FIXME: We should do this recalc lazily in borderStart/borderEnd so that we
  // don't have to make sure to call this before we call borderStart/borderEnd
  // to avoid getting a stale value.
  RecalcBordersInRowDirection();

  SubtreeLayoutScope layouter(*this);

  {
    LayoutState state(*this);
    LayoutUnit old_logical_width = LogicalWidth();
    LayoutUnit old_logical_height = LogicalHeight();

    SetLogicalHeight(LayoutUnit());
    UpdateLogicalWidth();

    if (LogicalWidth() != old_logical_width) {
      for (unsigned i = 0; i < captions_.size(); i++)
        layouter.SetNeedsLayout(captions_[i],
                                LayoutInvalidationReason::kTableChanged);
    }
    // FIXME: The optimisation below doesn't work since the internal table
    // layout could have changed. We need to add a flag to the table
    // layout that tells us if something has changed in the min max
    // calculations to do it correctly.
    // if ( oldWidth != width() || columns.size() + 1 != columnPos.size() )
    table_layout_->UpdateLayout();

    // Lay out top captions.
    // FIXME: Collapse caption margin.
    for (unsigned i = 0; i < captions_.size(); i++) {
      if (captions_[i]->Style()->CaptionSide() == ECaptionSide::kBottom)
        continue;
      LayoutCaption(*captions_[i], layouter);
    }

    LayoutTableSection* top_section = this->TopSection();
    LayoutTableSection* bottom_section = this->BottomSection();

    // This is the border-before edge of the "table box", relative to the "table
    // wrapper box", i.e. right after all top captions.
    // https://www.w3.org/TR/2011/REC-CSS2-20110607/tables.html#model
    LayoutUnit table_box_logical_top = LogicalHeight();

    bool collapsing = ShouldCollapseBorders();
    if (collapsing) {
      // Need to set up the table borders before we can position the sections.
      for (LayoutTableSection* section = top_section; section;
           section = SectionBelow(section))
        section->RecalcOuterBorder();
    }

    LayoutUnit border_and_padding_before =
        BorderBefore() + (collapsing ? LayoutUnit() : PaddingBefore());
    LayoutUnit border_and_padding_after =
        BorderAfter() + (collapsing ? LayoutUnit() : PaddingAfter());

    SetLogicalHeight(table_box_logical_top + border_and_padding_before);

    LayoutUnit section_logical_left = LayoutUnit(
        Style()->IsLeftToRightDirection() ? BorderStart() : BorderEnd());
    if (!collapsing) {
      section_logical_left +=
          Style()->IsLeftToRightDirection() ? PaddingStart() : PaddingEnd();
    }
    LayoutUnit current_available_logical_height =
        AvailableLogicalHeight(kIncludeMarginBorderPadding);
    TableHeightChangingValue table_height_changing =
        old_available_logical_height_ && old_available_logical_height_ !=
                                             current_available_logical_height
            ? kTableHeightChanging
            : kTableHeightNotChanging;
    old_available_logical_height_ = current_available_logical_height;

    // Lay out table header group.
    if (LayoutTableSection* section = Header()) {
      LayoutSection(*section, layouter, section_logical_left,
                    table_height_changing);
      if (state.IsPaginated()) {
        // If the repeating header group allows at least one row of content,
        // then store the offset for other sections to offset their rows
        // against.
        LayoutUnit section_logical_height = section->LogicalHeight();
        if (section_logical_height <
                section->PageLogicalHeightForOffset(section->LogicalTop()) &&
            section->GetPaginationBreakability() != kAllowAnyBreaks) {
          // Don't include any strut in the header group - we only want the
          // height from its content.
          LayoutUnit offset_for_table_headers = section_logical_height;
          if (LayoutTableRow* row = section->FirstRow())
            offset_for_table_headers -= row->PaginationStrut();
          SetRowOffsetFromRepeatingHeader(offset_for_table_headers);
        }
      }
    }

    // Lay out table body groups, and column groups.
    for (LayoutObject* child = FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsTableSection()) {
        if (child != Header() && child != Footer()) {
          LayoutTableSection& section = *ToLayoutTableSection(child);
          LayoutSection(section, layouter, section_logical_left,
                        table_height_changing);
        }
      } else if (child->IsLayoutTableCol()) {
        child->LayoutIfNeeded();
      } else {
        DCHECK(child->IsTableCaption());
      }
    }

    // Lay out table footer.
    if (LayoutTableSection* section = Footer()) {
      LayoutSection(*section, layouter, section_logical_left,
                    table_height_changing);
    }

    SetLogicalHeight(table_box_logical_top + border_and_padding_before);

    LayoutUnit computed_logical_height = LogicalHeightFromStyle();
    LayoutUnit total_section_logical_height;
    if (top_section) {
      total_section_logical_height =
          bottom_section->LogicalBottom() - top_section->LogicalTop();
    }

    if (!state.IsPaginated() ||
        !CrossesPageBoundary(table_box_logical_top, computed_logical_height)) {
      DistributeExtraLogicalHeight(
          FloorToInt(computed_logical_height - total_section_logical_height));
    }

    LayoutUnit logical_offset =
        top_section ? top_section->LogicalTop() : LayoutUnit();
    for (LayoutTableSection* section = top_section; section;
         section = SectionBelow(section)) {
      section->SetLogicalTop(logical_offset);
      section->LayoutRows();
      logical_offset += section->LogicalHeight();
    }

    if (!top_section &&
        computed_logical_height > total_section_logical_height &&
        !GetDocument().InQuirksMode()) {
      // Completely empty tables (with no sections or anything) should at least
      // honor specified height in strict mode.
      SetLogicalHeight(LogicalHeight() + computed_logical_height);
    }

    // position the table sections
    LayoutTableSection* section = top_section;
    while (section) {
      section->SetLogicalLocation(
          LayoutPoint(section_logical_left, LogicalHeight()));

      SetLogicalHeight(LogicalHeight() + section->LogicalHeight());

      section->UpdateAfterLayout();
      section->AddVisualEffectOverflow();

      section = SectionBelow(section);
    }

    SetLogicalHeight(LogicalHeight() + border_and_padding_after);

    // Lay out bottom captions.
    for (unsigned i = 0; i < captions_.size(); i++) {
      if (captions_[i]->Style()->CaptionSide() != ECaptionSide::kBottom)
        continue;
      LayoutCaption(*captions_[i], layouter);
    }

    UpdateLogicalHeight();

    // table can be containing block of positioned elements.
    bool dimension_changed = old_logical_width != LogicalWidth() ||
                             old_logical_height != LogicalHeight();
    LayoutPositionedObjects(dimension_changed);

    ComputeOverflow(ClientLogicalBottom());
    UpdateAfterLayout();

    if (state.IsPaginated() && IsPageLogicalHeightKnown()) {
      block_offset_to_first_repeatable_header_ = state.PageLogicalOffset(
          *this, top_section ? top_section->LogicalTop() : LayoutUnit());
    }
  }

  // FIXME: This value isn't the intrinsic content logical height, but we need
  // to update the value as its used by flexbox layout. crbug.com/367324
  SetIntrinsicContentLogicalHeight(ContentLogicalHeight());

  column_logical_width_changed_ = false;
  ClearNeedsLayout();
}

void LayoutTable::InvalidateCollapsedBorders() {
  collapsed_borders_.clear();
  collapsed_borders_valid_ = false;
  needs_invalidate_collapsed_borders_for_all_cells_ = true;
  SetMayNeedPaintInvalidation();
}

void LayoutTable::InvalidateCollapsedBordersForAllCellsIfNeeded() {
  DCHECK(ShouldCollapseBorders());

  if (!needs_invalidate_collapsed_borders_for_all_cells_)
    return;
  needs_invalidate_collapsed_borders_for_all_cells_ = false;

  for (LayoutObject* section = FirstChild(); section;
       section = section->NextSibling()) {
    if (!section->IsTableSection())
      continue;
    for (LayoutTableRow* row = ToLayoutTableSection(section)->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        DCHECK_EQ(cell->Table(), this);
        cell->InvalidateCollapsedBorderValues();
      }
    }
  }
}

// Collect all the unique border values that we want to paint in a sorted list.
// During the collection, each cell saves its recalculated borders into the
// cache of its containing section, and invalidates itself if any border
// changes. This method doesn't affect layout.
void LayoutTable::RecalcCollapsedBordersIfNeeded() {
  if (collapsed_borders_valid_ || !ShouldCollapseBorders())
    return;
  collapsed_borders_valid_ = true;
  collapsed_borders_.clear();
  for (LayoutObject* section = FirstChild(); section;
       section = section->NextSibling()) {
    if (!section->IsTableSection())
      continue;
    for (LayoutTableRow* row = ToLayoutTableSection(section)->FirstRow(); row;
         row = row->NextRow()) {
      for (LayoutTableCell* cell = row->FirstCell(); cell;
           cell = cell->NextCell()) {
        DCHECK_EQ(cell->Table(), this);
        cell->CollectCollapsedBorderValues(collapsed_borders_);
      }
    }
  }
  LayoutTableCell::SortCollapsedBorderValues(collapsed_borders_);
}

void LayoutTable::AddOverflowFromChildren() {
  // Add overflow from borders.
  // Technically it's odd that we are incorporating the borders into layout
  // overflow, which is only supposed to be about overflow from our
  // descendant objects, but since tables don't support overflow:auto, this
  // works out fine.
  if (ShouldCollapseBorders()) {
    LayoutUnit right_border_overflow =
        Size().Width() + OuterBorderRight() - BorderRight();
    LayoutUnit left_border_overflow = BorderLeft() - OuterBorderLeft();
    LayoutUnit bottom_border_overflow =
        Size().Height() + OuterBorderBottom() - BorderBottom();
    LayoutUnit top_border_overflow = BorderTop() - OuterBorderTop();
    IntRect border_overflow_rect(
        left_border_overflow.ToInt(), top_border_overflow.ToInt(),
        (right_border_overflow - left_border_overflow).ToInt(),
        (bottom_border_overflow - top_border_overflow).ToInt());
    if (border_overflow_rect != PixelSnappedBorderBoxRect()) {
      LayoutRect border_layout_rect(border_overflow_rect);
      AddLayoutOverflow(border_layout_rect);
      AddContentsVisualOverflow(border_layout_rect);
    }
  }

  // Add overflow from our caption.
  for (unsigned i = 0; i < captions_.size(); i++)
    AddOverflowFromChild(*captions_[i]);

  // Add overflow from our sections.
  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section))
    AddOverflowFromChild(*section);
}

void LayoutTable::PaintObject(const PaintInfo& paint_info,
                              const LayoutPoint& paint_offset) const {
  TablePainter(*this).PaintObject(paint_info, paint_offset);
}

void LayoutTable::SubtractCaptionRect(LayoutRect& rect) const {
  for (unsigned i = 0; i < captions_.size(); i++) {
    LayoutUnit caption_logical_height = captions_[i]->LogicalHeight() +
                                        captions_[i]->MarginBefore() +
                                        captions_[i]->MarginAfter();
    bool caption_is_before =
        (captions_[i]->Style()->CaptionSide() != ECaptionSide::kBottom) ^
        Style()->IsFlippedBlocksWritingMode();
    if (Style()->IsHorizontalWritingMode()) {
      rect.SetHeight(rect.Height() - caption_logical_height);
      if (caption_is_before)
        rect.Move(LayoutUnit(), caption_logical_height);
    } else {
      rect.SetWidth(rect.Width() - caption_logical_height);
      if (caption_is_before)
        rect.Move(caption_logical_height, LayoutUnit());
    }
  }
}

void LayoutTable::MarkAllCellsWidthsDirtyAndOrNeedsLayout(
    WhatToMarkAllCells what_to_mark) {
  for (LayoutObject* child = Children()->FirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsTableSection())
      continue;
    LayoutTableSection* section = ToLayoutTableSection(child);
    section->MarkAllCellsWidthsDirtyAndOrNeedsLayout(what_to_mark);
  }
}

void LayoutTable::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) const {
  TablePainter(*this).PaintBoxDecorationBackground(paint_info, paint_offset);
}

void LayoutTable::PaintMask(const PaintInfo& paint_info,
                            const LayoutPoint& paint_offset) const {
  TablePainter(*this).PaintMask(paint_info, paint_offset);
}

void LayoutTable::ComputeIntrinsicLogicalWidths(LayoutUnit& min_width,
                                                LayoutUnit& max_width) const {
  RecalcSectionsIfNeeded();
  // FIXME: Do the recalc in borderStart/borderEnd and make those const_cast
  // this call.
  // Then m_borderStart/m_borderEnd will be transparent a cache and it removes
  // the possibility of reading out stale values.
  const_cast<LayoutTable*>(this)->RecalcBordersInRowDirection();
  // FIXME: Restructure the table layout code so that we can make this method
  // const.
  const_cast<LayoutTable*>(this)->table_layout_->ComputeIntrinsicLogicalWidths(
      min_width, max_width);

  // FIXME: We should include captions widths here like we do in
  // computePreferredLogicalWidths.
}

void LayoutTable::ComputePreferredLogicalWidths() {
  DCHECK(PreferredLogicalWidthsDirty());

  ComputeIntrinsicLogicalWidths(min_preferred_logical_width_,
                                max_preferred_logical_width_);

  int borders_padding_and_spacing =
      BordersPaddingAndSpacingInRowDirection().ToInt();
  min_preferred_logical_width_ += borders_padding_and_spacing;
  max_preferred_logical_width_ += borders_padding_and_spacing;

  table_layout_->ApplyPreferredLogicalWidthQuirks(min_preferred_logical_width_,
                                                  max_preferred_logical_width_);

  for (unsigned i = 0; i < captions_.size(); i++)
    min_preferred_logical_width_ = std::max(
        min_preferred_logical_width_, captions_[i]->MinPreferredLogicalWidth());

  const ComputedStyle& style_to_use = StyleRef();
  // FIXME: This should probably be checking for isSpecified since you should be
  // able to use percentage or calc values for min-width.
  if (style_to_use.LogicalMinWidth().IsFixed() &&
      style_to_use.LogicalMinWidth().Value() > 0) {
    max_preferred_logical_width_ =
        std::max(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     style_to_use.LogicalMinWidth().Value()));
    min_preferred_logical_width_ =
        std::max(min_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     style_to_use.LogicalMinWidth().Value()));
  }

  // FIXME: This should probably be checking for isSpecified since you should be
  // able to use percentage or calc values for maxWidth.
  if (style_to_use.LogicalMaxWidth().IsFixed()) {
    // We don't constrain m_minPreferredLogicalWidth as the table should be at
    // least the size of its min-content, regardless of 'max-width'.
    max_preferred_logical_width_ =
        std::min(max_preferred_logical_width_,
                 AdjustContentBoxLogicalWidthForBoxSizing(
                     style_to_use.LogicalMaxWidth().Value()));
    max_preferred_logical_width_ =
        std::max(min_preferred_logical_width_, max_preferred_logical_width_);
  }

  // FIXME: We should be adding borderAndPaddingLogicalWidth here, but
  // m_tableLayout->computePreferredLogicalWidths already does, so a bunch of
  // tests break doing this naively.
  ClearPreferredLogicalWidthsDirty();
}

LayoutTableSection* LayoutTable::TopNonEmptySection() const {
  LayoutTableSection* section = TopSection();
  if (section && !section->NumRows())
    section = SectionBelow(section, kSkipEmptySections);
  return section;
}

void LayoutTable::SplitEffectiveColumn(unsigned index, unsigned first_span) {
  // We split the column at |index|, taking |firstSpan| cells from the span.
  DCHECK_GT(effective_columns_[index].span, first_span);
  effective_columns_.insert(index, first_span);
  effective_columns_[index + 1].span -= first_span;

  // Propagate the change in our columns representation to the sections that
  // don't need cell recalc. If they do, they will be synced up directly with
  // m_columns later.
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsTableSection())
      continue;

    LayoutTableSection* section = ToLayoutTableSection(child);
    if (section->NeedsCellRecalc())
      continue;

    section->SplitEffectiveColumn(index, first_span);
  }

  effective_column_positions_.Grow(NumEffectiveColumns() + 1);
}

void LayoutTable::AppendEffectiveColumn(unsigned span) {
  unsigned new_column_index = effective_columns_.size();
  effective_columns_.push_back(span);

  // Unless the table has cell(s) with colspan that exceed the number of columns
  // afforded by the other rows in the table we can use the fast path when
  // mapping columns to effective columns.
  if (span == 1 && no_cell_colspan_at_least_ + 1 == NumEffectiveColumns()) {
    no_cell_colspan_at_least_++;
  }

  // Propagate the change in our columns representation to the sections that
  // don't need cell recalc. If they do, they will be synced up directly with
  // m_columns later.
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsTableSection())
      continue;

    LayoutTableSection* section = ToLayoutTableSection(child);
    if (section->NeedsCellRecalc())
      continue;

    section->AppendEffectiveColumn(new_column_index);
  }

  effective_column_positions_.Grow(NumEffectiveColumns() + 1);
}

LayoutTableCol* LayoutTable::FirstColumn() const {
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsLayoutTableCol())
      return ToLayoutTableCol(child);
  }

  return nullptr;
}

void LayoutTable::UpdateColumnCache() const {
  DCHECK(has_col_elements_);
  DCHECK(column_layout_objects_.IsEmpty());
  DCHECK(!column_layout_objects_valid_);

  for (LayoutTableCol* column_layout_object = FirstColumn();
       column_layout_object;
       column_layout_object = column_layout_object->NextColumn()) {
    if (column_layout_object->IsTableColumnGroupWithColumnChildren())
      continue;
    column_layout_objects_.push_back(column_layout_object);
  }
  column_layout_objects_valid_ = true;
}

LayoutTable::ColAndColGroup LayoutTable::SlowColElementAtAbsoluteColumn(
    unsigned absolute_column_index) const {
  DCHECK(has_col_elements_);

  if (!column_layout_objects_valid_)
    UpdateColumnCache();

  unsigned column_count = 0;
  for (unsigned i = 0; i < column_layout_objects_.size(); i++) {
    LayoutTableCol* column_layout_object = column_layout_objects_[i];
    DCHECK(!column_layout_object->IsTableColumnGroupWithColumnChildren());
    unsigned span = column_layout_object->Span();
    unsigned start_col = column_count;
    DCHECK_GE(span, 1u);
    unsigned end_col = column_count + span - 1;
    column_count += span;
    if (column_count > absolute_column_index) {
      ColAndColGroup col_and_col_group;
      bool is_at_start_edge = start_col == absolute_column_index;
      bool is_at_end_edge = end_col == absolute_column_index;
      if (column_layout_object->IsTableColumnGroup()) {
        col_and_col_group.colgroup = column_layout_object;
        col_and_col_group.adjoins_start_border_of_col_group = is_at_start_edge;
        col_and_col_group.adjoins_end_border_of_col_group = is_at_end_edge;
      } else {
        col_and_col_group.col = column_layout_object;
        col_and_col_group.colgroup =
            column_layout_object->EnclosingColumnGroup();
        if (col_and_col_group.colgroup) {
          col_and_col_group.adjoins_start_border_of_col_group =
              is_at_start_edge && !col_and_col_group.col->PreviousSibling();
          col_and_col_group.adjoins_end_border_of_col_group =
              is_at_end_edge && !col_and_col_group.col->NextSibling();
        }
      }
      return col_and_col_group;
    }
  }
  return ColAndColGroup();
}

void LayoutTable::RecalcSections() const {
  DCHECK(needs_section_recalc_);

  head_ = nullptr;
  foot_ = nullptr;
  first_body_ = nullptr;
  has_col_elements_ = false;

  // We need to get valid pointers to caption, head, foot and first body again
  LayoutObject* next_sibling;
  for (LayoutObject* child = FirstChild(); child; child = next_sibling) {
    next_sibling = child->NextSibling();
    switch (child->Style()->Display()) {
      case EDisplay::kTableColumn:
      case EDisplay::kTableColumnGroup:
        has_col_elements_ = true;
        break;
      case EDisplay::kTableHeaderGroup:
        if (child->IsTableSection()) {
          LayoutTableSection* section = ToLayoutTableSection(child);
          if (!head_)
            head_ = section;
          else if (!first_body_)
            first_body_ = section;
          section->RecalcCellsIfNeeded();
        }
        break;
      case EDisplay::kTableFooterGroup:
        if (child->IsTableSection()) {
          LayoutTableSection* section = ToLayoutTableSection(child);
          if (!foot_)
            foot_ = section;
          else if (!first_body_)
            first_body_ = section;
          section->RecalcCellsIfNeeded();
        }
        break;
      case EDisplay::kTableRowGroup:
        if (child->IsTableSection()) {
          LayoutTableSection* section = ToLayoutTableSection(child);
          if (!first_body_)
            first_body_ = section;
          section->RecalcCellsIfNeeded();
        }
        break;
      default:
        break;
    }
  }

  // repair column count (addChild can grow it too much, because it always adds
  // elements to the last row of a section)
  unsigned max_cols = 0;
  for (LayoutObject* child = FirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsTableSection()) {
      LayoutTableSection* section = ToLayoutTableSection(child);
      unsigned section_cols = section->NumEffectiveColumns();
      if (section_cols > max_cols)
        max_cols = section_cols;
    }
  }

  effective_columns_.resize(max_cols);
  effective_column_positions_.resize(max_cols + 1);
  no_cell_colspan_at_least_ = CalcNoCellColspanAtLeast();

  DCHECK(SelfNeedsLayout());

  needs_section_recalc_ = false;
}

int LayoutTable::CalcBorderStart() const {
  if (!ShouldCollapseBorders())
    return LayoutBlock::BorderStart().ToInt();

  // Determined by the first cell of the first row. See the CSS 2.1 spec,
  // section 17.6.2.
  if (!NumEffectiveColumns())
    return 0;

  int border_width = 0;

  const BorderValue& table_start_border = Style()->BorderStart();
  if (table_start_border.Style() == EBorderStyle::kHidden)
    return 0;
  if (ComputedStyle::BorderStyleIsVisible(table_start_border.Style()))
    border_width = table_start_border.Width();

  // TODO(dgrogan): This logic doesn't properly account for the first column in
  // the first column-group case.
  if (LayoutTableCol* column =
          ColElementAtAbsoluteColumn(0).InnermostColOrColGroup()) {
    // FIXME: We don't account for direction on columns and column groups.
    const BorderValue& column_adjoining_border = column->Style()->BorderStart();
    if (column_adjoining_border.Style() == EBorderStyle::kHidden)
      return 0;
    if (ComputedStyle::BorderStyleIsVisible(column_adjoining_border.Style()))
      border_width =
          std::max<int>(border_width, column_adjoining_border.Width());
  }

  if (const LayoutTableSection* top_non_empty_section =
          this->TopNonEmptySection()) {
    const BorderValue& section_adjoining_border =
        top_non_empty_section->BorderAdjoiningTableStart();
    if (section_adjoining_border.Style() == EBorderStyle::kHidden)
      return 0;

    if (ComputedStyle::BorderStyleIsVisible(section_adjoining_border.Style()))
      border_width =
          std::max<int>(border_width, section_adjoining_border.Width());

    if (const LayoutTableCell* adjoining_start_cell =
            top_non_empty_section->FirstRowCellAdjoiningTableStart()) {
      // FIXME: Make this work with perpendicular and flipped cells.
      const BorderValue& start_cell_adjoining_border =
          adjoining_start_cell->BorderAdjoiningTableStart();
      if (start_cell_adjoining_border.Style() == EBorderStyle::kHidden)
        return 0;

      const BorderValue& first_row_adjoining_border =
          adjoining_start_cell->Row()->BorderAdjoiningTableStart();
      if (first_row_adjoining_border.Style() == EBorderStyle::kHidden)
        return 0;

      if (ComputedStyle::BorderStyleIsVisible(
              start_cell_adjoining_border.Style())) {
        border_width =
            std::max<int>(border_width, start_cell_adjoining_border.Width());
      }
      if (ComputedStyle::BorderStyleIsVisible(
              first_row_adjoining_border.Style())) {
        border_width =
            std::max<int>(border_width, first_row_adjoining_border.Width());
      }
    }
  }
  return (border_width + (Style()->IsLeftToRightDirection() ? 0 : 1)) / 2;
}

int LayoutTable::CalcBorderEnd() const {
  if (!ShouldCollapseBorders())
    return LayoutBlock::BorderEnd().ToInt();

  // Determined by the last cell of the first row. See the CSS 2.1 spec, section
  // 17.6.2.
  if (!NumEffectiveColumns())
    return 0;

  int border_width = 0;

  const BorderValue& table_end_border = Style()->BorderEnd();
  if (table_end_border.Style() == EBorderStyle::kHidden)
    return 0;
  if (ComputedStyle::BorderStyleIsVisible(table_end_border.Style()))
    border_width = table_end_border.Width();

  unsigned end_column = NumEffectiveColumns() - 1;

  // TODO(dgrogan): This logic doesn't properly account for the last column in
  // the last column-group case.
  if (LayoutTableCol* column =
          ColElementAtAbsoluteColumn(end_column).InnermostColOrColGroup()) {
    // FIXME: We don't account for direction on columns and column groups.
    const BorderValue& column_adjoining_border = column->Style()->BorderEnd();
    if (column_adjoining_border.Style() == EBorderStyle::kHidden)
      return 0;
    if (ComputedStyle::BorderStyleIsVisible(column_adjoining_border.Style()))
      border_width =
          std::max<int>(border_width, column_adjoining_border.Width());
  }

  if (const LayoutTableSection* top_non_empty_section =
          this->TopNonEmptySection()) {
    const BorderValue& section_adjoining_border =
        top_non_empty_section->BorderAdjoiningTableEnd();
    if (section_adjoining_border.Style() == EBorderStyle::kHidden)
      return 0;

    if (ComputedStyle::BorderStyleIsVisible(section_adjoining_border.Style()))
      border_width =
          std::max<int>(border_width, section_adjoining_border.Width());

    if (const LayoutTableCell* adjoining_end_cell =
            top_non_empty_section->FirstRowCellAdjoiningTableEnd()) {
      // FIXME: Make this work with perpendicular and flipped cells.
      const BorderValue& end_cell_adjoining_border =
          adjoining_end_cell->BorderAdjoiningTableEnd();
      if (end_cell_adjoining_border.Style() == EBorderStyle::kHidden)
        return 0;

      const BorderValue& first_row_adjoining_border =
          adjoining_end_cell->Row()->BorderAdjoiningTableEnd();
      if (first_row_adjoining_border.Style() == EBorderStyle::kHidden)
        return 0;

      if (ComputedStyle::BorderStyleIsVisible(
              end_cell_adjoining_border.Style())) {
        border_width =
            std::max<int>(border_width, end_cell_adjoining_border.Width());
      }
      if (ComputedStyle::BorderStyleIsVisible(
              first_row_adjoining_border.Style())) {
        border_width =
            std::max<int>(border_width, first_row_adjoining_border.Width());
      }
    }
  }
  return (border_width + (Style()->IsLeftToRightDirection() ? 1 : 0)) / 2;
}

void LayoutTable::RecalcBordersInRowDirection() {
  // FIXME: We need to compute the collapsed before / after borders in the same
  // fashion.
  border_start_ = CalcBorderStart();
  border_end_ = CalcBorderEnd();
}

LayoutUnit LayoutTable::BorderBefore() const {
  if (ShouldCollapseBorders()) {
    RecalcSectionsIfNeeded();
    return LayoutUnit(OuterBorderBefore());
  }
  return LayoutBlock::BorderBefore();
}

LayoutUnit LayoutTable::BorderAfter() const {
  if (ShouldCollapseBorders()) {
    RecalcSectionsIfNeeded();
    return LayoutUnit(OuterBorderAfter());
  }
  return LayoutBlock::BorderAfter();
}

int LayoutTable::OuterBorderBefore() const {
  if (!ShouldCollapseBorders())
    return 0;
  int border_width = 0;
  if (LayoutTableSection* top_section = this->TopSection()) {
    border_width = top_section->OuterBorderBefore();
    if (border_width < 0)
      return 0;  // Overridden by hidden
  }
  const BorderValue& tb = Style()->BorderBefore();
  if (tb.Style() == EBorderStyle::kHidden)
    return 0;
  if (ComputedStyle::BorderStyleIsVisible(tb.Style()))
    border_width = std::max<int>(border_width, tb.Width() / 2);
  return border_width;
}

int LayoutTable::OuterBorderAfter() const {
  if (!ShouldCollapseBorders())
    return 0;
  int border_width = 0;

  if (LayoutTableSection* section = BottomSection()) {
    border_width = section->OuterBorderAfter();
    if (border_width < 0)
      return 0;  // Overridden by hidden
  }
  const BorderValue& tb = Style()->BorderAfter();
  if (tb.Style() == EBorderStyle::kHidden)
    return 0;
  if (ComputedStyle::BorderStyleIsVisible(tb.Style()))
    border_width = std::max<int>(border_width, (tb.Width() + 1) / 2);
  return border_width;
}

int LayoutTable::OuterBorderStart() const {
  if (!ShouldCollapseBorders())
    return 0;

  int border_width = 0;

  const BorderValue& tb = Style()->BorderStart();
  if (tb.Style() == EBorderStyle::kHidden)
    return 0;
  if (ComputedStyle::BorderStyleIsVisible(tb.Style()))
    border_width =
        (tb.Width() + (Style()->IsLeftToRightDirection() ? 0 : 1)) / 2;

  bool all_hidden = true;
  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section)) {
    int sw = section->OuterBorderStart();
    if (sw < 0)
      continue;
    all_hidden = false;
    border_width = std::max(border_width, sw);
  }
  if (all_hidden)
    return 0;

  return border_width;
}

int LayoutTable::OuterBorderEnd() const {
  if (!ShouldCollapseBorders())
    return 0;

  int border_width = 0;

  const BorderValue& tb = Style()->BorderEnd();
  if (tb.Style() == EBorderStyle::kHidden)
    return 0;
  if (ComputedStyle::BorderStyleIsVisible(tb.Style()))
    border_width =
        (tb.Width() + (Style()->IsLeftToRightDirection() ? 1 : 0)) / 2;

  bool all_hidden = true;
  for (LayoutTableSection* section = TopSection(); section;
       section = SectionBelow(section)) {
    int sw = section->OuterBorderEnd();
    if (sw < 0)
      continue;
    all_hidden = false;
    border_width = std::max(border_width, sw);
  }
  if (all_hidden)
    return 0;

  return border_width;
}

LayoutTableSection* LayoutTable::SectionAbove(
    const LayoutTableSection* section,
    SkipEmptySectionsValue skip_empty_sections) const {
  RecalcSectionsIfNeeded();

  if (section == head_)
    return 0;

  LayoutObject* prev_section =
      section == foot_ ? LastChild() : section->PreviousSibling();
  while (prev_section) {
    if (prev_section->IsTableSection() && prev_section != head_ &&
        prev_section != foot_ &&
        (skip_empty_sections == kDoNotSkipEmptySections ||
         ToLayoutTableSection(prev_section)->NumRows()))
      break;
    prev_section = prev_section->PreviousSibling();
  }
  if (!prev_section && head_ &&
      (skip_empty_sections == kDoNotSkipEmptySections || head_->NumRows()))
    prev_section = head_;
  return ToLayoutTableSection(prev_section);
}

LayoutTableSection* LayoutTable::SectionBelow(
    const LayoutTableSection* section,
    SkipEmptySectionsValue skip_empty_sections) const {
  RecalcSectionsIfNeeded();

  if (section == foot_)
    return nullptr;

  LayoutObject* next_section =
      section == head_ ? FirstChild() : section->NextSibling();
  while (next_section) {
    if (next_section->IsTableSection() && next_section != head_ &&
        next_section != foot_ &&
        (skip_empty_sections == kDoNotSkipEmptySections ||
         ToLayoutTableSection(next_section)->NumRows()))
      break;
    next_section = next_section->NextSibling();
  }
  if (!next_section && foot_ &&
      (skip_empty_sections == kDoNotSkipEmptySections || foot_->NumRows()))
    next_section = foot_;
  return ToLayoutTableSection(next_section);
}

LayoutTableSection* LayoutTable::BottomSection() const {
  RecalcSectionsIfNeeded();

  if (foot_)
    return foot_;

  for (LayoutObject* child = LastChild(); child;
       child = child->PreviousSibling()) {
    if (child->IsTableSection())
      return ToLayoutTableSection(child);
  }

  return nullptr;
}

LayoutTableCell* LayoutTable::CellAbove(const LayoutTableCell* cell) const {
  RecalcSectionsIfNeeded();

  // Find the section and row to look in
  unsigned r = cell->RowIndex();
  LayoutTableSection* section = nullptr;
  unsigned r_above = 0;
  if (r > 0) {
    // cell is not in the first row, so use the above row in its own section
    section = cell->Section();
    r_above = r - 1;
  } else {
    section = SectionAbove(cell->Section(), kSkipEmptySections);
    if (section) {
      DCHECK(section->NumRows());
      r_above = section->NumRows() - 1;
    }
  }

  // Look up the cell in the section's grid, which requires effective col index
  if (section) {
    unsigned eff_col =
        AbsoluteColumnToEffectiveColumn(cell->AbsoluteColumnIndex());
    return section->PrimaryCellAt(r_above, eff_col);
  }
  return nullptr;
}

LayoutTableCell* LayoutTable::CellBelow(const LayoutTableCell* cell) const {
  RecalcSectionsIfNeeded();

  // Find the section and row to look in
  unsigned r = cell->RowIndex() + cell->RowSpan() - 1;
  LayoutTableSection* section = nullptr;
  unsigned r_below = 0;
  if (r < cell->Section()->NumRows() - 1) {
    // The cell is not in the last row, so use the next row in the section.
    section = cell->Section();
    r_below = r + 1;
  } else {
    section = SectionBelow(cell->Section(), kSkipEmptySections);
    if (section)
      r_below = 0;
  }

  // Look up the cell in the section's grid, which requires effective col index
  if (section) {
    unsigned eff_col =
        AbsoluteColumnToEffectiveColumn(cell->AbsoluteColumnIndex());
    return section->PrimaryCellAt(r_below, eff_col);
  }
  return nullptr;
}

LayoutTableCell* LayoutTable::CellBefore(const LayoutTableCell* cell) const {
  RecalcSectionsIfNeeded();

  LayoutTableSection* section = cell->Section();
  unsigned eff_col =
      AbsoluteColumnToEffectiveColumn(cell->AbsoluteColumnIndex());
  if (!eff_col)
    return nullptr;

  // If we hit a colspan back up to a real cell.
  return section->PrimaryCellAt(cell->RowIndex(), eff_col - 1);
}

LayoutTableCell* LayoutTable::CellAfter(const LayoutTableCell* cell) const {
  RecalcSectionsIfNeeded();

  unsigned eff_col = AbsoluteColumnToEffectiveColumn(
      cell->AbsoluteColumnIndex() + cell->ColSpan());
  return cell->Section()->PrimaryCellAt(cell->RowIndex(), eff_col);
}

int LayoutTable::BaselinePosition(FontBaseline baseline_type,
                                  bool first_line,
                                  LineDirectionMode direction,
                                  LinePositionMode line_position_mode) const {
  DCHECK_EQ(line_position_mode, kPositionOnContainingLine);
  int baseline = FirstLineBoxBaseline();
  if (baseline != -1) {
    if (IsInline())
      return BeforeMarginInLineDirection(direction) + baseline;
    return baseline;
  }

  return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                     line_position_mode);
}

int LayoutTable::InlineBlockBaseline(LineDirectionMode) const {
  // Tables are skipped when computing an inline-block's baseline.
  return -1;
}

int LayoutTable::FirstLineBoxBaseline() const {
  // The baseline of a 'table' is the same as the 'inline-table' baseline per
  // CSS 3 Flexbox (CSS 2.1 doesn't define the baseline of a 'table' only an
  // 'inline-table'). This is also needed to properly determine the baseline of
  // a cell if it has a table child.

  if (IsWritingModeRoot())
    return -1;

  RecalcSectionsIfNeeded();

  const LayoutTableSection* top_non_empty_section = this->TopNonEmptySection();
  if (!top_non_empty_section)
    return -1;

  int baseline = top_non_empty_section->FirstLineBoxBaseline();
  if (baseline >= 0)
    return (top_non_empty_section->LogicalTop() + baseline).ToInt();

  // FF, Presto and IE use the top of the section as the baseline if its first
  // row is empty of cells or content.
  // The baseline of an empty row isn't specified by CSS 2.1.
  if (top_non_empty_section->FirstRow() &&
      !top_non_empty_section->FirstRow()->FirstCell())
    return top_non_empty_section->LogicalTop().ToInt();

  return -1;
}

LayoutRect LayoutTable::OverflowClipRect(
    const LayoutPoint& location,
    OverlayScrollbarClipBehavior overlay_scrollbar_clip_behavior) const {
  LayoutRect rect =
      LayoutBlock::OverflowClipRect(location, overlay_scrollbar_clip_behavior);

  // If we have a caption, expand the clip to include the caption.
  // FIXME: Technically this is wrong, but it's virtually impossible to fix this
  // for real until captions have been re-written.
  // FIXME: This code assumes (like all our other caption code) that only
  // top/bottom are supported.  When we actually support left/right and stop
  // mapping them to top/bottom, we might have to hack this code first
  // (depending on what order we do these bug fixes in).
  if (!captions_.IsEmpty()) {
    if (Style()->IsHorizontalWritingMode()) {
      rect.SetHeight(Size().Height());
      rect.SetY(location.Y());
    } else {
      rect.SetWidth(Size().Width());
      rect.SetX(location.X());
    }
  }

  return rect;
}

bool LayoutTable::NodeAtPoint(HitTestResult& result,
                              const HitTestLocation& location_in_container,
                              const LayoutPoint& accumulated_offset,
                              HitTestAction action) {
  LayoutPoint adjusted_location = accumulated_offset + Location();

  // Check kids first.
  if (!HasOverflowClip() ||
      location_in_container.Intersects(OverflowClipRect(adjusted_location))) {
    for (LayoutObject* child = LastChild(); child;
         child = child->PreviousSibling()) {
      if (child->IsBox() && !ToLayoutBox(child)->HasSelfPaintingLayer() &&
          (child->IsTableSection() || child->IsTableCaption())) {
        LayoutPoint child_point =
            FlipForWritingModeForChild(ToLayoutBox(child), adjusted_location);
        if (child->NodeAtPoint(result, location_in_container, child_point,
                               action)) {
          UpdateHitTestResult(
              result,
              ToLayoutPoint(location_in_container.Point() - child_point));
          return true;
        }
      }
    }
  }

  // Check our bounds next.
  LayoutRect bounds_rect(adjusted_location, Size());
  if (VisibleToHitTestRequest(result.GetHitTestRequest()) &&
      (action == kHitTestBlockBackground ||
       action == kHitTestChildBlockBackground) &&
      location_in_container.Intersects(bounds_rect)) {
    UpdateHitTestResult(result,
                        FlipForWritingMode(location_in_container.Point() -
                                           ToLayoutSize(adjusted_location)));
    if (result.AddNodeToListBasedTestResult(GetNode(), location_in_container,
                                            bounds_rect) == kStopHitTesting)
      return true;
  }

  return false;
}

LayoutTable* LayoutTable::CreateAnonymousWithParent(
    const LayoutObject* parent) {
  RefPtr<ComputedStyle> new_style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(
          parent->StyleRef(),
          parent->IsLayoutInline() ? EDisplay::kInlineTable : EDisplay::kTable);
  LayoutTable* new_table = new LayoutTable(nullptr);
  new_table->SetDocumentForAnonymous(&parent->GetDocument());
  new_table->SetStyle(std::move(new_style));
  return new_table;
}

BorderValue LayoutTable::TableStartBorderAdjoiningCell(
    const LayoutTableCell* cell) const {
#if DCHECK_IS_ON()
  DCHECK(cell->IsFirstOrLastCellInRow());
#endif
  if (HasSameDirectionAs(cell->Row()))
    return Style()->BorderStart();

  return Style()->BorderEnd();
}

BorderValue LayoutTable::TableEndBorderAdjoiningCell(
    const LayoutTableCell* cell) const {
#if DCHECK_IS_ON()
  DCHECK(cell->IsFirstOrLastCellInRow());
#endif
  if (HasSameDirectionAs(cell->Row()))
    return Style()->BorderEnd();

  return Style()->BorderStart();
}

void LayoutTable::EnsureIsReadyForPaintInvalidation() {
  LayoutBlock::EnsureIsReadyForPaintInvalidation();
  RecalcCollapsedBordersIfNeeded();
}

PaintInvalidationReason LayoutTable::DeprecatedInvalidatePaint(
    const PaintInvalidationState& paint_invalidation_state) {
  if (ShouldCollapseBorders() && !collapsed_borders_.IsEmpty())
    paint_invalidation_state.PaintingLayer()
        .SetNeedsPaintPhaseDescendantBlockBackgrounds();

  return LayoutBlock::DeprecatedInvalidatePaint(paint_invalidation_state);
}

PaintInvalidationReason LayoutTable::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  return TablePaintInvalidator(*this, context).InvalidatePaint();
}

LayoutUnit LayoutTable::PaddingTop() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  return LayoutBlock::PaddingTop();
}

LayoutUnit LayoutTable::PaddingBottom() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  return LayoutBlock::PaddingBottom();
}

LayoutUnit LayoutTable::PaddingLeft() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  return LayoutBlock::PaddingLeft();
}

LayoutUnit LayoutTable::PaddingRight() const {
  if (ShouldCollapseBorders())
    return LayoutUnit();

  return LayoutBlock::PaddingRight();
}

}  // namespace blink
