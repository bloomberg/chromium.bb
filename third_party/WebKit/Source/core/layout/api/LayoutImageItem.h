// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutImageItem_h
#define LayoutImageItem_h

#include "core/layout/LayoutImage.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutImageItem : public LayoutBoxItem {
 public:
  explicit LayoutImageItem(LayoutImage* layout_image)
      : LayoutBoxItem(layout_image) {}

  explicit LayoutImageItem(const LayoutItem& item) : LayoutBoxItem(item) {
    SECURITY_DCHECK(!item || item.IsImage());
  }

  explicit LayoutImageItem(std::nullptr_t) : LayoutBoxItem(nullptr) {}

  LayoutImageItem() {}

  void SetImageDevicePixelRatio(float factor) {
    ToImage()->SetImageDevicePixelRatio(factor);
  }

 private:
  LayoutImage* ToImage() { return ToLayoutImage(GetLayoutObject()); }

  const LayoutImage* ToImage() const {
    return ToLayoutImage(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutImageItem_h
