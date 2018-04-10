/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"

#include <limits>
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/layout/flexible_box_algorithm.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/min_max_size.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/length_functions.h"
#include "third_party/blink/renderer/platform/wtf/auto_reset.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static bool HasAspectRatio(const LayoutBox& child) {
  return child.IsImage() || child.IsCanvas() || child.IsVideo();
}

LayoutFlexibleBox::LayoutFlexibleBox(Element* element)
    : LayoutBlock(element),
      order_iterator_(this),
      number_of_in_flow_children_on_first_line_(-1),
      has_definite_height_(SizeDefiniteness::kUnknown),
      in_layout_(false) {
  DCHECK(!ChildrenInline());
  if (!IsAnonymous())
    UseCounter::Count(GetDocument(), WebFeature::kCSSFlexibleBox);
}

LayoutFlexibleBox::~LayoutFlexibleBox() = default;

LayoutFlexibleBox* LayoutFlexibleBox::CreateAnonymous(Document* document) {
  LayoutFlexibleBox* layout_object = new LayoutFlexibleBox(nullptr);
  layout_object->SetDocumentForAnonymous(document);
  return layout_object;
}

void LayoutFlexibleBox::ComputeIntrinsicLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
  // honoring it though until the flex shorthand stops setting it to 0. See
  // https://bugs.webkit.org/show_bug.cgi?id=116117 and
  // https://crbug.com/240765.
  float previous_max_content_flex_fraction = -1;
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;

    LayoutUnit margin = MarginIntrinsicLogicalWidthForChild(*child);

    LayoutUnit min_preferred_logical_width;
    LayoutUnit max_preferred_logical_width;
    ComputeChildPreferredLogicalWidths(*child, min_preferred_logical_width,
                                       max_preferred_logical_width);
    DCHECK_GE(min_preferred_logical_width, LayoutUnit());
    DCHECK_GE(max_preferred_logical_width, LayoutUnit());
    min_preferred_logical_width += margin;
    max_preferred_logical_width += margin;
    if (!IsColumnFlow()) {
      max_logical_width += max_preferred_logical_width;
      if (IsMultiline()) {
        // For multiline, the min preferred width is if you put a break between
        // each item.
        min_logical_width =
            std::max(min_logical_width, min_preferred_logical_width);
      } else {
        min_logical_width += min_preferred_logical_width;
      }
    } else {
      min_logical_width =
          std::max(min_preferred_logical_width, min_logical_width);
      max_logical_width =
          std::max(max_preferred_logical_width, max_logical_width);
    }

    previous_max_content_flex_fraction = CountIntrinsicSizeForAlgorithmChange(
        max_preferred_logical_width, child, previous_max_content_flex_fraction);
  }

  max_logical_width = std::max(min_logical_width, max_logical_width);

  // Due to negative margins, it is possible that we calculated a negative
  // intrinsic width. Make sure that we never return a negative width.
  min_logical_width = std::max(LayoutUnit(), min_logical_width);
  max_logical_width = std::max(LayoutUnit(), max_logical_width);

  LayoutUnit scrollbar_width(ScrollbarLogicalWidth());
  max_logical_width += scrollbar_width;
  min_logical_width += scrollbar_width;
}

float LayoutFlexibleBox::CountIntrinsicSizeForAlgorithmChange(
    LayoutUnit max_preferred_logical_width,
    LayoutBox* child,
    float previous_max_content_flex_fraction) const {
  // Determine whether the new version of the intrinsic size algorithm of the
  // flexbox spec would produce a different result than our above algorithm.
  // The algorithm produces a different result iff the max-content flex
  // fraction (as defined in the new algorithm) is not identical for each flex
  // item.
  if (IsColumnFlow())
    return previous_max_content_flex_fraction;
  Length flex_basis = child->StyleRef().FlexBasis();
  float flex_grow = child->StyleRef().FlexGrow();
  // A flex-basis of auto will lead to a max-content flex fraction of zero, so
  // just like an inflexible item it would compute to a size of max-content, so
  // we ignore it here.
  if (flex_basis.IsAuto() || flex_grow == 0)
    return previous_max_content_flex_fraction;
  flex_grow = std::max(1.0f, flex_grow);
  float max_content_flex_fraction =
      max_preferred_logical_width.ToFloat() / flex_grow;
  if (previous_max_content_flex_fraction != -1 &&
      max_content_flex_fraction != previous_max_content_flex_fraction) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kFlexboxIntrinsicSizeAlgorithmIsDifferent);
  }
  return max_content_flex_fraction;
}

LayoutUnit LayoutFlexibleBox::SynthesizedBaselineFromContentBox(
    const LayoutBox& box,
    LineDirectionMode direction) {
  if (direction == kHorizontalLine) {
    return box.Size().Height() - box.BorderBottom() - box.PaddingBottom() -
           box.VerticalScrollbarWidth();
  }
  return box.Size().Width() - box.BorderLeft() - box.PaddingLeft() -
         box.HorizontalScrollbarHeight();
}

LayoutUnit LayoutFlexibleBox::BaselinePosition(FontBaseline,
                                               bool,
                                               LineDirectionMode direction,
                                               LinePositionMode mode) const {
  DCHECK_EQ(mode, kPositionOnContainingLine);
  LayoutUnit baseline = FirstLineBoxBaseline();
  if (baseline == -1)
    baseline = SynthesizedBaselineFromContentBox(*this, direction);

  return BeforeMarginInLineDirection(direction) + baseline;
}

LayoutUnit LayoutFlexibleBox::FirstLineBoxBaseline() const {
  if (IsWritingModeRoot() || number_of_in_flow_children_on_first_line_ <= 0)
    return LayoutUnit(-1);
  LayoutBox* baseline_child = nullptr;
  int child_number = 0;
  for (LayoutBox* child = order_iterator_.First(); child;
       child = order_iterator_.Next()) {
    if (child->IsOutOfFlowPositioned())
      continue;
    if (FlexLayoutAlgorithm::AlignmentForChild(StyleRef(), child->StyleRef()) ==
            ItemPosition::kBaseline &&
        !HasAutoMarginsInCrossAxis(*child)) {
      baseline_child = child;
      break;
    }
    if (!baseline_child)
      baseline_child = child;

    ++child_number;
    if (child_number == number_of_in_flow_children_on_first_line_)
      break;
  }

  if (!baseline_child)
    return LayoutUnit(-1);

  if (!IsColumnFlow() && HasOrthogonalFlow(*baseline_child)) {
    // TODO(cbiesinger): Should LogicalTop here be LogicalLeft?
    return CrossAxisExtentForChild(*baseline_child) +
           baseline_child->LogicalTop();
  }
  if (IsColumnFlow() && !HasOrthogonalFlow(*baseline_child)) {
    return MainAxisExtentForChild(*baseline_child) +
           baseline_child->LogicalTop();
  }

  LayoutUnit baseline = baseline_child->FirstLineBoxBaseline();
  if (baseline == -1) {
    // FIXME: We should pass |direction| into firstLineBoxBaseline and stop
    // bailing out if we're a writing mode root. This would also fix some
    // cases where the flexbox is orthogonal to its container.
    LineDirectionMode direction =
        IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine;
    return SynthesizedBaselineFromContentBox(*baseline_child, direction) +
           baseline_child->LogicalTop();
  }

  return baseline + baseline_child->LogicalTop();
}

LayoutUnit LayoutFlexibleBox::InlineBlockBaseline(
    LineDirectionMode direction) const {
  LayoutUnit baseline = FirstLineBoxBaseline();
  if (baseline != -1)
    return baseline;

  LayoutUnit margin_ascent =
      direction == kHorizontalLine ? MarginTop() : MarginRight();
  return SynthesizedBaselineFromContentBox(*this, direction) + margin_ascent;
}

IntSize LayoutFlexibleBox::OriginAdjustmentForScrollbars() const {
  IntSize size;
  int adjustment_width = VerticalScrollbarWidth();
  int adjustment_height = HorizontalScrollbarHeight();
  if (!adjustment_width && !adjustment_height)
    return size;

  EFlexDirection flex_direction = Style()->FlexDirection();
  TextDirection text_direction = Style()->Direction();
  WritingMode writing_mode = Style()->GetWritingMode();

  if (flex_direction == EFlexDirection::kRow) {
    if (text_direction == TextDirection::kRtl) {
      if (blink::IsHorizontalWritingMode(writing_mode))
        size.Expand(adjustment_width, 0);
      else
        size.Expand(0, adjustment_height);
    }
    if (IsFlippedBlocksWritingMode(writing_mode))
      size.Expand(adjustment_width, 0);
  } else if (flex_direction == EFlexDirection::kRowReverse) {
    if (text_direction == TextDirection::kLtr) {
      if (blink::IsHorizontalWritingMode(writing_mode))
        size.Expand(adjustment_width, 0);
      else
        size.Expand(0, adjustment_height);
    }
    if (IsFlippedBlocksWritingMode(writing_mode))
      size.Expand(adjustment_width, 0);
  } else if (flex_direction == EFlexDirection::kColumn) {
    if (IsFlippedBlocksWritingMode(writing_mode))
      size.Expand(adjustment_width, 0);
  } else {
    if (blink::IsHorizontalWritingMode(writing_mode))
      size.Expand(0, adjustment_height);
    else if (IsFlippedLinesWritingMode(writing_mode))
      size.Expand(adjustment_width, 0);
  }
  return size;
}

