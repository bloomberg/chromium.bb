// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class NGInlineItem;

// A collection of |NGInlineItem| associated to |LayoutNGText|.
//
// ***** INLINE ITEMS OWNERSHIP *****
// NGInlineItems in items_ are not owned by LayoutText but are pointers into the
// LayoutNGBlockFlow's items_. Should not be accessed outside of layout.
class NGInlineItems final {
 public:
  NGInlineItems() = default;

  void SetRange(NGInlineItem* begin, NGInlineItem* end) {
    begin_ = begin;
    end_ = end;
  }
  void Clear() { begin_ = end_ = nullptr; }

  bool IsEmpty() const { return begin_ == end_; }

  const NGInlineItem& front() const {
    CHECK(!IsEmpty());
    return *begin_;
  }

  const NGInlineItem* begin() const { return begin_; }
  const NGInlineItem* end() const { return end_; }

 private:
  NGInlineItem* begin_ = nullptr;
  NGInlineItem* end_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NGInlineItems);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_INLINE_ITEMS_H_
