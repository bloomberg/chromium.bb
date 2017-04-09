
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutBlockItem_h
#define LayoutBlockItem_h

#include "core/layout/LayoutBlock.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutBlockItem : public LayoutBoxItem {
 public:
  explicit LayoutBlockItem(LayoutBlock* layout_block)
      : LayoutBoxItem(layout_block) {}

  explicit LayoutBlockItem(const LayoutBoxItem& item) : LayoutBoxItem(item) {
    SECURITY_DCHECK(!item || item.IsLayoutBlock());
  }

  explicit LayoutBlockItem(std::nullptr_t) : LayoutBoxItem(nullptr) {}

  LayoutBlockItem() {}

  void FlipForWritingMode(LayoutRect& rect) const {
    ToBlock()->FlipForWritingMode(rect);
  }

  bool RecalcOverflowAfterStyleChange() {
    return ToBlock()->RecalcOverflowAfterStyleChange();
  }

  LayoutItem FirstChild() const { return LayoutItem(ToBlock()->FirstChild()); }

 private:
  LayoutBlock* ToBlock() { return ToLayoutBlock(GetLayoutObject()); }
  const LayoutBlock* ToBlock() const {
    return ToLayoutBlock(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutBlockItem_h
