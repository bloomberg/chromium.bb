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

#include "core/layout/FlexibleBoxAlgorithm.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/MinMaxSize.h"

namespace blink {

FlexItem::FlexItem(LayoutBox* box,
                   LayoutUnit flex_base_content_size,
                   MinMaxSize min_max_sizes,
                   LayoutUnit main_axis_border_and_padding,
                   LayoutUnit main_axis_margin)
    : box(box),
      flex_base_content_size(flex_base_content_size),
      min_max_sizes(min_max_sizes),
      hypothetical_main_content_size(
          min_max_sizes.ClampSizeToMinAndMax(flex_base_content_size)),
      main_axis_border_and_padding(main_axis_border_and_padding),
      main_axis_margin(main_axis_margin),
      frozen(false) {
  DCHECK(!box->IsOutOfFlowPositioned());
  DCHECK_GE(min_max_sizes.max_size, LayoutUnit())
      << "Use LayoutUnit::Max() for no max size";
}

bool FlexItem::HasOrthogonalFlow() const {
  return algorithm->IsHorizontalFlow() != box->IsHorizontalWritingMode();
}

LayoutUnit FlexItem::FlowAwareMarginStart() const {
  if (algorithm->IsHorizontalFlow()) {
    return algorithm->IsLeftToRightFlow() ? box->MarginLeft()
                                          : box->MarginRight();
  }
  return algorithm->IsLeftToRightFlow() ? box->MarginTop()
                                        : box->MarginBottom();
}

LayoutUnit FlexItem::FlowAwareMarginEnd() const {
  if (algorithm->IsHorizontalFlow()) {
    return algorithm->IsLeftToRightFlow() ? box->MarginRight()
                                          : box->MarginLeft();
  }
  return algorithm->IsLeftToRightFlow() ? box->MarginBottom()
                                        : box->MarginTop();
}

LayoutUnit FlexItem::FlowAwareMarginBefore() const {
  switch (algorithm->GetTransformedWritingMode()) {
    case TransformedWritingMode::kTopToBottomWritingMode:
      return box->MarginTop();
    case TransformedWritingMode::kBottomToTopWritingMode:
      return box->MarginBottom();
    case TransformedWritingMode::kLeftToRightWritingMode:
      return box->MarginLeft();
    case TransformedWritingMode::kRightToLeftWritingMode:
      return box->MarginRight();
  }
  NOTREACHED();
  return box->MarginTop();
}

LayoutUnit FlexItem::CrossAxisMarginExtent() const {
  return algorithm->IsHorizontalFlow() ? box->MarginHeight()
                                       : box->MarginWidth();
}

LayoutUnit FlexItem::MarginBoxAscent() const {
  LayoutUnit ascent(box->FirstLineBoxBaseline());
  if (ascent == -1)
    ascent = cross_axis_size;
  return ascent + FlowAwareMarginBefore();
}

LayoutUnit FlexItem::AvailableAlignmentSpace(
    LayoutUnit line_cross_axis_extent) const {
  LayoutUnit cross_extent = CrossAxisMarginExtent() + cross_axis_size;
  return line_cross_axis_extent - cross_extent;
}

void FlexLine::FreezeViolations(Vector<FlexItem*>& violations) {
  for (size_t i = 0; i < violations.size(); ++i) {
    DCHECK(!violations[i]->frozen) << i;
    LayoutBox* child = violations[i]->box;
    LayoutUnit child_size = violations[i]->flexed_content_size;
    remaining_free_space -= child_size - violations[i]->flex_base_content_size;
    total_flex_grow -= child->Style()->FlexGrow();
    total_flex_shrink -= child->Style()->FlexShrink();
    total_weighted_flex_shrink -=
        child->Style()->FlexShrink() * violations[i]->flex_base_content_size;
    // totalWeightedFlexShrink can be negative when we exceed the precision of
    // a double when we initially calcuate totalWeightedFlexShrink. We then
    // subtract each child's weighted flex shrink with full precision, now
    // leading to a negative result. See
    // css3/flexbox/large-flex-shrink-assert.html
    total_weighted_flex_shrink = std::max(total_weighted_flex_shrink, 0.0);
    violations[i]->frozen = true;
  }
}

void FlexLine::FreezeInflexibleItems() {
  // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
  // we freeze all items with a flex factor of 0 as well as those with a min/max
  // size violation.
  FlexSign flex_sign = Sign();
  remaining_free_space = container_main_inner_size - sum_flex_base_size;

  Vector<FlexItem*> new_inflexible_items;
  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];
    LayoutBox* child = flex_item.box;
    DCHECK(!flex_item.box->IsOutOfFlowPositioned());
    DCHECK(!flex_item.frozen) << i;
    float flex_factor = (flex_sign == kPositiveFlexibility)
                            ? child->Style()->FlexGrow()
                            : child->Style()->FlexShrink();
    if (flex_factor == 0 ||
        (flex_sign == kPositiveFlexibility &&
         flex_item.flex_base_content_size >
             flex_item.hypothetical_main_content_size) ||
        (flex_sign == kNegativeFlexibility &&
         flex_item.flex_base_content_size <
             flex_item.hypothetical_main_content_size)) {
      flex_item.flexed_content_size = flex_item.hypothetical_main_content_size;
      new_inflexible_items.push_back(&flex_item);
    }
  }
  FreezeViolations(new_inflexible_items);
  initial_free_space = remaining_free_space;
}