IntSize LayoutFlexibleBox::ScrolledContentOffset() const {
  DCHECK(HasOverflowClip());
  DCHECK(HasLayer());
  // FIXME: Return DoubleSize here. crbug.com/414283.
  PaintLayerScrollableArea* scrollable_area = GetScrollableArea();
  IntSize result =
      scrollable_area->ScrollOffsetInt() + OriginAdjustmentForScrollbars();
  if (IsHorizontalWritingMode() &&
      ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    result.Expand(-VerticalScrollbarWidth(), 0);
  return result;
}

bool LayoutFlexibleBox::HasTopOverflow() const {
  EFlexDirection flex_direction = Style()->FlexDirection();
  if (IsHorizontalWritingMode())
    return flex_direction == EFlexDirection::kColumnReverse;
  return flex_direction == (Style()->IsLeftToRightDirection()
                                ? EFlexDirection::kRowReverse
                                : EFlexDirection::kRow);
}

bool LayoutFlexibleBox::HasLeftOverflow() const {
  EFlexDirection flex_direction = Style()->FlexDirection();
  if (IsHorizontalWritingMode()) {
    return flex_direction == (Style()->IsLeftToRightDirection()
                                  ? EFlexDirection::kRowReverse
                                  : EFlexDirection::kRow);
  }
  return flex_direction == EFlexDirection::kColumnReverse;
}

void LayoutFlexibleBox::MergeAnonymousFlexItems(LayoutObject* remove_child) {
  // When we remove a flex item, and the previous and next siblings of the item
  // are text nodes wrapped in anonymous flex items, the adjacent text nodes
  // need to be merged into the same flex item.
  LayoutObject* prev = remove_child->PreviousSibling();
  if (!prev || !prev->IsAnonymousBlock())
    return;
  LayoutObject* next = remove_child->NextSibling();
  if (!next || !next->IsAnonymousBlock())
    return;
  ToLayoutBoxModelObject(next)->MoveAllChildrenTo(ToLayoutBoxModelObject(prev));
  ToLayoutBlockFlow(next)->DeleteLineBoxTree();
  next->Destroy();
  intrinsic_size_along_main_axis_.erase(next);
}

void LayoutFlexibleBox::RemoveChild(LayoutObject* child) {
  if (!DocumentBeingDestroyed())
    MergeAnonymousFlexItems(child);

  LayoutBlock::RemoveChild(child);
  intrinsic_size_along_main_axis_.erase(child);
}

void LayoutFlexibleBox::StyleDidChange(StyleDifference diff,
                                       const ComputedStyle* old_style) {
  LayoutBlock::StyleDidChange(diff, old_style);

  if (old_style &&
      old_style->ResolvedAlignItems(SelfAlignmentNormalBehavior())
              .GetPosition() == ItemPosition::kStretch &&
      diff.NeedsFullLayout()) {
    // Flex items that were previously stretching need to be relayed out so we
    // can compute new available cross axis space. This is only necessary for
    // stretching since other alignment values don't change the size of the
    // box.
    for (LayoutBox* child = FirstChildBox(); child;
         child = child->NextSiblingBox()) {
      ItemPosition previous_alignment =
          child->StyleRef()
              .ResolvedAlignSelf(SelfAlignmentNormalBehavior(), old_style)
              .GetPosition();
      if (previous_alignment == ItemPosition::kStretch &&
          previous_alignment !=
              child->StyleRef()
                  .ResolvedAlignSelf(SelfAlignmentNormalBehavior(), Style())
                  .GetPosition())
        child->SetChildNeedsLayout(kMarkOnlyThis);
    }
  }
}

void LayoutFlexibleBox::UpdateBlockLayout(bool relayout_children) {
  DCHECK(NeedsLayout());

  if (!relayout_children && SimplifiedLayout())
    return;

  relaid_out_children_.clear();
  WTF::AutoReset<bool> reset1(&in_layout_, true);
  DCHECK_EQ(has_definite_height_, SizeDefiniteness::kUnknown);

  if (UpdateLogicalWidthAndColumnWidth())
    relayout_children = true;

  SubtreeLayoutScope layout_scope(*this);
  LayoutUnit previous_height = LogicalHeight();
  SetLogicalHeight(BorderAndPaddingLogicalHeight() + ScrollbarLogicalHeight());

  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  {
    TextAutosizer::LayoutScope text_autosizer_layout_scope(this, &layout_scope);
    LayoutState state(*this);

    number_of_in_flow_children_on_first_line_ = -1;

    PrepareOrderIteratorAndMargins();

    LayoutFlexItems(relayout_children, layout_scope);
    if (PaintLayerScrollableArea::PreventRelayoutScope::RelayoutNeeded()) {
      // Recompute the logical width, because children may have added or removed
      // scrollbars.
      UpdateLogicalWidthAndColumnWidth();
      PaintLayerScrollableArea::FreezeScrollbarsScope freeze_scrollbars_scope;
      PrepareOrderIteratorAndMargins();
      LayoutFlexItems(true, layout_scope);
      PaintLayerScrollableArea::PreventRelayoutScope::ResetRelayoutNeeded();
    }

    if (LogicalHeight() != previous_height)
      relayout_children = true;

    LayoutPositionedObjects(relayout_children || IsDocumentElement());

    // FIXME: css3/flexbox/repaint-rtl-column.html seems to issue paint
    // invalidations for more overflow than it needs to.
    ComputeOverflow(ClientLogicalBottomAfterRepositioning());
  }

  // We have to reset this, because changes to our ancestors' style can affect
  // this value. Also, this needs to be before we call updateAfterLayout, as
  // that function may re-enter this one.
  has_definite_height_ = SizeDefiniteness::kUnknown;

  // Update our scroll information if we're overflow:auto/scroll/hidden now
  // that we know if we overflow or not.
  UpdateAfterLayout();

  ClearNeedsLayout();
}

void LayoutFlexibleBox::PaintChildren(const PaintInfo& paint_info,
                                      const LayoutPoint& paint_offset) const {
  BlockPainter::PaintChildrenOfFlexibleBox(*this, paint_info, paint_offset);
}

void LayoutFlexibleBox::RepositionLogicalHeightDependentFlexItems(
    Vector<FlexLine>& line_contexts) {
  LayoutUnit cross_axis_start_edge = line_contexts.IsEmpty()
                                         ? LayoutUnit()
                                         : line_contexts[0].cross_axis_offset;
  AlignFlexLines(line_contexts);

  AlignChildren(line_contexts);

  if (Style()->FlexWrap() == EFlexWrap::kWrapReverse)
    FlipForWrapReverse(line_contexts, cross_axis_start_edge);

  // direction:rtl + flex-direction:column means the cross-axis direction is
  // flipped.
  FlipForRightToLeftColumn(line_contexts);
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::ClientLogicalBottomAfterRepositioning() {
  LayoutUnit max_child_logical_bottom;
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if (child->IsOutOfFlowPositioned())
      continue;
    LayoutUnit child_logical_bottom = LogicalTopForChild(*child) +
                                      LogicalHeightForChild(*child) +
                                      MarginAfterForChild(*child);
    max_child_logical_bottom =
        std::max(max_child_logical_bottom, child_logical_bottom);
  }
  return std::max(ClientLogicalBottom(),
                  max_child_logical_bottom + PaddingAfter());
}

bool LayoutFlexibleBox::HasOrthogonalFlow(const LayoutBox& child) const {
  return IsHorizontalFlow() != child.IsHorizontalWritingMode();
}

bool LayoutFlexibleBox::IsColumnFlow() const {
  return Style()->IsColumnFlexDirection();
}

bool LayoutFlexibleBox::IsHorizontalFlow() const {
  if (IsHorizontalWritingMode())
    return !IsColumnFlow();
  return IsColumnFlow();
}

bool LayoutFlexibleBox::IsLeftToRightFlow() const {
  if (IsColumnFlow()) {
    return blink::IsHorizontalWritingMode(Style()->GetWritingMode()) ||
           IsFlippedLinesWritingMode(Style()->GetWritingMode());
  }
  return Style()->IsLeftToRightDirection() ^
         (Style()->FlexDirection() == EFlexDirection::kRowReverse);
}

bool LayoutFlexibleBox::IsMultiline() const {
  return Style()->FlexWrap() != EFlexWrap::kNowrap;
}

Length LayoutFlexibleBox::FlexBasisForChild(const LayoutBox& child) const {
  Length flex_length = child.Style()->FlexBasis();
  if (flex_length.IsAuto())
    flex_length =
        IsHorizontalFlow() ? child.Style()->Width() : child.Style()->Height();
  return flex_length;
}

LayoutUnit LayoutFlexibleBox::CrossAxisExtentForChild(
    const LayoutBox& child) const {
  return IsHorizontalFlow() ? child.Size().Height() : child.Size().Width();
}

LayoutUnit LayoutFlexibleBox::ChildIntrinsicLogicalHeight(
    const LayoutBox& child) const {
  // This should only be called if the logical height is the cross size
  DCHECK(!HasOrthogonalFlow(child));
  if (NeedToStretchChildLogicalHeight(child)) {
    LayoutUnit child_intrinsic_content_logical_height;
    if (!child.StyleRef().ContainsSize()) {
      child_intrinsic_content_logical_height =
          child.IntrinsicContentLogicalHeight();
    }
    LayoutUnit child_intrinsic_logical_height =
        child_intrinsic_content_logical_height +
        child.ScrollbarLogicalHeight() + child.BorderAndPaddingLogicalHeight();
    return child.ConstrainLogicalHeightByMinMax(
        child_intrinsic_logical_height, child_intrinsic_content_logical_height);
  }
  return child.LogicalHeight();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::ChildIntrinsicLogicalWidth(
    const LayoutBox& child) const {
  // This should only be called if the logical width is the cross size
  DCHECK(HasOrthogonalFlow(child));
  // If our height is auto, make sure that our returned height is unaffected by
  // earlier layouts by returning the max preferred logical width
  if (!CrossAxisLengthIsDefinite(child, child.StyleRef().LogicalWidth()))
    return child.MaxPreferredLogicalWidth();

  return child.LogicalWidth();
}

LayoutUnit LayoutFlexibleBox::CrossAxisIntrinsicExtentForChild(
    const LayoutBox& child) const {
  return HasOrthogonalFlow(child) ? ChildIntrinsicLogicalWidth(child)
                                  : ChildIntrinsicLogicalHeight(child);
}

LayoutUnit LayoutFlexibleBox::MainAxisExtentForChild(
    const LayoutBox& child) const {
  return IsHorizontalFlow() ? child.Size().Width() : child.Size().Height();
}

LayoutUnit LayoutFlexibleBox::MainAxisContentExtentForChildIncludingScrollbar(
    const LayoutBox& child) const {
  return IsHorizontalFlow()
             ? child.ContentWidth() + child.VerticalScrollbarWidth()
             : child.ContentHeight() + child.HorizontalScrollbarHeight();
}

LayoutUnit LayoutFlexibleBox::CrossAxisExtent() const {
  return IsHorizontalFlow() ? Size().Height() : Size().Width();
}

LayoutUnit LayoutFlexibleBox::CrossAxisContentExtent() const {
  return IsHorizontalFlow() ? ContentHeight() : ContentWidth();
}

LayoutUnit LayoutFlexibleBox::MainAxisContentExtent(
    LayoutUnit content_logical_height) {
  if (IsColumnFlow()) {
    LogicalExtentComputedValues computed_values;
    LayoutUnit border_padding_and_scrollbar =
        BorderAndPaddingLogicalHeight() + ScrollbarLogicalHeight();
    LayoutUnit border_box_logical_height =
        content_logical_height + border_padding_and_scrollbar;
    ComputeLogicalHeight(border_box_logical_height, LogicalTop(),
                         computed_values);
    if (computed_values.extent_ == LayoutUnit::Max())
      return computed_values.extent_;
    return std::max(LayoutUnit(),
                    computed_values.extent_ - border_padding_and_scrollbar);
  }
  return ContentLogicalWidth();
}

LayoutUnit LayoutFlexibleBox::ComputeMainAxisExtentForChild(
    const LayoutBox& child,
    SizeType size_type,
    const Length& size) const {
  // If we have a horizontal flow, that means the main size is the width.
  // That's the logical width for horizontal writing modes, and the logical
  // height in vertical writing modes. For a vertical flow, main size is the
  // height, so it's the inverse. So we need the logical width if we have a
  // horizontal flow and horizontal writing mode, or vertical flow and vertical
  // writing mode. Otherwise we need the logical height.
  if (IsHorizontalFlow() != child.StyleRef().IsHorizontalWritingMode()) {
    // We don't have to check for "auto" here - computeContentLogicalHeight
    // will just return -1 for that case anyway. It's safe to access
    // scrollbarLogicalHeight here because ComputeNextFlexLine will have
    // already forced layout on the child. We previously layed out the child
    // if necessary (see ComputeNextFlexLine and the call to
    // childHasIntrinsicMainAxisSize) so we can be sure that the two height
    // calls here will return up-to-date data.
    return child.ComputeContentLogicalHeight(
               size_type, size, child.IntrinsicContentLogicalHeight()) +
           child.ScrollbarLogicalHeight();
  }
  // computeLogicalWidth always re-computes the intrinsic widths. However, when
  // our logical width is auto, we can just use our cached value. So let's do
  // that here. (Compare code in LayoutBlock::computePreferredLogicalWidths)
  LayoutUnit border_and_padding = child.BorderAndPaddingLogicalWidth();
  if (child.StyleRef().LogicalWidth().IsAuto() && !HasAspectRatio(child)) {
    if (size.GetType() == kMinContent)
      return child.MinPreferredLogicalWidth() - border_and_padding;
    if (size.GetType() == kMaxContent)
      return child.MaxPreferredLogicalWidth() - border_and_padding;
  }
  return child.ComputeLogicalWidthUsing(size_type, size, ContentLogicalWidth(),
                                        this) -
         border_and_padding;
}

LayoutUnit LayoutFlexibleBox::FlowAwareBorderStart() const {
  if (IsHorizontalFlow())
    return IsLeftToRightFlow() ? BorderLeft() : BorderRight();
  return IsLeftToRightFlow() ? BorderTop() : BorderBottom();
}

LayoutUnit LayoutFlexibleBox::FlowAwareBorderEnd() const {
  if (IsHorizontalFlow())
    return IsLeftToRightFlow() ? BorderRight() : BorderLeft();
  return IsLeftToRightFlow() ? BorderBottom() : BorderTop();
}

LayoutUnit LayoutFlexibleBox::FlowAwareBorderBefore() const {
  switch (FlexLayoutAlgorithm::GetTransformedWritingMode(StyleRef())) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return BorderTop();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return BorderBottom();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return BorderLeft();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return BorderRight();
  }
  NOTREACHED();
  return BorderTop();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::FlowAwareBorderAfter() const {
  switch (FlexLayoutAlgorithm::GetTransformedWritingMode(StyleRef())) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return BorderBottom();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return BorderTop();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return BorderRight();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return BorderLeft();
  }
  NOTREACHED();
  return BorderTop();
}

LayoutUnit LayoutFlexibleBox::FlowAwarePaddingStart() const {
  if (IsHorizontalFlow())
    return IsLeftToRightFlow() ? PaddingLeft() : PaddingRight();
  return IsLeftToRightFlow() ? PaddingTop() : PaddingBottom();
}

LayoutUnit LayoutFlexibleBox::FlowAwarePaddingEnd() const {
  if (IsHorizontalFlow())
    return IsLeftToRightFlow() ? PaddingRight() : PaddingLeft();
  return IsLeftToRightFlow() ? PaddingBottom() : PaddingTop();
}

LayoutUnit LayoutFlexibleBox::FlowAwarePaddingBefore() const {
  switch (FlexLayoutAlgorithm::GetTransformedWritingMode(StyleRef())) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return PaddingTop();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return PaddingBottom();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return PaddingLeft();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return PaddingRight();
  }
  NOTREACHED();
  return PaddingTop();
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::FlowAwarePaddingAfter() const {
  switch (FlexLayoutAlgorithm::GetTransformedWritingMode(StyleRef())) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return PaddingBottom();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return PaddingTop();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return PaddingRight();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return PaddingLeft();
  }
  NOTREACHED();
  return PaddingTop();
}

