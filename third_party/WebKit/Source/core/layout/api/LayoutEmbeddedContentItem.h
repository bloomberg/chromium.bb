// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutEmbeddedContentItem_h
#define LayoutEmbeddedContentItem_h

#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/api/LayoutBoxItem.h"

namespace blink {

class LayoutEmbeddedContentItem : public LayoutBoxItem {
 public:
  explicit LayoutEmbeddedContentItem(
      LayoutEmbeddedContent* layout_embedded_content)
      : LayoutBoxItem(layout_embedded_content) {}

  explicit LayoutEmbeddedContentItem(const LayoutItem& item)
      : LayoutBoxItem(item) {
    SECURITY_DCHECK(!item || item.IsLayoutEmbeddedContent());
  }

  explicit LayoutEmbeddedContentItem(std::nullptr_t) : LayoutBoxItem(nullptr) {}

  LayoutEmbeddedContentItem() {}

  void UpdateOnEmbeddedContentViewChange() {
    ToPart()->UpdateOnEmbeddedContentViewChange();
  }

 private:
  LayoutEmbeddedContent* ToPart() {
    return ToLayoutEmbeddedContent(GetLayoutObject());
  }

  const LayoutEmbeddedContent* ToPart() const {
    return ToLayoutEmbeddedContent(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutEmbeddedContentItem_h
