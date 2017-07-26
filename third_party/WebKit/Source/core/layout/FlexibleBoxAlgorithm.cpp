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

FlexLayoutAlgorithm::FlexLayoutAlgorithm(const ComputedStyle* style,
                                         LayoutUnit line_break_length,
                                         const Vector<FlexItem>& all_items)
    : style_(style),
      line_break_length_(line_break_length),
      all_items_(all_items),
      next_item_index_(0) {}

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

}  // namespace blink
