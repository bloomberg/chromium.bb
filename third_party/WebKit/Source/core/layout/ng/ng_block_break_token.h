// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockBreakToken_h
#define NGBlockBreakToken_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_break_token.h"
#include "platform/LayoutUnit.h"

namespace blink {

class NGBlockNode;

// Represents a break token for a block node.
class CORE_EXPORT NGBlockBreakToken : public NGBreakToken {
 public:
  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  //
  // The NGBlockBreakToken takes ownership of child_break_tokens, leaving it
  // empty for the caller.
  NGBlockBreakToken(NGBlockNode* node,
                    LayoutUnit used_block_size,
                    HeapVector<Member<NGBreakToken>>& child_break_tokens);

  // Creates a break token for a node which cannot produce any more fragments.
  explicit NGBlockBreakToken(NGBlockNode* node);

  // TODO(ikilpatrick): Remove this constructor and break_offset once we've
  // switched to new multi-col approach.
  NGBlockBreakToken(NGBlockNode* node, LayoutUnit break_offset);

  // Represents the amount of block size used in previous fragments.
  //
  // E.g. if the layout block specifies a block size of 200px, and the previous
  // fragments of this block used 150px (used block size), the next fragment
  // should have a size of 50px (assuming no additional fragmentation).
  LayoutUnit UsedBlockSize() const { return used_block_size_; }

  // The break tokens for children of the layout node.
  //
  // Each child we have visited previously in the block-flow layout algorithm
  // has an associated break token. This may be either finished (we should skip
  // this child) or unfinished (we should try and produce the next fragment for
  // this child).
  //
  // A child which we haven't visited yet doesn't have a break token here.
  const HeapVector<Member<NGBreakToken>>& ChildBreakTokens() const {
    return child_break_tokens_;
  }

  // TODO(ikilpatrick): Remove this accessor.
  LayoutUnit BreakOffset() const { return break_offset_; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    NGBreakToken::trace(visitor);
    visitor->trace(child_break_tokens_);
  }

 private:
  LayoutUnit break_offset_;
  LayoutUnit used_block_size_;
  HeapVector<Member<NGBreakToken>> child_break_tokens_;
};

DEFINE_TYPE_CASTS(NGBlockBreakToken,
                  NGBreakToken,
                  token,
                  token->Type() == NGBreakToken::kBlockBreakToken,
                  token.Type() == NGBreakToken::kBlockBreakToken);

}  // namespace blink

#endif  // NGBlockBreakToken_h