LayoutUnit LayoutFlexibleBox::CrossAxisScrollbarExtent() const {
  return LayoutUnit(IsHorizontalFlow() ? HorizontalScrollbarHeight()
                                       : VerticalScrollbarWidth());
}

LayoutUnit LayoutFlexibleBox::CrossAxisScrollbarExtentForChild(
    const LayoutBox& child) const {
  return LayoutUnit(IsHorizontalFlow() ? child.HorizontalScrollbarHeight()
                                       : child.VerticalScrollbarWidth());
}

LayoutPoint LayoutFlexibleBox::FlowAwareLocationForChild(
    const LayoutBox& child) const {
  return IsHorizontalFlow() ? child.Location()
                            : child.Location().TransposedPoint();
}

bool LayoutFlexibleBox::UseChildAspectRatio(const LayoutBox& child) const {
  if (!HasAspectRatio(child))
    return false;
  if (child.IntrinsicSize().Height() == 0) {
    // We can't compute a ratio in this case.
    return false;
  }
  Length cross_size;
  if (IsHorizontalFlow())
    cross_size = child.StyleRef().Height();
  else
    cross_size = child.StyleRef().Width();
  return CrossAxisLengthIsDefinite(child, cross_size);
}

LayoutUnit LayoutFlexibleBox::ComputeMainSizeFromAspectRatioUsing(
    const LayoutBox& child,
    Length cross_size_length) const {
  DCHECK(HasAspectRatio(child));
  DCHECK_NE(child.IntrinsicSize().Height(), 0);

  LayoutUnit cross_size;
  if (cross_size_length.IsFixed()) {
    cross_size = LayoutUnit(cross_size_length.Value());
  } else {
    DCHECK(cross_size_length.IsPercentOrCalc());
    cross_size = HasOrthogonalFlow(child)
                     ? AdjustBorderBoxLogicalWidthForBoxSizing(
                           ValueForLength(cross_size_length, ContentWidth()))
                     : child.ComputePercentageLogicalHeight(cross_size_length);
  }

  const LayoutSize& child_intrinsic_size = child.IntrinsicSize();
  double ratio = child_intrinsic_size.Width().ToFloat() /
                 child_intrinsic_size.Height().ToFloat();
  if (IsHorizontalFlow())
    return LayoutUnit(cross_size * ratio);
  return LayoutUnit(cross_size / ratio);
}

