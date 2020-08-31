// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_BREAK_TOKEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_BREAK_TOKEN_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/ng_break_token.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGInlineBreakToken;

// Represents a break token for a block node.
class CORE_EXPORT NGBlockBreakToken final : public NGBreakToken {
 public:
  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  //
  // The node is NGBlockNode, or any other NGLayoutInputNode that produces
  // anonymous box.
  static scoped_refptr<NGBlockBreakToken> Create(
      NGLayoutInputNode node,
      LayoutUnit consumed_block_size,
      unsigned sequence_number,
      const NGBreakTokenVector& child_break_tokens,
      NGBreakAppeal break_appeal,
      bool has_seen_all_children) {
    // We store the children list inline in the break token as a flexible
    // array. Therefore, we need to make sure to allocate enough space for
    // that array here, which requires a manual allocation + placement new.
    void* data = ::WTF::Partitions::FastMalloc(
        sizeof(NGBlockBreakToken) +
            child_break_tokens.size() * sizeof(NGBreakToken*),
        ::WTF::GetStringWithTypeName<NGBlockBreakToken>());
    new (data) NGBlockBreakToken(PassKey(), node, consumed_block_size,
                                 sequence_number, child_break_tokens,
                                 break_appeal, has_seen_all_children);
    return base::AdoptRef(static_cast<NGBlockBreakToken*>(data));
  }

  // Creates a break token for a node that needs to produce its first fragment
  // in the next fragmentainer. In this case we create a break token for a node
  // that hasn't yet produced any fragments.
  static scoped_refptr<NGBlockBreakToken> CreateBreakBefore(
      NGLayoutInputNode node,
      bool is_forced_break) {
    auto* token = new NGBlockBreakToken(PassKey(), node);
    token->is_break_before_ = true;
    token->is_forced_break_ = is_forced_break;
    return base::AdoptRef(token);
  }

  ~NGBlockBreakToken() override {
    for (const NGBreakToken* token : ChildBreakTokens())
      token->Release();
  }

  // Represents the amount of block-size consumed by previous fragments.
  //
  // E.g. if the node specifies a block-size of 200px, and the previous
  // fragments generated for this box consumed 150px in total (which is what
  // this method would return then), there's 50px left to consume. The next
  // fragment will become 50px tall, assuming no additional fragmentation (if
  // the fragmentainer is shorter than 50px, for instance).
  LayoutUnit ConsumedBlockSize() const { return consumed_block_size_; }

  // A unique identifier for a fragment that generates a break token. This is
  // unique within the generating layout input node. The break token of the
  // first fragment gets 0, then second 1, and so on. Note that we don't "count"
  // break tokens that aren't associated with a fragment (this happens when we
  // want a fragmentainer break before laying out the node). What the sequence
  // number is for such a break token is undefined.
  unsigned SequenceNumber() const {
    DCHECK(!IsBreakBefore());
    return sequence_number_;
  }

  // Return true if this is a break token that was produced without any
  // "preceding" fragment. This happens when we determine that the first
  // fragment for a node needs to be created in a later fragmentainer than the
  // one it was it was first encountered, due to block space shortage.
  bool IsBreakBefore() const { return is_break_before_; }

  bool IsForcedBreak() const { return is_forced_break_; }

  // Return true if all children have been "seen". When we have reached this
  // point, and resume layout in a fragmentainer, we should only process child
  // break tokens, if any, and not attempt to start laying out nodes that don't
  // have one (since all children are either finished, or have a break token).
  bool HasSeenAllChildren() const { return has_seen_all_children_; }

  // The break tokens for children of the layout node.
  //
  // Each child we have visited previously in the block-flow layout algorithm
  // has an associated break token. This may be either finished (we should skip
  // this child) or unfinished (we should try and produce the next fragment for
  // this child).
  //
  // A child which we haven't visited yet doesn't have a break token here.
  const base::span<const NGBreakToken* const> ChildBreakTokens() const {
    return base::make_span(child_break_tokens_, num_children_);
  }

  // Find the child NGInlineBreakToken for the specified node.
  const NGInlineBreakToken* InlineBreakTokenFor(const NGLayoutInputNode&) const;
  const NGInlineBreakToken* InlineBreakTokenFor(const LayoutBox&) const;

#if DCHECK_IS_ON()
  String ToString() const override;
#endif

  using PassKey = util::PassKey<NGBlockBreakToken>;

  // Must only be called from Create(), because it assumes that enough space
  // has been allocated in the flexible array to store the children.
  NGBlockBreakToken(PassKey,
                    NGLayoutInputNode node,
                    LayoutUnit consumed_block_size,
                    unsigned sequence_number,
                    const NGBreakTokenVector& child_break_tokens,
                    NGBreakAppeal break_appeal,
                    bool has_seen_all_children);

  explicit NGBlockBreakToken(PassKey, NGLayoutInputNode node);

 private:
  LayoutUnit consumed_block_size_;
  unsigned sequence_number_ = 0;

  wtf_size_t num_children_;
  // This must be the last member, because it is a flexible array.
  const NGBreakToken* child_break_tokens_[];
};

template <>
struct DowncastTraits<NGBlockBreakToken> {
  static bool AllowFrom(const NGBreakToken& token) {
    return token.IsBlockType();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_BREAK_TOKEN_H_
