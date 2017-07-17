// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_break_token.h"

#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_unpositioned_float.h"

namespace blink {

NGBlockBreakToken::NGBlockBreakToken(
    NGLayoutInputNode node,
    LayoutUnit used_block_size,
    Vector<RefPtr<NGBreakToken>>& child_break_tokens)
    : NGBreakToken(kBlockBreakToken, kUnfinished, node),
      used_block_size_(used_block_size) {
  child_break_tokens_.swap(child_break_tokens);
}

NGBlockBreakToken::NGBlockBreakToken(NGLayoutInputNode node)
    : NGBreakToken(kBlockBreakToken, kFinished, node) {}

}  // namespace blink
