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

namespace blink {

FlexItem::FlexItem(LayoutBox* box,
                   LayoutUnit flex_base_content_size,
                   LayoutUnit hypothetical_main_content_size,
                   LayoutUnit main_axis_border_and_padding,
                   LayoutUnit main_axis_margin)
    : box(box),
      flex_base_content_size(flex_base_content_size),
      hypothetical_main_content_size(hypothetical_main_content_size),
      main_axis_border_and_padding(main_axis_border_and_padding),
      main_axis_margin(main_axis_margin),
      frozen(false) {
  DCHECK(!box->IsOutOfFlowPositioned());
}

FlexLayoutAlgorithm::FlexLayoutAlgorithm(const ComputedStyle* style,
                                         LayoutUnit line_break_length,
                                         const Vector<FlexItem>& all_items)
    : style_(style),
      line_break_length_(line_break_length),
      all_items_(all_items) {}

bool FlexLayoutAlgorithm::ComputeNextFlexLine(
    size_t& next_index,
    Vector<FlexItem>& line_items,
    LayoutUnit& sum_flex_base_size,
    double& total_flex_grow,
    double& total_flex_shrink,
    double& total_weighted_flex_shrink,
    LayoutUnit& sum_hypothetical_main_size) {
  line_items.clear();
  sum_flex_base_size = LayoutUnit();
  total_flex_grow = total_flex_shrink = total_weighted_flex_shrink = 0;
  sum_hypothetical_main_size = LayoutUnit();

  bool line_has_in_flow_item = false;

  for (; next_index < all_items_.size(); ++next_index) {
    const FlexItem& flex_item = all_items_[next_index];
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
  DCHECK(line_items.size() > 0 || next_index == all_items_.size());
  return line_items.size() > 0;
}

}  // namespace blink
