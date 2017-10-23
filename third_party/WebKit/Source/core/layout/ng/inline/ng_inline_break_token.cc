// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_break_token.h"

namespace blink {

NGInlineBreakToken::NGInlineBreakToken(
    NGInlineNode node,
    unsigned item_index,
    unsigned text_offset,
    bool is_forced_break,
    std::unique_ptr<const NGInlineLayoutStateStack> state_stack)
    : NGBreakToken(kInlineBreakToken, kUnfinished, node),
      item_index_(item_index),
      text_offset_(text_offset),
      is_forced_break_(is_forced_break),
      state_stack_(std::move(state_stack)) {
  // Use nullptr for the initial layout, rather than (0, 0) break token.
  DCHECK(item_index || text_offset);
}

NGInlineBreakToken::NGInlineBreakToken(NGLayoutInputNode node)
    : NGBreakToken(kInlineBreakToken, kFinished, node),
      item_index_(0),
      text_offset_(0),
      is_forced_break_(false),
      state_stack_(nullptr) {}

NGInlineBreakToken::~NGInlineBreakToken() {}

}  // namespace blink
