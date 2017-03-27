// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_line_breaker.h"

#include "core/layout/ng/ng_box_fragment.h"
#include "core/layout/ng/ng_break_token.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment_builder.h"
#include "core/layout/ng/ng_inline_layout_algorithm.h"
#include "core/layout/ng/ng_inline_node.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "core/layout/ng/ng_text_fragment.h"
#include "core/style/ComputedStyle.h"
#include "platform/text/TextBreakIterator.h"

namespace blink {

static bool IsHangable(UChar ch) {
  return ch == ' ';
}

void NGLineBreaker::BreakLines(NGInlineLayoutAlgorithm* algorithm,
                               const String& text_content,
                               unsigned current_offset) {
  DCHECK(!text_content.isEmpty());
  // TODO(kojii): Give the locale to LazyLineBreakIterator.
  LazyLineBreakIterator line_break_iterator(text_content);
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
    algorithm->SetEnd(current_offset);

    // If there are more available spaces, mark the break opportunity and fetch
    // more text.
    // TODO(layout-ng): check if the height of the linebox can fit within
    // the current opportunity.
    if (algorithm->CanFitOnLine()) {
      algorithm->SetBreakOpportunity();
      continue;
    }

    // Compute hangable characters if exists.
    if (current_offset != start_of_hangables) {
      algorithm->SetStartOfHangables(start_of_hangables);
      // If text before hangables can fit, include it in the current line.
      if (algorithm->CanFitOnLine())
        algorithm->SetBreakOpportunity();
    }

    if (!algorithm->HasBreakOpportunity()) {
      // The first word (break opportunity) did not fit on the line.
      // Create a line including items that don't fit, allowing them to
      // overflow.
      if (!algorithm->CreateLine())
        return;
    } else {
      if (!algorithm->CreateLineUpToLastBreakOpportunity())
        return;

      // Items after the last break opportunity were sent to the next line.
      // Set the break opportunity, or create a line if the word doesn't fit.
      if (algorithm->HasItems()) {
        if (algorithm->CanFitOnLine())
          algorithm->SetBreakOpportunity();
        else if (!algorithm->CreateLine())
          return;
      }
    }
  }

  // If inline children ended with items left in the line builder, create a line
  // for them.
  if (algorithm->HasItems())
    algorithm->CreateLine();
}

}  // namespace blink