bool FlexLine::ResolveFlexibleLengths() {
  LayoutUnit total_violation;
  LayoutUnit used_free_space;
  Vector<FlexItem*> min_violations;
  Vector<FlexItem*> max_violations;

  FlexSign flex_sign = Sign();
  double sum_flex_factors =
      (flex_sign == kPositiveFlexibility) ? total_flex_grow : total_flex_shrink;
  if (sum_flex_factors > 0 && sum_flex_factors < 1) {
    LayoutUnit fractional(initial_free_space * sum_flex_factors);
    if (fractional.Abs() < remaining_free_space.Abs())
      remaining_free_space = fractional;
  }

  for (size_t i = 0; i < line_items.size(); ++i) {
    FlexItem& flex_item = line_items[i];
    LayoutBox* child = flex_item.box;

    // This check also covers out-of-flow children.
    if (flex_item.frozen)
      continue;

    LayoutUnit child_size = flex_item.flex_base_content_size;
    double extra_space = 0;
    if (remaining_free_space > 0 && total_flex_grow > 0 &&
        flex_sign == kPositiveFlexibility && std::isfinite(total_flex_grow)) {
      extra_space =
          remaining_free_space * child->Style()->FlexGrow() / total_flex_grow;
    } else if (remaining_free_space < 0 && total_weighted_flex_shrink > 0 &&
               flex_sign == kNegativeFlexibility &&
               std::isfinite(total_weighted_flex_shrink) &&
               child->Style()->FlexShrink()) {
      extra_space = remaining_free_space * child->Style()->FlexShrink() *
                    flex_item.flex_base_content_size /
                    total_weighted_flex_shrink;
    }
    if (std::isfinite(extra_space))
      child_size += LayoutUnit::FromFloatRound(extra_space);

    LayoutUnit adjusted_child_size = flex_item.ClampSizeToMinAndMax(child_size);
    DCHECK_GE(adjusted_child_size, 0);
    flex_item.flexed_content_size = adjusted_child_size;
    used_free_space += adjusted_child_size - flex_item.flex_base_content_size;

    LayoutUnit violation = adjusted_child_size - child_size;
    if (violation > 0)
      min_violations.push_back(&flex_item);
    else if (violation < 0)
      max_violations.push_back(&flex_item);
    total_violation += violation;
  }

  if (total_violation) {
    FreezeViolations(total_violation < 0 ? max_violations : min_violations);
  } else {
    remaining_free_space -= used_free_space;
  }

  return !total_violation;
}

FlexLayoutAlgorithm::FlexLayoutAlgorithm(const ComputedStyle* style,
                                         LayoutUnit line_break_length,
                                         Vector<FlexItem>& all_items)
    : style_(style),
      line_break_length_(line_break_length),
      all_items_(all_items),
      next_item_index_(0) {
  for (FlexItem& item : all_items_)
    item.algorithm = this;
}

