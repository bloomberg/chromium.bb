// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutLIItem_h
#define LayoutLIItem_h

#include "core/layout/LayoutListItem.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutLIItem : public LayoutBoxItem {
 public:
  explicit LayoutLIItem(LayoutListItem* layout_list_item)
      : LayoutBoxItem(layout_list_item) {}

  explicit LayoutLIItem(const LayoutItem& item) : LayoutBoxItem(item) {
    SECURITY_DCHECK(!item || item.IsListItem());
  }

  explicit LayoutLIItem(std::nullptr_t) : LayoutBoxItem(nullptr) {}

  LayoutLIItem() {}

  ListItemOrdinal& Ordinal() { return ToListItem()->Ordinal(); }

 private:
  LayoutListItem* ToListItem() { return ToLayoutListItem(GetLayoutObject()); }

  const LayoutListItem* ToListItem() const {
    return ToLayoutListItem(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutLIItem_h
