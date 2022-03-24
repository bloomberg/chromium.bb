// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_WORD_BREAK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_WORD_BREAK_H_

#include "third_party/blink/renderer/core/layout/layout_word_break.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_span.h"

namespace blink {

// This class is identical to |LayoutWordBreak| except for this class returns
// true for |IsLayoutNGObject()| and |NGInlineItem| support, to become child
// of |LayoutNGTextCombine|. See also |LayoutNGBR|.
// TODO(yosin): Once we get rid of |IsLayoutNGObject()|, we should unify this
// class |LayoutWordBreak|.
class CORE_EXPORT LayoutNGWordBreak final : public LayoutWordBreak {
 public:
  explicit LayoutNGWordBreak(Node* node) : LayoutWordBreak(node) {}

  bool IsLayoutNGObject() const final {
    NOT_DESTROYED();
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(inline_items_);
    LayoutWordBreak::Trace(visitor);
  }

 private:
  const NGInlineItemSpan* GetNGInlineItems() const final {
    NOT_DESTROYED();
    return &inline_items_;
  }
  NGInlineItemSpan* GetNGInlineItems() final {
    NOT_DESTROYED();
    return &inline_items_;
  }

  NGInlineItemSpan inline_items_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_WORD_BREAK_H_
