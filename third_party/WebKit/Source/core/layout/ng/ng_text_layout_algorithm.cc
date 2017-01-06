// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_layout_algorithm.h"

#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_line_builder.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/style/ComputedStyle.h"
#include "core/layout/ng/ng_box_fragment.h"

namespace blink {

NGTextLayoutAlgorithm::NGTextLayoutAlgorithm(
    NGInlineNode* inline_box,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : NGLayoutAlgorithm(kTextLayoutAlgorithm),
      inline_box_(inline_box),
      constraint_space_(constraint_space),
      break_token_(break_token) {
  DCHECK(inline_box_);
}

NGLayoutStatus NGTextLayoutAlgorithm::Layout(NGPhysicalFragment*,
                                             NGPhysicalFragment** fragment_out,
                                             NGLayoutAlgorithm**) {
  ASSERT_NOT_REACHED();
  *fragment_out = nullptr;
  return kNewFragment;
}

bool NGTextLayoutAlgorithm::LayoutInline(NGLineBuilder* line_builder) {
  // TODO(kojii): Make this tickable. Each line is easy. Needs more thoughts
  // for each fragment in a line. Bidi reordering is probably atomic.
  // TODO(kojii): oof is not well-thought yet. The bottom static position may be
  // in the next line, https://github.com/w3c/csswg-drafts/issues/609
  const Vector<NGLayoutInlineItem>& items = inline_box_->Items();
  for (unsigned start_index = 0; start_index < items.size();) {
    const NGLayoutInlineItem& start_item = items[start_index];
    // Make a bidi control a chunk by itself.
    // TODO(kojii): atomic inline is not well-thought yet.
    if (!start_item.Style()) {
      line_builder->Add(start_index, start_index + 1, LayoutUnit());
      start_index++;
      continue;
    }

    // TODO(kojii): Implement the line breaker.

    LayoutUnit inline_size = start_item.InlineSize();
    unsigned i = start_index + 1;
    for (; i < items.size(); i++) {
      const NGLayoutInlineItem& item = items[i];
      // Split chunks before bidi controls, or at bidi level boundaries.
      if (!item.Style() || item.BidiLevel() != start_item.BidiLevel()) {
        break;
      }
      inline_size += item.InlineSize();
    }
    line_builder->Add(start_index, i, inline_size);
    start_index = i;
  }
  line_builder->CreateLine();
  return true;
}

DEFINE_TRACE(NGTextLayoutAlgorithm) {
  NGLayoutAlgorithm::trace(visitor);
  visitor->trace(inline_box_);
  visitor->trace(constraint_space_);
  visitor->trace(break_token_);
}

}  // namespace blink