void LayoutFlexibleBox::SetFlowAwareLocationForChild(
    LayoutBox& child,
    const LayoutPoint& location) {
  if (IsHorizontalFlow())
    child.SetLocationAndUpdateOverflowControlsIfNeeded(location);
  else
    child.SetLocationAndUpdateOverflowControlsIfNeeded(
        location.TransposedPoint());
}

bool LayoutFlexibleBox::MainAxisLengthIsDefinite(
    const LayoutBox& child,
    const Length& flex_basis) const {
  if (flex_basis.IsAuto())
    return false;
  if (flex_basis.IsPercentOrCalc()) {
    if (!IsColumnFlow() || has_definite_height_ == SizeDefiniteness::kDefinite)
      return true;
    if (has_definite_height_ == SizeDefiniteness::kIndefinite)
      return false;
    bool definite = child.ComputePercentageLogicalHeight(flex_basis) != -1;
    if (in_layout_) {
      // We can reach this code even while we're not laying ourselves out, such
      // as from mainSizeForPercentageResolution.
      has_definite_height_ = definite ? SizeDefiniteness::kDefinite
                                      : SizeDefiniteness::kIndefinite;
    }
    return definite;
  }
  return true;
}

bool LayoutFlexibleBox::CrossAxisLengthIsDefinite(const LayoutBox& child,
                                                  const Length& length) const {
  if (length.IsAuto())
    return false;
  if (length.IsPercentOrCalc()) {
    if (HasOrthogonalFlow(child) ||
        has_definite_height_ == SizeDefiniteness::kDefinite)
      return true;
    if (has_definite_height_ == SizeDefiniteness::kIndefinite)
      return false;
    bool definite = child.ComputePercentageLogicalHeight(length) != -1;
    has_definite_height_ =
        definite ? SizeDefiniteness::kDefinite : SizeDefiniteness::kIndefinite;
    return definite;
  }
  // TODO(cbiesinger): Eventually we should support other types of sizes here.
  // Requires updating computeMainSizeFromAspectRatioUsing.
  return length.IsFixed();
}

void LayoutFlexibleBox::CacheChildMainSize(const LayoutBox& child) {
  DCHECK(!child.NeedsLayout());
  LayoutUnit main_size;
  if (HasOrthogonalFlow(child))
    main_size = child.LogicalHeight();
  else
    main_size = child.MaxPreferredLogicalWidth();
  intrinsic_size_along_main_axis_.Set(&child, main_size);
  relaid_out_children_.insert(&child);
}

void LayoutFlexibleBox::ClearCachedMainSizeForChild(const LayoutBox& child) {
  intrinsic_size_along_main_axis_.erase(&child);
}

DISABLE_CFI_PERF
LayoutUnit LayoutFlexibleBox::ComputeInnerFlexBaseSizeForChild(
    LayoutBox& child,
    LayoutUnit main_axis_border_and_padding,
    ChildLayoutType child_layout_type) {
  child.ClearOverrideSize();

  if (child.IsImage() || child.IsVideo() || child.IsCanvas())
    UseCounter::Count(GetDocument(), WebFeature::kAspectRatioFlexItem);

  Length flex_basis = FlexBasisForChild(child);
  if (MainAxisLengthIsDefinite(child, flex_basis))
    return std::max(LayoutUnit(), ComputeMainAxisExtentForChild(
                                      child, kMainOrPreferredSize, flex_basis));

  if (child.StyleRef().ContainsSize())
    return LayoutUnit();

  // The flex basis is indefinite (=auto), so we need to compute the actual
  // width of the child. For the logical width axis we just use the preferred
  // width; for the height we need to lay out the child.
  LayoutUnit main_axis_extent;
  if (HasOrthogonalFlow(child)) {
    if (child_layout_type == kNeverLayout)
      return LayoutUnit();

    UpdateBlockChildDirtyBitsBeforeLayout(child_layout_type == kForceLayout,
                                          child);
    if (child.NeedsLayout() || child_layout_type == kForceLayout ||
        !intrinsic_size_along_main_axis_.Contains(&child)) {
      child.ForceChildLayout();
      CacheChildMainSize(child);
    }
    main_axis_extent = intrinsic_size_along_main_axis_.at(&child);
  } else {
    // We don't need to add scrollbarLogicalWidth here because the preferred
    // width includes the scrollbar, even for overflow: auto.
    main_axis_extent = child.MaxPreferredLogicalWidth();
  }
  DCHECK_GE(main_axis_extent - main_axis_border_and_padding, LayoutUnit())
      << main_axis_extent << " - " << main_axis_border_and_padding;
  return main_axis_extent - main_axis_border_and_padding;
}

void LayoutFlexibleBox::LayoutFlexItems(bool relayout_children,
                                        SubtreeLayoutScope& layout_scope) {
  PaintLayerScrollableArea::PreventRelayoutScope prevent_relayout_scope(
      layout_scope);

  // Set up our master list of flex items. All of the rest of the algorithm
  // should work off this list of a subset.
  // TODO(cbiesinger): That second part is not yet true.
  ChildLayoutType layout_type =
      relayout_children ? kForceLayout : kLayoutIfNeeded;
  Vector<FlexItem> all_items;
  order_iterator_.First();
  for (LayoutBox* child = order_iterator_.CurrentChild(); child;
       child = order_iterator_.Next()) {
    if (child->IsOutOfFlowPositioned()) {
      // Out-of-flow children are not flex items, so we skip them here.
      PrepareChildForPositionedLayout(*child);
      continue;
    }

    all_items.push_back(ConstructFlexItem(*child, layout_type));
  }

  const LayoutUnit line_break_length = MainAxisContentExtent(LayoutUnit::Max());
  FlexLayoutAlgorithm flex_algorithm(Style(), line_break_length, all_items);
  LayoutUnit cross_axis_offset =
      FlowAwareBorderBefore() + FlowAwarePaddingBefore();
  LayoutUnit logical_width = LogicalWidth();
  // TODO(cbiesinger): I don't know why this is necessary but without it we
  // incorrectly lay out vertical+RTL row flexboxes that overflow.
  if (!ShouldPlaceBlockDirectionScrollbarOnLogicalLeft())
    logical_width -= ScrollbarLogicalWidth();
  FlexLine* current_line;
  while ((current_line = flex_algorithm.ComputeNextFlexLine(logical_width))) {
    DCHECK_GE(current_line->line_items.size(), 0ULL);
    current_line->SetContainerMainInnerSize(
        MainAxisContentExtent(current_line->sum_hypothetical_main_size));
    current_line->FreezeInflexibleItems();

    while (!current_line->ResolveFlexibleLengths()) {
      DCHECK_GE(current_line->total_flex_grow, 0);
      DCHECK_GE(current_line->total_weighted_flex_shrink, 0);
    }

    LayoutLineItems(current_line, relayout_children, layout_scope);

    LayoutUnit main_axis_offset =
        FlowAwareBorderStart() + FlowAwarePaddingStart();
    if (Style()->FlexDirection() == EFlexDirection::kRowReverse &&
        ShouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
      main_axis_offset += IsHorizontalFlow() ? VerticalScrollbarWidth()
                                             : HorizontalScrollbarHeight();
    }

    current_line->ComputeLineItemsPosition(main_axis_offset, cross_axis_offset);
    ApplyLineItemsPosition(current_line);
    if (number_of_in_flow_children_on_first_line_ == -1) {
      number_of_in_flow_children_on_first_line_ =
          current_line->line_items.size();
    }
  }
  if (HasLineIfEmpty()) {
    // Even if ComputeNextFlexLine returns true, the flexbox might not have
    // a line because all our children might be out of flow positioned.
    // Instead of just checking if we have a line, make sure the flexbox
    // has at least a line's worth of height to cover this case.
    LayoutUnit min_height = MinimumLogicalHeightForEmptyLine();
    if (Size().Height() < min_height)
      SetLogicalHeight(min_height);
  }

  UpdateLogicalHeight();
  RepositionLogicalHeightDependentFlexItems(flex_algorithm.FlexLines());
}

bool LayoutFlexibleBox::HasAutoMarginsInCrossAxis(
    const LayoutBox& child) const {
  if (IsHorizontalFlow())
    return child.Style()->MarginTop().IsAuto() ||
           child.Style()->MarginBottom().IsAuto();
  return child.Style()->MarginLeft().IsAuto() ||
         child.Style()->MarginRight().IsAuto();
}