FlexLine* FlexLayoutAlgorithm::ComputeNextFlexLine() {
  Vector<FlexItem> line_items;
  LayoutUnit sum_flex_base_size;
  double total_flex_grow = 0;
  double total_flex_shrink = 0;
  double total_weighted_flex_shrink = 0;
  LayoutUnit sum_hypothetical_main_size;

  bool line_has_in_flow_item = false;

  for (; next_item_index_ < all_items_.size(); ++next_item_index_) {
    const FlexItem& flex_item = all_items_[next_item_index_];
    DCHECK(!flex_item.box->IsOutOfFlowPositioned());
    if (IsMultiline() &&
        sum_hypothetical_main_size +
                flex_item.HypotheticalMainAxisMarginBoxSize() >
            line_break_length_ &&
        line_has_in_flow_item)
      break;
    line_items.push_back(flex_item);
    line_has_in_flow_item = true;
    sum_flex_base_size += flex_item.FlexBaseMarginBoxSize();
    total_flex_grow += flex_item.box->Style()->FlexGrow();
    total_flex_shrink += flex_item.box->Style()->FlexShrink();
    total_weighted_flex_shrink +=
        flex_item.box->Style()->FlexShrink() * flex_item.flex_base_content_size;
    sum_hypothetical_main_size += flex_item.HypotheticalMainAxisMarginBoxSize();
  }
  DCHECK(line_items.size() > 0 || next_item_index_ == all_items_.size());
  if (line_items.size() > 0) {
    // This will std::move line_items.
    return &flex_lines_.emplace_back(
        line_items, sum_flex_base_size, total_flex_grow, total_flex_shrink,
        total_weighted_flex_shrink, sum_hypothetical_main_size);
  }
  return nullptr;
}

bool FlexLayoutAlgorithm::IsHorizontalFlow() const {
  if (style_->IsHorizontalWritingMode())
    return !style_->IsColumnFlexDirection();
  return style_->IsColumnFlexDirection();
}

bool FlexLayoutAlgorithm::IsLeftToRightFlow() const {
  if (style_->IsColumnFlexDirection()) {
    return blink::IsHorizontalWritingMode(style_->GetWritingMode()) ||
           IsFlippedLinesWritingMode(style_->GetWritingMode());
  }
  return style_->IsLeftToRightDirection() ^
         (style_->FlexDirection() == EFlexDirection::kRowReverse);
}

TransformedWritingMode FlexLayoutAlgorithm::GetTransformedWritingMode() const {
  return GetTransformedWritingMode(*style_);
}

TransformedWritingMode FlexLayoutAlgorithm::GetTransformedWritingMode(
    const ComputedStyle& style) {
  WritingMode mode = style.GetWritingMode();
  if (!style.IsColumnFlexDirection()) {
    static_assert(
        static_cast<TransformedWritingMode>(WritingMode::kHorizontalTb) ==
                TransformedWritingMode::kTopToBottomWritingMode &&
            static_cast<TransformedWritingMode>(WritingMode::kVerticalLr) ==
                TransformedWritingMode::kLeftToRightWritingMode &&
            static_cast<TransformedWritingMode>(WritingMode::kVerticalRl) ==
                TransformedWritingMode::kRightToLeftWritingMode,
        "WritingMode and TransformedWritingMode must match values.");
    return static_cast<TransformedWritingMode>(mode);
  }

  switch (mode) {
    case WritingMode::kHorizontalTb:
      return style.IsLeftToRightDirection()
                 ? TransformedWritingMode::kLeftToRightWritingMode
                 : TransformedWritingMode::kRightToLeftWritingMode;
    case WritingMode::kVerticalLr:
    case WritingMode::kVerticalRl:
      return style.IsLeftToRightDirection()
                 ? TransformedWritingMode::kTopToBottomWritingMode
                 : TransformedWritingMode::kBottomToTopWritingMode;
  }
  NOTREACHED();
  return TransformedWritingMode::kTopToBottomWritingMode;
}

}  // namespace blink
