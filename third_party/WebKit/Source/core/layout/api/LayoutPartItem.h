// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutPartItem_h
#define LayoutPartItem_h

#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutPartItem : public LayoutBoxItem {
 public:
  explicit LayoutPartItem(LayoutPart* layout_part)
      : LayoutBoxItem(layout_part) {}

  explicit LayoutPartItem(const LayoutItem& item) : LayoutBoxItem(item) {
    SECURITY_DCHECK(!item || item.IsLayoutPart());
  }

  explicit LayoutPartItem(std::nullptr_t) : LayoutBoxItem(nullptr) {}

  LayoutPartItem() {}

  void UpdateOnWidgetChange() { ToPart()->UpdateOnWidgetChange(); }

 private:
  LayoutPart* ToPart() { return ToLayoutPart(GetLayoutObject()); }

  const LayoutPart* ToPart() const { return ToLayoutPart(GetLayoutObject()); }
};

}  // namespace blink

#endif  // LayoutPartItem_h
