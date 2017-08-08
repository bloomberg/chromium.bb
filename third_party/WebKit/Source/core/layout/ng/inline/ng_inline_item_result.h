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

class NGConstraintSpace;
class NGInlineItem;
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
  // The NGInlineItem and its index.
  const NGInlineItem* item;
  unsigned item_index;

  // The range of text content for this item.
  unsigned start_offset;
  unsigned end_offset;

  // Inline size of this item.
  LayoutUnit inline_size;

  // ShapeResult for text items. Maybe different from NGInlineItem if re-shape
  // is needed in the line breaker.
  RefPtr<const ShapeResult> shape_result;

  // NGLayoutResult for atomic inline items.
  RefPtr<NGLayoutResult> layout_result;

  // Margins for atomic inline items and open/close tags.
  NGBoxStrut margins;

  // Borders/padding for open tags.
  LayoutUnit borders_paddings_block_start;
  LayoutUnit borders_paddings_block_end;

  // Create a box when the box is empty, for open/close tags.
  bool needs_box_when_empty = false;

  // Inside of this is not breakable. Set only for text items.
  // Used only during line breaking.
  bool no_break_opportunities_inside = false;

  // Lines must not break after this. Set for all items.
  // Used only during line breaking.
  bool prohibit_break_after = false;

  // Has spaces that hangs beyond the end margin.
  // Set only for text items.
  bool has_hanging_spaces = false;

  NGInlineItemResult();
  NGInlineItemResult(const NGInlineItem*,
                     unsigned index,
                     unsigned start,
                     unsigned end);
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
  void SetLineStyle(const NGInlineNode&,
                    const NGConstraintSpace&,
                    bool is_first_line,
                    bool is_after_forced_break);

  // Use ::first-line style if true.
  // https://drafts.csswg.org/css-pseudo/#selectordef-first-line
  // This is false for the "first formatted line" if '::first-line' rule is not
  // used in the document.
  // https://www.w3.org/TR/CSS22/selector.html#first-formatted-line
  bool UseFirstLineStyle() const { return use_first_line_style_; }

  // The last line of a block, or the line ends with a forced line break.
  // https://drafts.csswg.org/css-text-3/#propdef-text-align-last
  bool IsLastLine() const { return is_last_line_; }
  void SetIsLastLine(bool is_last_line) { is_last_line_ = is_last_line; }

  // NGInlineItemResults for this line.
  NGInlineItemResults& Results() { return results_; }
  const NGInlineItemResults& Results() const { return results_; }

  LayoutUnit TextIndent() const { return text_indent_; }

  LayoutUnit LineLeft() const { return line_left_; }
  LayoutUnit AvailableWidth() const { return available_width_; }
  LayoutUnit LineTop() const { return line_top_; }
  void SetLineLocation(LayoutUnit line_left,
                       LayoutUnit available_width,
                       LayoutUnit line_top);

 private:
  const ComputedStyle* line_style_ = nullptr;
  NGInlineItemResults results_;

  LayoutUnit line_left_;
  LayoutUnit available_width_;
  LayoutUnit line_top_;
  LayoutUnit text_indent_;

  bool use_first_line_style_ = false;
  bool is_last_line_ = false;
};

}  // namespace blink

#endif  // NGInlineItemResult_h
