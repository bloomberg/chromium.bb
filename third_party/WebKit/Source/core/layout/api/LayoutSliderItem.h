
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutSliderItem_h
#define LayoutSliderItem_h

#include "core/layout/LayoutSlider.h"
#include "core/layout/api/LayoutBlockItem.h"

namespace blink {

class LayoutSliderItem : public LayoutBlockItem {
 public:
  explicit LayoutSliderItem(LayoutSlider* layout_slider)
      : LayoutBlockItem(layout_slider) {}

  explicit LayoutSliderItem(const LayoutBlockItem& item)
      : LayoutBlockItem(item) {
    SECURITY_DCHECK(!item || item.IsSlider());
  }

  explicit LayoutSliderItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

  LayoutSliderItem() {}

  bool InDragMode() const { return ToSlider()->InDragMode(); }

 private:
  LayoutSlider* ToSlider() { return ToLayoutSlider(GetLayoutObject()); }
  const LayoutSlider* ToSlider() const {
    return ToLayoutSlider(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutSliderItem_h
