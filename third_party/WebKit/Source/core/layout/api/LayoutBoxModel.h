// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutBoxModel_h
#define LayoutBoxModel_h

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LayoutItem.h"

namespace blink {

class LayoutBoxModelObject;

class LayoutBoxModel : public LayoutItem {
 public:
  explicit LayoutBoxModel(LayoutBoxModelObject* layout_box)
      : LayoutItem(layout_box) {}

  explicit LayoutBoxModel(const LayoutItem& item) : LayoutItem(item) {
    SECURITY_DCHECK(!item || item.IsBoxModelObject());
  }

  explicit LayoutBoxModel(std::nullptr_t) : LayoutItem(nullptr) {}

  LayoutBoxModel() {}

  PaintLayer* Layer() const { return ToBoxModel()->Layer(); }

  PaintLayerScrollableArea* GetScrollableArea() const {
    return ToBoxModel()->GetScrollableArea();
  }

  LayoutUnit BorderTop() const { return ToBoxModel()->BorderTop(); }

  LayoutUnit BorderLeft() const { return ToBoxModel()->BorderLeft(); }

  LayoutUnit PaddingTop() const { return ToBoxModel()->PaddingTop(); }

  LayoutUnit PaddingLeft() const { return ToBoxModel()->PaddingLeft(); }

 private:
  LayoutBoxModelObject* ToBoxModel() {
    return ToLayoutBoxModelObject(GetLayoutObject());
  }
  const LayoutBoxModelObject* ToBoxModel() const {
    return ToLayoutBoxModelObject(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutBoxModel_h
