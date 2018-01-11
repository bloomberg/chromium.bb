// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/inline/ng_inline_break_token.h"

#include "platform/wtf/text/StringBuilder.h"

namespace blink {

NGInlineBreakToken::NGInlineBreakToken(
    NGInlineNode node,
    const ComputedStyle* style,
    unsigned item_index,
    unsigned text_offset,
    bool is_forced_break,
    std::unique_ptr<const NGInlineLayoutStateStack> state_stack)
    : NGBreakToken(kInlineBreakToken, kUnfinished, node),
      style_(style),
      item_index_(item_index),
      text_offset_(text_offset),
      is_forced_break_(is_forced_break),
      ignore_floats_(false),
      state_stack_(std::move(state_stack)) {}

NGInlineBreakToken::NGInlineBreakToken(NGLayoutInputNode node)
    : NGBreakToken(kInlineBreakToken, kFinished, node),
      item_index_(0),
      text_offset_(0),
      is_forced_break_(false),
      ignore_floats_(false),
      state_stack_(nullptr) {}

NGInlineBreakToken::~NGInlineBreakToken() = default;

#ifndef NDEBUG

String NGInlineBreakToken::ToString() const {
  StringBuilder string_builder;
  string_builder.Append(NGBreakToken::ToString());
  if (!IsFinished()) {
    string_builder.Append(
        String::Format(" index:%u offset:%u", ItemIndex(), TextOffset()));
    if (IsForcedBreak())
      string_builder.Append(" forced");
  }
  return string_builder.ToString();
}

#endif  // NDEBUG

}  // namespace blink
