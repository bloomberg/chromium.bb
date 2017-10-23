// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineBreakToken_h
#define NGInlineBreakToken_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_box_state.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_break_token.h"

namespace blink {

// Represents a break token for an inline node.
class CORE_EXPORT NGInlineBreakToken : public NGBreakToken {
 public:
  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  // Takes ownership of the state_stack.
  static scoped_refptr<NGInlineBreakToken> Create(
      NGInlineNode node,
      unsigned item_index,
      unsigned text_offset,
      bool is_forced_break,
      std::unique_ptr<const NGInlineLayoutStateStack> state_stack) {
    return WTF::AdoptRef(new NGInlineBreakToken(node, item_index, text_offset,
                                                is_forced_break,
                                                std::move(state_stack)));
  }

  // Creates a break token for a node which cannot produce any more fragments.
  static scoped_refptr<NGInlineBreakToken> Create(NGLayoutInputNode node) {
    return WTF::AdoptRef(new NGInlineBreakToken(node));
  }

  ~NGInlineBreakToken() override;

  unsigned ItemIndex() const {
    DCHECK(!IsFinished());
    return item_index_;
  }

  unsigned TextOffset() const {
    DCHECK(!IsFinished());
    return text_offset_;
  }

  bool IsForcedBreak() const {
    DCHECK(!IsFinished());
    return is_forced_break_;
  }

  const NGInlineLayoutStateStack& StateStack() const {
    DCHECK(!IsFinished());
    return *state_stack_;
  }

 private:
  NGInlineBreakToken(NGInlineNode node,
                     unsigned item_index,
                     unsigned text_offset,
                     bool is_forced_break,
                     std::unique_ptr<const NGInlineLayoutStateStack>);

  explicit NGInlineBreakToken(NGLayoutInputNode node);

  unsigned item_index_;
  unsigned text_offset_;
  unsigned is_forced_break_ : 1;

  std::unique_ptr<const NGInlineLayoutStateStack> state_stack_;
};

DEFINE_TYPE_CASTS(NGInlineBreakToken,
                  NGBreakToken,
                  token,
                  token->IsInlineType(),
                  token.IsInlineType());

}  // namespace blink

#endif  // NGInlineBreakToken_h
