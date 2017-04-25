// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/api/LayoutSliderItem.h"

#include "core/layout/LayoutSlider.h"

namespace blink {

LayoutSliderItem::LayoutSliderItem(LayoutSlider* layout_slider)
    : LayoutBlockItem(layout_slider) {}

LayoutSliderItem::LayoutSliderItem(const LayoutBlockItem& item)
    : LayoutBlockItem(item) {
  SECURITY_DCHECK(!item || item.IsSlider());
}

LayoutSliderItem::LayoutSliderItem(std::nullptr_t) : LayoutBlockItem(nullptr) {}

LayoutSliderItem::LayoutSliderItem() = default;

bool LayoutSliderItem::InDragMode() const {
  return ToSlider()->InDragMode();
}

LayoutSlider* LayoutSliderItem::ToSlider() {
  return ToLayoutSlider(GetLayoutObject());
}

const LayoutSlider* LayoutSliderItem::ToSlider() const {
  return ToLayoutSlider(GetLayoutObject());
}

}  // namespace blink
