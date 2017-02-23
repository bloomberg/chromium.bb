// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_text_layout_algorithm.h"

#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_line_builder.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/TextBreakIterator.h"

namespace blink {

NGTextLayoutAlgorithm::NGTextLayoutAlgorithm(
    NGInlineNode* inline_box,
    NGConstraintSpace* constraint_space,
    NGBreakToken* break_token)
    : inline_box_(inline_box),
      constraint_space_(constraint_space),
      break_token_(break_token) {
  DCHECK(inline_box_);
}

RefPtr<NGLayoutResult> NGTextLayoutAlgorithm::Layout() {
  ASSERT_NOT_REACHED();
  return nullptr;
}

static bool IsHangable(UChar ch) {
  return ch == ' ';
}

void NGTextLayoutAlgorithm::LayoutInline(NGLineBuilder* line_builder) {
  // TODO(kojii): Make this tickable. Each line is easy. Needs more thoughts
  // for each fragment in a line. Bidi reordering is probably atomic.
  // TODO(kojii): oof is not well-thought yet. The bottom static position may be
  // in the next line, https://github.com/w3c/csswg-drafts/issues/609
  const String& text_content = inline_box_->Text();
  DCHECK(!text_content.isEmpty());
  // TODO(kojii): Give the locale to LazyLineBreakIterator.
  LazyLineBreakIterator line_break_iterator(text_content);
  unsigned current_offset = 0;
  line_builder->SetStart(0, current_offset);
  const unsigned end_offset = text_content.length();
  while (current_offset < end_offset) {
    // Find the next break opportunity.
    int tmp_next_breakable_offset = -1;
    line_break_iterator.isBreakable(current_offset + 1,
                                    tmp_next_breakable_offset);
    current_offset =
        tmp_next_breakable_offset >= 0 ? tmp_next_breakable_offset : end_offset;
    DCHECK_LE(current_offset, end_offset);

    // Advance the break opportunity to the end of hangable characters; e.g.,
    // spaces.
    // Unlike the ICU line breaker, LazyLineBreakIterator breaks before
    // breakable spaces, and expect the line breaker to handle spaces
    // differently. This logic computes in the ICU way; break after spaces, and
    // handle spaces as hangable characters.
    unsigned start_of_hangables = current_offset;
    while (current_offset < end_offset &&
           IsHangable(text_content[current_offset]))
      current_offset++;

    // Set the end to the next break opportunity.
    line_builder->SetEnd(current_offset);

    // If there are more available spaces, mark the break opportunity and fetch
    // more text.
    if (line_builder->CanFitOnLine()) {
      line_builder->SetBreakOpportunity();
      continue;
    }

    // Compute hangable characters if exists.
    if (current_offset != start_of_hangables) {
      line_builder->SetStartOfHangables(start_of_hangables);
      // If text before hangables can fit, include it in the current line.
      if (line_builder->CanFitOnLine())
        line_builder->SetBreakOpportunity();
    }

    if (!line_builder->HasBreakOpportunity()) {
      // The first word (break opportunity) did not fit on the line.
      // Create a line including items that don't fit, allowing them to
      // overflow.
      line_builder->CreateLine();
    } else {
      line_builder->CreateLineUpToLastBreakOpportunity();

      // Items after the last break opportunity were sent to the next line.
      // Set the break opportunity, or create a line if the word doesn't fit.
      if (line_builder->HasItems()) {
        if (!line_builder->CanFitOnLine())
          line_builder->CreateLine();
        else
          line_builder->SetBreakOpportunity();
      }
    }
  }

  // If inline children ended with items left in the line builder, create a line
  // for them.
  if (line_builder->HasItems())
    line_builder->CreateLine();
}

}  // namespace blink
