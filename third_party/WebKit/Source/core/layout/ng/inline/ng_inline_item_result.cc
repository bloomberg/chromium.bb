// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_item_result.h"

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_constraint_space.h"

namespace blink {

NGInlineItemResult::NGInlineItemResult() {}

NGInlineItemResult::NGInlineItemResult(unsigned index,
                                       unsigned start,
                                       unsigned end)
    : item_index(index),
      start_offset(start),
      end_offset(end),
      no_break_opportunities_inside(false),
      prohibit_break_after(false) {}

void NGLineInfo::SetLineStyle(const NGInlineNode& node,
                              const NGConstraintSpace& constraint_space,
                              bool is_first_line,
                              bool is_after_forced_break) {
  LayoutObject* layout_object = node.GetLayoutObject();
  use_first_line_style_ =
      is_first_line &&
      layout_object->GetDocument().GetStyleEngine().UsesFirstLineRules();
  line_style_ = layout_object->Style(use_first_line_style_);

  if (line_style_->ShouldUseTextIndent(is_first_line, is_after_forced_break)) {
    text_indent_ =
        MinimumValueForLength(line_style_->TextIndent(),
                              constraint_space.AvailableSize().inline_size);
  } else {
    text_indent_ = LayoutUnit();
  }
}

void NGLineInfo::SetLineLocation(LayoutUnit line_left,
                                 LayoutUnit available_width,
                                 LayoutUnit line_top) {
  line_left_ = line_left;
  available_width_ = available_width;
  line_top_ = line_top;
}

}  // namespace blink
