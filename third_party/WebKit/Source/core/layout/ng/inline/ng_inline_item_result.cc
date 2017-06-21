// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_item_result.h"

#include "core/layout/ng/inline/ng_inline_node.h"

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

void NGLineInfo::SetLineStyle(const NGInlineNode& node, bool is_first_line) {
  LayoutObject* layout_object = node.GetLayoutObject();
  if (is_first_line &&
      layout_object->GetDocument().GetStyleEngine().UsesFirstLineRules()) {
    is_first_line_ = true;
    line_style_ = layout_object->FirstLineStyle();
    return;
  }
  is_first_line_ = false;
  line_style_ = layout_object->Style();
}

void NGLineInfo::SetLineLocation(LayoutUnit line_left,
                                 LayoutUnit available_width,
                                 LayoutUnit line_top) {
  line_left_ = line_left;
  available_width_ = available_width;
  line_top_ = line_top;
}

}  // namespace blink
