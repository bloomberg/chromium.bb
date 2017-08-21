// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockBreakToken_h
#define NGBlockBreakToken_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_break_token.h"
#include "platform/LayoutUnit.h"

namespace blink {

// Represents a break token for a block node.
class CORE_EXPORT NGBlockBreakToken : public NGBreakToken {
 public:
  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  //
  // The NGBlockBreakToken takes ownership of child_break_tokens, leaving it
  // empty for the caller.
  //
  // The node is NGBlockNode, or any other NGLayoutInputNode that produces
  // anonymous box.
  static RefPtr<NGBlockBreakToken> Create(
      NGLayoutInputNode node,
      LayoutUnit used_block_size,
      Vector<RefPtr<NGBreakToken>>& child_break_tokens) {
    return AdoptRef(
        new NGBlockBreakToken(node, used_block_size, child_break_tokens));
  }

  // Creates a break token for a node which cannot produce any more fragments.
  static RefPtr<NGBlockBreakToken> Create(NGLayoutInputNode node) {
    return AdoptRef(new NGBlockBreakToken(node));
  }

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
  const Vector<RefPtr<NGBreakToken>>& ChildBreakTokens() const {
    return child_break_tokens_;
  }

 private:
  NGBlockBreakToken(NGLayoutInputNode node,
                    LayoutUnit used_block_size,
                    Vector<RefPtr<NGBreakToken>>& child_break_tokens);

  explicit NGBlockBreakToken(NGLayoutInputNode node);

  LayoutUnit used_block_size_;
  Vector<RefPtr<NGBreakToken>> child_break_tokens_;
};

DEFINE_TYPE_CASTS(NGBlockBreakToken,
                  NGBreakToken,
                  token,
                  token->IsBlockType(),
                  token.IsBlockType());

}  // namespace blink

#endif  // NGBlockBreakToken_h
