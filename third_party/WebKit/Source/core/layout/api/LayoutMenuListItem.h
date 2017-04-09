
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutMenuListItem_h
#define LayoutMenuListItem_h

#include "core/layout/LayoutMenuList.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutMenuListItem : public LayoutBlockItem {
 public:
  explicit LayoutMenuListItem(LayoutBlock* layout_block)
      : LayoutBlockItem(layout_block) {}

  explicit LayoutMenuListItem(const LayoutBlockItem& item)
      : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.IsMenuList());
  }

  explicit LayoutMenuListItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutMenuListItem() {}

  String GetText() const { return ToMenuList()->GetText(); }

 private:
  LayoutMenuList* ToMenuList() { return ToLayoutMenuList(GetLayoutObject()); }
  const LayoutMenuList* ToMenuList() const {
    return ToLayoutMenuList(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutMenuListItem_h