bool LayoutFlexibleBox::UpdateAutoMarginsInCrossAxis(
    LayoutBox& child,
    LayoutUnit available_alignment_space) {
  DCHECK(!child.IsOutOfFlowPositioned());
  DCHECK_GE(available_alignment_space, LayoutUnit());

  bool is_horizontal = IsHorizontalFlow();
  Length top_or_left =
      is_horizontal ? child.Style()->MarginTop() : child.Style()->MarginLeft();
  Length bottom_or_right = is_horizontal ? child.Style()->MarginBottom()
                                         : child.Style()->MarginRight();
  if (top_or_left.IsAuto() && bottom_or_right.IsAuto()) {
    AdjustAlignmentForChild(child, available_alignment_space / 2);
    if (is_horizontal) {
      child.SetMarginTop(available_alignment_space / 2);
      child.SetMarginBottom(available_alignment_space / 2);
    } else {
      child.SetMarginLeft(available_alignment_space / 2);
      child.SetMarginRight(available_alignment_space / 2);
    }
    return true;
  }
  bool should_adjust_top_or_left = true;
  if (IsColumnFlow() && !child.Style()->IsLeftToRightDirection()) {
    // For column flows, only make this adjustment if topOrLeft corresponds to
    // the "before" margin, so that flipForRightToLeftColumn will do the right
    // thing.
    should_adjust_top_or_left = false;
  }
  if (!IsColumnFlow() && child.Style()->IsFlippedBlocksWritingMode()) {
    // If we are a flipped writing mode, we need to adjust the opposite side.
    // This is only needed for row flows because this only affects the
    // block-direction axis.
    should_adjust_top_or_left = false;
  }

  if (top_or_left.IsAuto()) {
    if (should_adjust_top_or_left)
      AdjustAlignmentForChild(child, available_alignment_space);

    if (is_horizontal)
      child.SetMarginTop(available_alignment_space);
    else
      child.SetMarginLeft(available_alignment_space);
    return true;
  }
  if (bottom_or_right.IsAuto()) {
    if (!should_adjust_top_or_left)
      AdjustAlignmentForChild(child, available_alignment_space);

    if (is_horizontal)
      child.SetMarginBottom(available_alignment_space);
    else
      child.SetMarginRight(available_alignment_space);
    return true;
  }
  return false;
}

LayoutUnit LayoutFlexibleBox::ComputeChildMarginValue(Length margin) {
  // When resolving the margins, we use the content size for resolving percent
  // and calc (for percents in calc expressions) margins. Fortunately, percent
  // margins are always computed with respect to the block's width, even for
  // margin-top and margin-bottom.
  LayoutUnit available_size = ContentLogicalWidth();
  return MinimumValueForLength(margin, available_size);
}

void LayoutFlexibleBox::PrepareOrderIteratorAndMargins() {
  OrderIteratorPopulator populator(order_iterator_);

  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    populator.CollectChild(child);

    if (child->IsOutOfFlowPositioned())
      continue;

    // Before running the flex algorithm, 'auto' has a margin of 0.
    // Also, if we're not auto sizing, we don't do a layout that computes the
    // start/end margins.
    if (IsHorizontalFlow()) {
      child->SetMarginLeft(
          ComputeChildMarginValue(child->Style()->MarginLeft()));
      child->SetMarginRight(
          ComputeChildMarginValue(child->Style()->MarginRight()));
    } else {
      child->SetMarginTop(ComputeChildMarginValue(child->Style()->MarginTop()));
      child->SetMarginBottom(
          ComputeChildMarginValue(child->Style()->MarginBottom()));
    }
  }
}

DISABLE_CFI_PERF
MinMaxSize LayoutFlexibleBox::ComputeMinAndMaxSizesForChild(
    const LayoutBox& child) const {
  MinMaxSize sizes{LayoutUnit(), LayoutUnit::Max()};

  Length max = IsHorizontalFlow() ? child.Style()->MaxWidth()
                                  : child.Style()->MaxHeight();
  if (max.IsSpecifiedOrIntrinsic()) {
    sizes.max_size = ComputeMainAxisExtentForChild(child, kMaxSize, max);
    if (sizes.max_size == -1)
      sizes.max_size = LayoutUnit::Max();
    DCHECK_GE(sizes.max_size, LayoutUnit());
  }

  Length min = IsHorizontalFlow() ? child.Style()->MinWidth()
                                  : child.Style()->MinHeight();
  if (min.IsSpecifiedOrIntrinsic()) {
    sizes.min_size = ComputeMainAxisExtentForChild(child, kMinSize, min);
    // computeMainAxisExtentForChild can return -1 when the child has a
    // percentage min size, but we have an indefinite size in that axis.
    sizes.min_size = std::max(LayoutUnit(), sizes.min_size);
  } else if (min.IsAuto() && !child.StyleRef().ContainsSize() &&
             MainAxisOverflowForChild(child) == EOverflow::kVisible &&
             !(IsColumnFlow() && child.IsFlexibleBox())) {
    // TODO(cbiesinger): For now, we do not handle min-height: auto for nested
    // column flexboxes. We need to implement
    // https://drafts.csswg.org/css-flexbox/#intrinsic-sizes before that
    // produces reasonable results. Tracking bug: https://crbug.com/581553
    // css-flexbox section 4.5
    LayoutUnit content_size =
        ComputeMainAxisExtentForChild(child, kMinSize, Length(kMinContent));
    DCHECK_GE(content_size, LayoutUnit());
    if (HasAspectRatio(child) && child.IntrinsicSize().Height() > 0)
      content_size =
          AdjustChildSizeForAspectRatioCrossAxisMinAndMax(child, content_size);
    if (child.IsTable() && !IsColumnFlow()) {
      // Avoid resolving minimum size to something narrower than the minimum
      // preferred logical width of the table.
      sizes.min_size = content_size;
    } else {
      if (sizes.max_size != -1 && content_size > sizes.max_size)
        content_size = sizes.max_size;

      Length main_size = IsHorizontalFlow() ? child.StyleRef().Width()
                                            : child.StyleRef().Height();
      if (MainAxisLengthIsDefinite(child, main_size)) {
        LayoutUnit resolved_main_size = ComputeMainAxisExtentForChild(
            child, kMainOrPreferredSize, main_size);
        DCHECK_GE(resolved_main_size, LayoutUnit());
        LayoutUnit specified_size =
            sizes.max_size != -1 ? std::min(resolved_main_size, sizes.max_size)
                                 : resolved_main_size;

        sizes.min_size = std::min(specified_size, content_size);
      } else if (UseChildAspectRatio(child)) {
        Length cross_size_length = IsHorizontalFlow()
                                       ? child.StyleRef().Height()
                                       : child.StyleRef().Width();
        LayoutUnit transferred_size =
            ComputeMainSizeFromAspectRatioUsing(child, cross_size_length);
        transferred_size = AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
            child, transferred_size);
        sizes.min_size = std::min(transferred_size, content_size);
      } else {
        sizes.min_size = content_size;
      }
    }
  }
  DCHECK_GE(sizes.min_size, LayoutUnit());
  return sizes;
}

LayoutUnit LayoutFlexibleBox::CrossSizeForPercentageResolution(
    const LayoutBox& child) {
  if (FlexLayoutAlgorithm::AlignmentForChild(StyleRef(), child.StyleRef()) !=
      ItemPosition::kStretch)
    return LayoutUnit(-1);

  // Here we implement https://drafts.csswg.org/css-flexbox/#algo-stretch
  if (HasOrthogonalFlow(child) && child.HasOverrideLogicalContentWidth())
    return child.OverrideLogicalContentWidth() - child.ScrollbarLogicalWidth();
  if (!HasOrthogonalFlow(child) && child.HasOverrideLogicalContentHeight()) {
    return child.OverrideLogicalContentHeight() -
           child.ScrollbarLogicalHeight();
  }

  // We don't currently implement the optimization from
  // https://drafts.csswg.org/css-flexbox/#definite-sizes case 1. While that
  // could speed up a specialized case, it requires determining if we have a
  // definite size, which itself is not cheap. We can consider implementing it
  // at a later time. (The correctness is ensured by redoing layout in
  // applyStretchAlignmentToChild)
  return LayoutUnit(-1);
}

LayoutUnit LayoutFlexibleBox::MainSizeForPercentageResolution(
    const LayoutBox& child) {
  // This function implements section 9.8. Definite and Indefinite Sizes, case
  // 2) of the flexbox spec.
  // We need to check for the flexbox to have a definite main size, and for the
  // flex item to have a definite flex basis.
  const Length& flex_basis = FlexBasisForChild(child);
  if (!MainAxisLengthIsDefinite(child, flex_basis))
    return LayoutUnit(-1);
  if (!flex_basis.IsPercentOrCalc()) {
    // If flex basis had a percentage, our size is guaranteed to be definite or
    // the flex item's size could not be definite. Otherwise, we make up a
    // percentage to check whether we have a definite size.
    if (!MainAxisLengthIsDefinite(child, Length(0, kPercent)))
      return LayoutUnit(-1);
  }

  if (HasOrthogonalFlow(child))
    return child.HasOverrideLogicalContentHeight()
               ? child.OverrideLogicalContentHeight() -
                     child.ScrollbarLogicalHeight()
               : LayoutUnit(-1);
  return child.HasOverrideLogicalContentWidth()
             ? child.OverrideLogicalContentWidth() -
                   child.ScrollbarLogicalWidth()
             : LayoutUnit(-1);
}

