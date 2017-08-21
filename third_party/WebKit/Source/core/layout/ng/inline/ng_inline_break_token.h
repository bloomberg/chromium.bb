// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGInlineBreakToken_h
#define NGInlineBreakToken_h

#include "core/CoreExport.h"
#include "core/layout/ng/inline/ng_inline_node.h"
#include "core/layout/ng/ng_break_token.h"

namespace blink {

// Represents a break token for an inline node.
class CORE_EXPORT NGInlineBreakToken : public NGBreakToken {
 public:
  // Creates a break token for a node which did fragment, and can potentially
  // produce more fragments.
  static RefPtr<NGInlineBreakToken> Create(NGInlineNode node,
                                           unsigned item_index,
                                           unsigned text_offset) {
    return AdoptRef(new NGInlineBreakToken(node, item_index, text_offset));
  }

  // Creates a break token for a node which cannot produce any more fragments.
  static RefPtr<NGInlineBreakToken> Create(NGLayoutInputNode node) {
    return AdoptRef(new NGInlineBreakToken(node));
  }

  unsigned ItemIndex() const { return item_index_; }
  unsigned TextOffset() const { return text_offset_; }

 private:
  NGInlineBreakToken(NGInlineNode node,
                     unsigned item_index,
                     unsigned text_offset);

  explicit NGInlineBreakToken(NGLayoutInputNode node);

  unsigned item_index_;
  unsigned text_offset_;
};

DEFINE_TYPE_CASTS(NGInlineBreakToken,
                  NGBreakToken,
                  token,
                  token->IsInlineType(),
                  token.IsInlineType());

}  // namespace blink

#endif  // NGInlineBreakToken_h
