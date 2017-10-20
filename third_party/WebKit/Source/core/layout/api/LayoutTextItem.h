
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTextItem_h
#define LayoutTextItem_h

#include "core/layout/LayoutText.h"
#include "core/layout/api/LayoutItem.h"

namespace blink {

class ComputedStyle;

class LayoutTextItem : public LayoutItem {
 public:
  explicit LayoutTextItem(LayoutText* layout_text) : LayoutItem(layout_text) {}

  explicit LayoutTextItem(const LayoutItem& item) : LayoutItem(item) {
    SECURITY_DCHECK(!item || item.IsText());
  }

  explicit LayoutTextItem(std::nullptr_t) : LayoutItem(nullptr) {}

  LayoutTextItem() {}

  void SetStyle(scoped_refptr<ComputedStyle> style) {
    ToText()->SetStyle(std::move(style));
  }

  void SetText(scoped_refptr<StringImpl> text, bool force = false) {
    ToText()->SetText(std::move(text), force);
  }

  bool IsTextFragment() const { return ToText()->IsTextFragment(); }

  void DirtyLineBoxes() { ToText()->DirtyLineBoxes(); }

 private:
  LayoutText* ToText() { return ToLayoutText(GetLayoutObject()); }
  const LayoutText* ToText() const { return ToLayoutText(GetLayoutObject()); }
};

}  // namespace blink

#endif  // LayoutTextItem_h