LayoutUnit LayoutFlexibleBox::ChildLogicalHeightForPercentageResolution(
    const LayoutBox& child) {
  if (!HasOrthogonalFlow(child))
    return CrossSizeForPercentageResolution(child);
  return MainSizeForPercentageResolution(child);
}

LayoutUnit LayoutFlexibleBox::AdjustChildSizeForAspectRatioCrossAxisMinAndMax(
    const LayoutBox& child,
    LayoutUnit child_size) const {
  Length cross_min = IsHorizontalFlow() ? child.Style()->MinHeight()
                                        : child.Style()->MinWidth();
  Length cross_max = IsHorizontalFlow() ? child.Style()->MaxHeight()
                                        : child.Style()->MaxWidth();

  if (CrossAxisLengthIsDefinite(child, cross_max)) {
    LayoutUnit max_value =
        ComputeMainSizeFromAspectRatioUsing(child, cross_max);
    child_size = std::min(max_value, child_size);
  }

  if (CrossAxisLengthIsDefinite(child, cross_min)) {
    LayoutUnit min_value =
        ComputeMainSizeFromAspectRatioUsing(child, cross_min);
    child_size = std::max(min_value, child_size);
  }

  return child_size;
}

DISABLE_CFI_PERF
FlexItem LayoutFlexibleBox::ConstructFlexItem(LayoutBox& child,
                                              ChildLayoutType layout_type) {
  if (layout_type != kNeverLayout && ChildHasIntrinsicMainAxisSize(child)) {
    // If this condition is true, then ComputeMainAxisExtentForChild will call
    // child.IntrinsicContentLogicalHeight() and
    // child.ScrollbarLogicalHeight(), so if the child has intrinsic
    // min/max/preferred size, run layout on it now to make sure its logical
    // height and scroll bars are up to date.
    // For column flow flex containers, we even need to do this for children
    // that don't need layout, if there's a chance that the logical width of
    // the flex container has changed (because that may affect the intrinsic
    // height of the child).
    if (child.NeedsLayout() ||
        (IsColumnFlow() && layout_type == kForceLayout)) {
      child.ClearOverrideSize();
      child.ForceChildLayout();
      CacheChildMainSize(child);
      layout_type = kLayoutIfNeeded;
    }
  }

  MinMaxSize sizes = ComputeMinAndMaxSizesForChild(child);

  LayoutUnit border_and_padding = IsHorizontalFlow()
                                      ? child.BorderAndPaddingWidth()
                                      : child.BorderAndPaddingHeight();
  LayoutUnit child_inner_flex_base_size =
      ComputeInnerFlexBaseSizeForChild(child, border_and_padding, layout_type);
  LayoutUnit margin =
      IsHorizontalFlow() ? child.MarginWidth() : child.MarginHeight();
  return FlexItem(&child, child_inner_flex_base_size, sizes, border_and_padding,
                  margin);
}

static LayoutUnit AlignmentOffset(LayoutUnit available_free_space,
                                  ItemPosition position,
                                  LayoutUnit ascent,
                                  LayoutUnit max_ascent,
                                  bool is_wrap_reverse) {
  switch (position) {
    case ItemPosition::kLegacy:
    case ItemPosition::kAuto:
    case ItemPosition::kNormal:
      NOTREACHED();
      break;
    case ItemPosition::kStretch:
      // Actual stretching must be handled by the caller. Since wrap-reverse
      // flips cross start and cross end, stretch children should be aligned
      // with the cross end. This matters because applyStretchAlignment
      // doesn't always stretch or stretch fully (explicit cross size given, or
      // stretching constrained by max-height/max-width). For flex-start and
      // flex-end this is handled by alignmentForChild().
      if (is_wrap_reverse)
        return available_free_space;
      break;
    case ItemPosition::kFlexStart:
      break;
    case ItemPosition::kFlexEnd:
      return available_free_space;
    case ItemPosition::kCenter:
      return available_free_space / 2;
    case ItemPosition::kBaseline:
      // FIXME: If we get here in columns, we want the use the descent, except
      // we currently can't get the ascent/descent of orthogonal children.
      // https://bugs.webkit.org/show_bug.cgi?id=98076
      return max_ascent - ascent;
    case ItemPosition::kLastBaseline:
    case ItemPosition::kSelfStart:
    case ItemPosition::kSelfEnd:
    case ItemPosition::kStart:
    case ItemPosition::kEnd:
    case ItemPosition::kLeft:
    case ItemPosition::kRight:
      // TODO(jferanndez): Implement these (https://crbug.com/722287).
      break;
  }
  return LayoutUnit();
}

void LayoutFlexibleBox::SetOverrideMainAxisContentSizeForChild(
    LayoutBox& child,
    LayoutUnit child_preferred_size) {
  if (HasOrthogonalFlow(child))
    child.SetOverrideLogicalContentHeight(child_preferred_size);
  else
    child.SetOverrideLogicalContentWidth(child_preferred_size);
}

LayoutUnit LayoutFlexibleBox::StaticMainAxisPositionForPositionedChild(
    const LayoutBox& child) {
  const LayoutUnit available_space =
      MainAxisContentExtent(ContentLogicalHeight()) -
      MainAxisExtentForChild(child);

  LayoutUnit offset = FlexLayoutAlgorithm::InitialContentPositionOffset(
      available_space, FlexLayoutAlgorithm::ResolvedJustifyContent(StyleRef()),
      1);
  if (StyleRef().FlexDirection() == EFlexDirection::kRowReverse ||
      StyleRef().FlexDirection() == EFlexDirection::kColumnReverse)
    offset = available_space - offset;
  return offset;
}

LayoutUnit LayoutFlexibleBox::StaticCrossAxisPositionForPositionedChild(
    const LayoutBox& child) {
  LayoutUnit available_space =
      CrossAxisContentExtent() - CrossAxisExtentForChild(child);
  return AlignmentOffset(
      available_space,
      FlexLayoutAlgorithm::AlignmentForChild(StyleRef(), child.StyleRef()),
      LayoutUnit(), LayoutUnit(),
      StyleRef().FlexWrap() == EFlexWrap::kWrapReverse);
}

LayoutUnit LayoutFlexibleBox::StaticInlinePositionForPositionedChild(
    const LayoutBox& child) {
  return StartOffsetForContent() +
         (IsColumnFlow() ? StaticCrossAxisPositionForPositionedChild(child)
                         : StaticMainAxisPositionForPositionedChild(child));
}

LayoutUnit LayoutFlexibleBox::StaticBlockPositionForPositionedChild(
    const LayoutBox& child) {
  return BorderAndPaddingBefore() +
         (IsColumnFlow() ? StaticMainAxisPositionForPositionedChild(child)
                         : StaticCrossAxisPositionForPositionedChild(child));
}

bool LayoutFlexibleBox::SetStaticPositionForPositionedLayout(LayoutBox& child) {
  bool position_changed = false;
  PaintLayer* child_layer = child.Layer();
  if (child.StyleRef().HasStaticInlinePosition(
          StyleRef().IsHorizontalWritingMode())) {
    LayoutUnit inline_position = StaticInlinePositionForPositionedChild(child);
    if (child_layer->StaticInlinePosition() != inline_position) {
      child_layer->SetStaticInlinePosition(inline_position);
      position_changed = true;
    }
  }
  if (child.StyleRef().HasStaticBlockPosition(
          StyleRef().IsHorizontalWritingMode())) {
    LayoutUnit block_position = StaticBlockPositionForPositionedChild(child);
    if (child_layer->StaticBlockPosition() != block_position) {
      child_layer->SetStaticBlockPosition(block_position);
      position_changed = true;
    }
  }
  return position_changed;
}

void LayoutFlexibleBox::PrepareChildForPositionedLayout(LayoutBox& child) {
  DCHECK(child.IsOutOfFlowPositioned());
  child.ContainingBlock()->InsertPositionedObject(&child);
  PaintLayer* child_layer = child.Layer();
  LayoutUnit static_inline_position =
      FlowAwareBorderStart() + FlowAwarePaddingStart();
  if (child_layer->StaticInlinePosition() != static_inline_position) {
    child_layer->SetStaticInlinePosition(static_inline_position);
    if (child.Style()->HasStaticInlinePosition(
            Style()->IsHorizontalWritingMode()))
      child.SetChildNeedsLayout(kMarkOnlyThis);
  }

  LayoutUnit static_block_position =
      FlowAwareBorderBefore() + FlowAwarePaddingBefore();
  if (child_layer->StaticBlockPosition() != static_block_position) {
    child_layer->SetStaticBlockPosition(static_block_position);
    if (child.Style()->HasStaticBlockPosition(
            Style()->IsHorizontalWritingMode()))
      child.SetChildNeedsLayout(kMarkOnlyThis);
  }
}

