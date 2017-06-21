// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineItemResult_h
#define NGInlineItemResult_h

#include "core/layout/ng/geometry/ng_box_strut.h"
#include "core/layout/ng/ng_layout_result.h"
#include "platform/LayoutUnit.h"
#include "platform/fonts/shaping/ShapeResult.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class NGInlineNode;

// The result of measuring NGInlineItem.
//
// This is a transient context object only while building line boxes.
// Produced while determining the line break points, but these data are needed
// to create line boxes.
//
// NGLineBreaker produces, and NGInlineLayoutAlgorithm consumes.
struct CORE_EXPORT NGInlineItemResult {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  // The index of NGInlineItem and its text range.
  unsigned item_index;
  unsigned start_offset;
  unsigned end_offset;

  // Inline size of this item.
  LayoutUnit inline_size;

  // ShapeResult for text items. Maybe different from NGInlineItem if re-shape
  // is needed in the line breaker.
  RefPtr<ShapeResult> shape_result;

  // NGLayoutResult for atomic inline items.
  RefPtr<NGLayoutResult> layout_result;

  // Margins for atomic inline items and open/close tags.
  NGBoxStrut margins;

  // Borders/padding for open tags.
  LayoutUnit borders_paddings_block_start;
  LayoutUnit borders_paddings_block_end;

  // Create a box when the box is empty, for open/close tags.
  unsigned needs_box_when_empty : 1;

  // Inside of this is not breakable.
  // Used only during line breaking.
  unsigned no_break_opportunities_inside : 1;

  // Lines must not break after this.
  // Used only during line breaking.
  unsigned prohibit_break_after : 1;

  NGInlineItemResult();
  NGInlineItemResult(unsigned index, unsigned start, unsigned end);
};

// Represents a set of NGInlineItemResult that form a line box.
using NGInlineItemResults = Vector<NGInlineItemResult, 32>;

// Represents a line to build.
//
// This is a transient context object only while building line boxes.
//
// NGLineBreaker produces, and NGInlineLayoutAlgorithm consumes.
class CORE_EXPORT NGLineInfo {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  NGLineInfo() {}
  explicit NGLineInfo(size_t capacity) : results_(capacity) {}

  // The style to use for the line.
  const ComputedStyle& LineStyle() const {
    DCHECK(line_style_);
    return *line_style_;
  }
  void SetLineStyle(const NGInlineNode&, bool is_first_line);

  // Use ::first-line style if true.
  // https://drafts.csswg.org/css-pseudo/#selectordef-first-line
  bool IsFirstLine() const { return is_first_line_; }

  // The last line of a block, or the line ends with a forced line break.
  // https://drafts.csswg.org/css-text-3/#propdef-text-align-last
  bool IsLastLine() const { return is_last_line_; }
  void SetIsLastLine(bool is_last_line) { is_last_line_ = is_last_line; }

  // NGInlineItemResults for this line.
  NGInlineItemResults& Results() { return results_; }
  const NGInlineItemResults& Results() const { return results_; }

 private:
  const ComputedStyle* line_style_ = nullptr;
  NGInlineItemResults results_;
  bool is_first_line_ = false;
  bool is_last_line_ = false;
};

}  // namespace blink

#endif  // NGInlineItemResult_h
