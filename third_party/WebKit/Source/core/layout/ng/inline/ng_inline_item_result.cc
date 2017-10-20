// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_item_result.h"

#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_constraint_space.h"

namespace blink {

NGInlineItemResult::NGInlineItemResult()
    : item(nullptr), item_index(0), start_offset(0), end_offset(0) {}

NGInlineItemResult::NGInlineItemResult(const NGInlineItem* item,
                                       unsigned index,
                                       unsigned start,
                                       unsigned end)
    : item(item), item_index(index), start_offset(start), end_offset(end) {}

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
    // 'text-indent' applies to block container, and percentage is of its
    // containing block.
    // https://drafts.csswg.org/css-text-3/#valdef-text-indent-percentage
    // In our constraint space tree, parent constraint space is of its
    // containing block.
    // TODO(kojii): ComputeMinMaxSize does not know parent constraint
    // space that we cannot compute percent for text-indent.
    const Length& length = line_style_->TextIndent();
    LayoutUnit maximum_value;
    if (length.IsPercentOrCalc() &&
        constraint_space.ParentPercentageResolutionInlineSize().has_value())
      maximum_value = *constraint_space.ParentPercentageResolutionInlineSize();
    text_indent_ = MinimumValueForLength(length, maximum_value);
  } else {
    text_indent_ = LayoutUnit();
  }
}

void NGLineInfo::SetLineOffset(NGLogicalOffset line_offset,
                               LayoutUnit available_width) {
  line_offset_ = line_offset;
  available_width_ = available_width;
}

void NGLineInfo::SetLineEndShapeResult(
    scoped_refptr<ShapeResult> result,
    scoped_refptr<const ComputedStyle> style) {
  line_end_shape_result_ = std::move(result);
  line_end_style_ = std::move(style);
}

}  // namespace blink