void LayoutFlexibleBox::ResetAutoMarginsAndLogicalTopInCrossAxis(
    LayoutBox& child) {
  if (HasAutoMarginsInCrossAxis(child)) {
    child.UpdateLogicalHeight();
    if (IsHorizontalFlow()) {
      if (child.Style()->MarginTop().IsAuto())
        child.SetMarginTop(LayoutUnit());
      if (child.Style()->MarginBottom().IsAuto())
        child.SetMarginBottom(LayoutUnit());
    } else {
      if (child.Style()->MarginLeft().IsAuto())
        child.SetMarginLeft(LayoutUnit());
      if (child.Style()->MarginRight().IsAuto())
        child.SetMarginRight(LayoutUnit());
    }
  }
}

bool LayoutFlexibleBox::NeedToStretchChildLogicalHeight(
    const LayoutBox& child) const {
  // This function is a little bit magical. It relies on the fact that blocks
  // intrinsically "stretch" themselves in their inline axis, i.e. a <div> has
  // an implicit width: 100%. So the child will automatically stretch if our
  // cross axis is the child's inline axis. That's the case if:
  // - We are horizontal and the child is in vertical writing mode
  // - We are vertical and the child is in horizontal writing mode
  // Otherwise, we need to stretch if the cross axis size is auto.
  if (FlexLayoutAlgorithm::AlignmentForChild(StyleRef(), child.StyleRef()) !=
      ItemPosition::kStretch)
    return false;

  if (IsHorizontalFlow() != child.StyleRef().IsHorizontalWritingMode())
    return false;

  return child.StyleRef().LogicalHeight().IsAuto();
}

bool LayoutFlexibleBox::ChildHasIntrinsicMainAxisSize(
    const LayoutBox& child) const {
  bool result = false;
  if (IsHorizontalFlow() != child.StyleRef().IsHorizontalWritingMode()) {
    Length child_flex_basis = FlexBasisForChild(child);
    Length child_min_size = IsHorizontalFlow() ? child.Style()->MinWidth()
                                               : child.Style()->MinHeight();
    Length child_max_size = IsHorizontalFlow() ? child.Style()->MaxWidth()
                                               : child.Style()->MaxHeight();
    if (child_flex_basis.IsIntrinsic() || child_min_size.IsIntrinsicOrAuto() ||
        child_max_size.IsIntrinsic())
      result = true;
  }
  return result;
}

EOverflow LayoutFlexibleBox::MainAxisOverflowForChild(
    const LayoutBox& child) const {
  if (IsHorizontalFlow())
    return child.StyleRef().OverflowX();
  return child.StyleRef().OverflowY();
}

EOverflow LayoutFlexibleBox::CrossAxisOverflowForChild(
    const LayoutBox& child) const {
  if (IsHorizontalFlow())
    return child.StyleRef().OverflowY();
  return child.StyleRef().OverflowX();
}

DISABLE_CFI_PERF
void LayoutFlexibleBox::LayoutLineItems(FlexLine* current_line,
                                        bool relayout_children,
                                        SubtreeLayoutScope& layout_scope) {
  for (size_t i = 0; i < current_line->line_items.size(); ++i) {
    FlexItem& flex_item = current_line->line_items[i];
    LayoutBox* child = flex_item.box;

    DCHECK(!flex_item.box->IsOutOfFlowPositioned());

    child->SetMayNeedPaintInvalidation();

    SetOverrideMainAxisContentSizeForChild(*child,
                                           flex_item.flexed_content_size);
    // The flexed content size and the override size include the scrollbar
    // width, so we need to compare to the size including the scrollbar.
    // TODO(cbiesinger): Should it include the scrollbar?
    if (flex_item.flexed_content_size !=
        MainAxisContentExtentForChildIncludingScrollbar(*child)) {
      child->SetChildNeedsLayout(kMarkOnlyThis);
    } else {
      // To avoid double applying margin changes in
      // updateAutoMarginsInCrossAxis, we reset the margins here.
      ResetAutoMarginsAndLogicalTopInCrossAxis(*child);
    }
    // We may have already forced relayout for orthogonal flowing children in
    // computeInnerFlexBaseSizeForChild.
    bool force_child_relayout =
        relayout_children && !relaid_out_children_.Contains(child);
    if (child->IsLayoutBlock() &&
        ToLayoutBlock(*child).HasPercentHeightDescendants()) {
      // Have to force another relayout even though the child is sized
      // correctly, because its descendants are not sized correctly yet. Our
      // previous layout of the child was done without an override height set.
      // So, redo it here.
      force_child_relayout = true;
    }
    UpdateBlockChildDirtyBitsBeforeLayout(force_child_relayout, *child);
    if (!child->NeedsLayout())
      MarkChildForPaginationRelayoutIfNeeded(*child, layout_scope);
    if (child->NeedsLayout())
      relaid_out_children_.insert(child);
    child->LayoutIfNeeded();

    // This shouldn't be necessary, because we set the override size to be
    // the flexed_content_size and so the result should in fact be that size.
    // But it turns out that tables ignore the override size, and so we have
    // to re-check the size so that we place the flex item correctly.
    flex_item.flexed_content_size =
        MainAxisExtentForChild(*child) - flex_item.main_axis_border_and_padding;
    flex_item.cross_axis_size = CrossAxisExtentForChild(*child);
    flex_item.cross_axis_intrinsic_size =
        CrossAxisIntrinsicExtentForChild(*child);
  }
}

void LayoutFlexibleBox::ApplyLineItemsPosition(FlexLine* current_line) {
  bool is_paginated = View()->GetLayoutState()->IsPaginated();
  for (size_t i = 0; i < current_line->line_items.size(); ++i) {
    const FlexItem& flex_item = current_line->line_items[i];
    LayoutBox* child = flex_item.box;
    SetFlowAwareLocationForChild(*child, flex_item.desired_location);

    if (is_paginated)
      UpdateFragmentationInfoForChild(*child);
  }

  if (IsColumnFlow()) {
    SetLogicalHeight(std::max(
        LogicalHeight(), current_line->main_axis_extent + FlowAwareBorderEnd() +
                             FlowAwarePaddingEnd() + ScrollbarLogicalHeight()));
  } else {
    SetLogicalHeight(std::max(
        LogicalHeight(), current_line->cross_axis_offset +
                             FlowAwareBorderAfter() + FlowAwarePaddingAfter() +
                             current_line->cross_axis_extent +
                             CrossAxisScrollbarExtent()));
  }

  if (Style()->FlexDirection() == EFlexDirection::kColumnReverse) {
    // We have to do an extra pass for column-reverse to reposition the flex
    // items since the start depends on the height of the flexbox, which we
    // only know after we've positioned all the flex items.
    UpdateLogicalHeight();
    LayoutColumnReverse(current_line->line_items,
                        current_line->cross_axis_offset,
                        current_line->remaining_free_space);
  }
}

void LayoutFlexibleBox::LayoutColumnReverse(Vector<FlexItem>& children,
                                            LayoutUnit cross_axis_offset,
                                            LayoutUnit available_free_space) {
  const StyleContentAlignmentData justify_content =
      FlexLayoutAlgorithm::ResolvedJustifyContent(StyleRef());

  // This is similar to the logic in layoutAndPlaceChildren, except we place
  // the children starting from the end of the flexbox. We also don't need to
  // layout anything since we're just moving the children to a new position.
  LayoutUnit main_axis_offset =
      LogicalHeight() - FlowAwareBorderEnd() - FlowAwarePaddingEnd();
  main_axis_offset -= FlexLayoutAlgorithm::InitialContentPositionOffset(
      available_free_space, justify_content, children.size());
  main_axis_offset -= IsHorizontalFlow() ? VerticalScrollbarWidth()
                                         : HorizontalScrollbarHeight();

  for (size_t i = 0; i < children.size(); ++i) {
    FlexItem& flex_item = children[i];
    LayoutBox* child = flex_item.box;

    DCHECK(!child->IsOutOfFlowPositioned());

    main_axis_offset -=
        MainAxisExtentForChild(*child) + flex_item.FlowAwareMarginEnd();

    SetFlowAwareLocationForChild(
        *child,
        LayoutPoint(main_axis_offset,
                    cross_axis_offset + flex_item.FlowAwareMarginBefore()));

    main_axis_offset -= flex_item.FlowAwareMarginStart();

    main_axis_offset -=
        FlexLayoutAlgorithm::ContentDistributionSpaceBetweenChildren(
            available_free_space, justify_content, children.size());
  }
}

