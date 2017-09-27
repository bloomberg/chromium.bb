
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutFullScreenItem_h
#define LayoutFullScreenItem_h

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutFullScreenItem : public LayoutBlockItem {
 public:
  explicit LayoutFullScreenItem(LayoutBlock* layout_block)
      : LayoutBlockItem(layout_block) {}

  explicit LayoutFullScreenItem(const LayoutBlockItem& item)
      : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.IsLayoutFullScreen());
  }

  explicit LayoutFullScreenItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutFullScreenItem() {}

  void UnwrapLayoutObject() { return ToFullScreen()->UnwrapLayoutObject(); }

 private:
  LayoutFullScreen* ToFullScreen() {
    return ToLayoutFullScreen(GetLayoutObject());
  }
  const LayoutFullScreen* ToFullScreen() const {
    return ToLayoutFullScreen(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutFullScreenItem_h
