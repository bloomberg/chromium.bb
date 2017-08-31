// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_child_iterator.h"

#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_layout_input_node.h"

namespace blink {

NGBlockChildIterator::NGBlockChildIterator(NGLayoutInputNode first_child,
                                           NGBlockBreakToken* break_token)
    : child_(first_child), break_token_(break_token), child_token_idx_(0) {
  // Locate the first child to resume layout at.
  if (!break_token)
    return;
  const auto& child_break_tokens = break_token->ChildBreakTokens();
  if (!child_break_tokens.size())
    return;
  child_ = child_break_tokens[0]->InputNode();
}

NGBlockChildIterator::Entry NGBlockChildIterator::NextChild() {
  NGBreakToken* child_break_token = nullptr;

  if (break_token_) {
    // If we're resuming layout after a fragmentainer break, we need to skip
    // siblings that we're done with. We may have been able to fully lay out
    // some node(s) preceding a node that we had to break inside (and therefore
    // were not able to fully lay out). This happens when we have parallel
    // flows [1], which are caused by floats, overflow, etc.
    //
    // [1] https://drafts.csswg.org/css-break/#parallel-flows
    const auto& child_break_tokens = break_token_->ChildBreakTokens();

    do {
      // Early exit if we've exhausted our child break tokens.
      if (child_token_idx_ >= child_break_tokens.size())
        break;

      // This child break token candidate doesn't match the current node, this
      // node must be unfinished.
      NGBreakToken* child_break_token_candidate =
          child_break_tokens[child_token_idx_].Get();
      if (child_break_token_candidate->InputNode() != child_)
        break;

      ++child_token_idx_;

      // We have only found a node if its break token is unfinished.
      if (!child_break_token_candidate->IsFinished()) {
        child_break_token = child_break_token_candidate;
        break;
      }
    } while ((child_ = child_.NextSibling()));
  }

  NGLayoutInputNode child = child_;
  if (child_)
    child_ = child_.NextSibling();

  return Entry(child, child_break_token);
}

}  // namespace blink