void LayoutFlexibleBox::AlignFlexLines(Vector<FlexLine>& line_contexts) {
  const StyleContentAlignmentData align_content =
      FlexLayoutAlgorithm::ResolvedAlignContent(StyleRef());

  // If we have a single line flexbox or a multiline line flexbox with only one
  // flex line, the line height is all the available space. For
  // flex-direction: row, this means we need to use the height, so we do this
  // after calling updateLogicalHeight.
  if (line_contexts.size() == 1) {
    line_contexts[0].cross_axis_extent = CrossAxisContentExtent();
    return;
  }

  if (align_content.GetPosition() == ContentPosition::kFlexStart)
    return;

  LayoutUnit available_cross_axis_space = CrossAxisContentExtent();
  for (size_t i = 0; i < line_contexts.size(); ++i)
    available_cross_axis_space -= line_contexts[i].cross_axis_extent;

  LayoutUnit line_offset;
  if (line_contexts.size() > 1) {
    line_offset = FlexLayoutAlgorithm::InitialContentPositionOffset(
        available_cross_axis_space, align_content, line_contexts.size());
  }
  for (unsigned line_number = 0; line_number < line_contexts.size();
       ++line_number) {
    FlexLine& line_context = line_contexts[line_number];
    line_context.cross_axis_offset += line_offset;
    for (size_t child_number = 0; child_number < line_context.line_items.size();
         ++child_number) {
      FlexItem& flex_item = line_context.line_items[child_number];
      AdjustAlignmentForChild(*flex_item.box, line_offset);
    }

    if (align_content.Distribution() == ContentDistributionType::kStretch &&
        available_cross_axis_space > 0)
      line_contexts[line_number].cross_axis_extent +=
          available_cross_axis_space /
          static_cast<unsigned>(line_contexts.size());

    line_offset += FlexLayoutAlgorithm::ContentDistributionSpaceBetweenChildren(
        available_cross_axis_space, align_content, line_contexts.size());
  }
}

void LayoutFlexibleBox::AdjustAlignmentForChild(LayoutBox& child,
                                                LayoutUnit delta) {
  DCHECK(!child.IsOutOfFlowPositioned());

  SetFlowAwareLocationForChild(child, FlowAwareLocationForChild(child) +
                                          LayoutSize(LayoutUnit(), delta));
}

void LayoutFlexibleBox::AlignChildren(Vector<FlexLine>& line_contexts) {
  // Keep track of the space between the baseline edge and the after edge of
  // the box for each line.
  Vector<LayoutUnit> min_margin_after_baselines;

  for (size_t line_number = 0; line_number < line_contexts.size();
       ++line_number) {
    FlexLine& line_context = line_contexts[line_number];

    LayoutUnit min_margin_after_baseline = LayoutUnit::Max();
    LayoutUnit line_cross_axis_extent = line_context.cross_axis_extent;
    LayoutUnit max_ascent = line_context.max_ascent;

    for (size_t child_number = 0; child_number < line_context.line_items.size();
         ++child_number) {
      FlexItem& flex_item = line_context.line_items[child_number];
      DCHECK(!flex_item.box->IsOutOfFlowPositioned());

      if (UpdateAutoMarginsInCrossAxis(
              *flex_item.box,
              std::max(LayoutUnit(), flex_item.AvailableAlignmentSpace(
                                         line_cross_axis_extent))))
        continue;

      ItemPosition position = flex_item.Alignment();
      if (position == ItemPosition::kStretch)
        ApplyStretchAlignmentToChild(flex_item, line_cross_axis_extent);
      LayoutUnit available_space =
          flex_item.AvailableAlignmentSpace(line_cross_axis_extent);
      LayoutUnit offset = AlignmentOffset(
          available_space, position, flex_item.MarginBoxAscent(), max_ascent,
          StyleRef().FlexWrap() == EFlexWrap::kWrapReverse);
      AdjustAlignmentForChild(*flex_item.box, offset);
      if (position == ItemPosition::kBaseline &&
          StyleRef().FlexWrap() == EFlexWrap::kWrapReverse) {
        min_margin_after_baseline = std::min(
            min_margin_after_baseline,
            flex_item.AvailableAlignmentSpace(line_cross_axis_extent) - offset);
      }
    }
    min_margin_after_baselines.push_back(min_margin_after_baseline);
  }

  if (Style()->FlexWrap() != EFlexWrap::kWrapReverse)
    return;

  // wrap-reverse flips the cross axis start and end. For baseline alignment,
  // this means we need to align the after edge of baseline elements with the
  // after edge of the flex line.
  for (size_t line_number = 0; line_number < line_contexts.size();
       ++line_number) {
    const FlexLine& line_context = line_contexts[line_number];
    LayoutUnit min_margin_after_baseline =
        min_margin_after_baselines[line_number];
    for (size_t child_number = 0; child_number < line_context.line_items.size();
         ++child_number) {
      const FlexItem& flex_item = line_context.line_items[child_number];
      if (flex_item.Alignment() == ItemPosition::kBaseline &&
          !flex_item.HasAutoMarginsInCrossAxis() && min_margin_after_baseline)
        AdjustAlignmentForChild(*flex_item.box, min_margin_after_baseline);
    }
  }
}

void LayoutFlexibleBox::ApplyStretchAlignmentToChild(
    FlexItem& flex_item,
    LayoutUnit line_cross_axis_extent) {
  LayoutBox& child = *flex_item.box;
  if (!flex_item.HasOrthogonalFlow() &&
      child.Style()->LogicalHeight().IsAuto()) {
    LayoutUnit stretched_logical_height =
        std::max(child.BorderAndPaddingLogicalHeight(),
                 line_cross_axis_extent - flex_item.CrossAxisMarginExtent());
    DCHECK(!child.NeedsLayout());
    LayoutUnit desired_logical_height = child.ConstrainLogicalHeightByMinMax(
        stretched_logical_height, child.IntrinsicContentLogicalHeight());
    flex_item.cross_axis_size = desired_logical_height;

    // FIXME: Can avoid laying out here in some cases. See
    // https://webkit.org/b/87905.
    bool child_needs_relayout = desired_logical_height != child.LogicalHeight();
    if (child.IsLayoutBlock() &&
        ToLayoutBlock(child).HasPercentHeightDescendants() &&
        relaid_out_children_.Contains(&child)) {
      // Have to force another relayout even though the child is sized
      // correctly, because its descendants are not sized correctly yet. Our
      // previous layout of the child was done without an override height set.
      // So, redo it here.
      child_needs_relayout = true;
    }
    if (child_needs_relayout || !child.HasOverrideLogicalContentHeight())
      child.SetOverrideLogicalContentHeight(
          desired_logical_height - child.BorderAndPaddingLogicalHeight());
    if (child_needs_relayout) {
      child.SetLogicalHeight(LayoutUnit());
      // We cache the child's intrinsic content logical height to avoid it being
      // reset to the stretched height.
      // FIXME: This is fragile. LayoutBoxes should be smart enough to
      // determine their intrinsic content logical height correctly even when
      // there's an overrideHeight.
      LayoutUnit child_intrinsic_content_logical_height =
          child.IntrinsicContentLogicalHeight();
      child.ForceChildLayout();
      child.SetIntrinsicContentLogicalHeight(
          child_intrinsic_content_logical_height);
    }
  } else if (flex_item.HasOrthogonalFlow() &&
             child.Style()->LogicalWidth().IsAuto()) {
    LayoutUnit child_width =
        (line_cross_axis_extent - flex_item.CrossAxisMarginExtent())
            .ClampNegativeToZero();
    child_width = child.ConstrainLogicalWidthByMinMax(
        child_width, CrossAxisContentExtent(), this);
    flex_item.cross_axis_size = child_width;

    if (child_width != child.LogicalWidth()) {
      child.SetOverrideLogicalContentWidth(
          child_width - child.BorderAndPaddingLogicalWidth());
      child.ForceChildLayout();
    }
  }
}

void LayoutFlexibleBox::FlipForRightToLeftColumn(
    const Vector<FlexLine>& line_contexts) {
  if (Style()->IsLeftToRightDirection() || !IsColumnFlow())
    return;

  LayoutUnit cross_extent = CrossAxisExtent();
  for (size_t line_number = 0; line_number < line_contexts.size();
       ++line_number) {
    const FlexLine& line_context = line_contexts[line_number];
    for (size_t child_number = 0; child_number < line_context.line_items.size();
         ++child_number) {
      const FlexItem& flex_item = line_context.line_items[child_number];
      DCHECK(!flex_item.box->IsOutOfFlowPositioned());

      LayoutPoint location = FlowAwareLocationForChild(*flex_item.box);
      // For vertical flows, setFlowAwareLocationForChild will transpose x and
      // y,
      // so using the y axis for a column cross axis extent is correct.
      location.SetY(cross_extent - flex_item.cross_axis_size - location.Y());
      if (!IsHorizontalWritingMode())
        location.Move(LayoutSize(0, -HorizontalScrollbarHeight()));
      SetFlowAwareLocationForChild(*flex_item.box, location);
    }
  }
}

void LayoutFlexibleBox::FlipForWrapReverse(
    const Vector<FlexLine>& line_contexts,
    LayoutUnit cross_axis_start_edge) {
  LayoutUnit content_extent = CrossAxisContentExtent();
  for (size_t line_number = 0; line_number < line_contexts.size();
       ++line_number) {
    const FlexLine& line_context = line_contexts[line_number];
    for (size_t child_number = 0; child_number < line_context.line_items.size();
         ++child_number) {
      const FlexItem& flex_item = line_context.line_items[child_number];
      LayoutUnit line_cross_axis_extent =
          line_contexts[line_number].cross_axis_extent;
      LayoutUnit original_offset =
          line_contexts[line_number].cross_axis_offset - cross_axis_start_edge;
      LayoutUnit new_offset =
          content_extent - original_offset - line_cross_axis_extent;
      AdjustAlignmentForChild(*flex_item.box, new_offset - original_offset);
    }
  }
}

}  // namespace blink
