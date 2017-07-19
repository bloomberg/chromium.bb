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

#ifndef FlexibleBoxAlgorithm_h
#define FlexibleBoxAlgorithm_h

#include "core/CoreExport.h"
#include "core/layout/OrderIterator.h"
#include "core/style/ComputedStyle.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/Vector.h"

namespace blink {

class LayoutBox;

class FlexItem {
 public:
  FlexItem(LayoutBox*,
           LayoutUnit flex_base_content_size,
           LayoutUnit hypothetical_main_content_size,
           LayoutUnit main_axis_border_and_padding,
           LayoutUnit main_axis_margin);

  LayoutUnit HypotheticalMainAxisMarginBoxSize() const {
    return hypothetical_main_content_size + main_axis_border_and_padding +
           main_axis_margin;
  }

  LayoutUnit FlexBaseMarginBoxSize() const {
    return flex_base_content_size + main_axis_border_and_padding +
           main_axis_margin;
  }

  LayoutUnit FlexedMarginBoxSize() const {
    return flexed_content_size + main_axis_border_and_padding +
           main_axis_margin;
  }

  LayoutBox* box;
  const LayoutUnit flex_base_content_size;
  const LayoutUnit hypothetical_main_content_size;
  const LayoutUnit main_axis_border_and_padding;
  const LayoutUnit main_axis_margin;
  LayoutUnit flexed_content_size;
  bool frozen;
};

struct FlexLine {
  void Reset() {
    line_items.clear();
    sum_flex_base_size = LayoutUnit();
    total_flex_grow = total_flex_shrink = total_weighted_flex_shrink = 0;
    sum_hypothetical_main_size = LayoutUnit();
    cross_axis_offset = cross_axis_extent = max_ascent = LayoutUnit();
  }

  // These fields get filled in by ComputeNextFlexLine.
  Vector<FlexItem> line_items;
  LayoutUnit sum_flex_base_size;
  double total_flex_grow;
  double total_flex_shrink;
  double total_weighted_flex_shrink;
  // The hypothetical main size of an item is the flex base size clamped
  // according to its min and max main size properties
  LayoutUnit sum_hypothetical_main_size;

  // These get filled in by LayoutAndPlaceChildren (for now)
  // TODO(cbiesinger): Move that to FlexibleBoxAlgorithm.
  LayoutUnit cross_axis_offset;
  LayoutUnit cross_axis_extent;
  LayoutUnit max_ascent;
};

class FlexLayoutAlgorithm {
  WTF_MAKE_NONCOPYABLE(FlexLayoutAlgorithm);

 public:
  FlexLayoutAlgorithm(const ComputedStyle*,
                      LayoutUnit line_break_length,
                      const Vector<FlexItem>& all_items);

  bool ComputeNextFlexLine(size_t* next_index, FlexLine*);

 private:
  bool IsMultiline() const { return style_->FlexWrap() != EFlexWrap::kNowrap; }

  const ComputedStyle* style_;
  LayoutUnit line_break_length_;
  const Vector<FlexItem>& all_items_;
};

}  // namespace blink

#endif  // FlexibleBoxAlgorithm_h
