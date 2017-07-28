// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTextControlItem_h
#define LayoutTextControlItem_h

#include "core/layout/LayoutTextControl.h"
#include "core/layout/api/LayoutBoxModel.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class LayoutTextControlItem : public LayoutBoxModel {
 public:
  explicit LayoutTextControlItem(LayoutTextControl* layout_text_control)
      : LayoutBoxModel(layout_text_control) {}

  explicit LayoutTextControlItem(const LayoutItem& item)
      : LayoutBoxModel(item) {
    SECURITY_DCHECK(!item || item.IsTextControl());
  }

  explicit LayoutTextControlItem(std::nullptr_t) : LayoutBoxModel(nullptr) {}

  LayoutTextControlItem() {}

  RefPtr<ComputedStyle> CreateInnerEditorStyle(
      const ComputedStyle& start_style) const {
    return ToTextControl()->CreateInnerEditorStyle(start_style);
  }

 private:
  LayoutTextControl* ToTextControl() {
    return ToLayoutTextControl(GetLayoutObject());
  }

  const LayoutTextControl* ToTextControl() const {
    return ToLayoutTextControl(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutTextControlItem_h
