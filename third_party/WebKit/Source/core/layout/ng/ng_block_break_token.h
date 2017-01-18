// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockBreakToken_h
#define NGBlockBreakToken_h

#include "core/layout/ng/ng_break_token.h"
#include "platform/LayoutUnit.h"

namespace blink {

class NGBlockNode;

class NGBlockBreakToken : public NGBreakToken {
 public:
  NGBlockBreakToken(NGBlockNode* node, LayoutUnit break_offset)
      : NGBreakToken(kBlockBreakToken),
        node_(node),
        break_offset_(break_offset) {}

  NGBlockNode& InputNode() const { return *node_.get(); }
  LayoutUnit BreakOffset() const { return break_offset_; }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    NGBreakToken::trace(visitor);
    visitor->trace(node_);
  }

 private:
  Member<NGBlockNode> node_;
  LayoutUnit break_offset_;
};

DEFINE_TYPE_CASTS(NGBlockBreakToken,
                  NGBreakToken,
                  token,
                  token->Type() == NGBreakToken::kBlockBreakToken,
                  token.Type() == NGBreakToken::kBlockBreakToken);

}  // namespace blink

#endif  // NGBlockBreakToken_h
