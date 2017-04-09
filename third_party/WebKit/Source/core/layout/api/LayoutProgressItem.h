
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutProgressItem_h
#define LayoutProgressItem_h

#include "core/layout/LayoutProgress.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutProgressItem : public LayoutBlockItem {
 public:
  explicit LayoutProgressItem(LayoutProgress* layout_progress)
      : LayoutBlockItem(layout_progress) {}

  explicit LayoutProgressItem(const LayoutBlockItem& item)
      : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.IsProgress());
  }

  explicit LayoutProgressItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutProgressItem() {}

  bool IsDeterminate() const { return ToProgress()->IsDeterminate(); }

  void UpdateFromElement() { return ToProgress()->UpdateFromElement(); }

 private:
  LayoutProgress* ToProgress() { return ToLayoutProgress(GetLayoutObject()); }
  const LayoutProgress* ToProgress() const {
    return ToLayoutProgress(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutProgressItem_h
