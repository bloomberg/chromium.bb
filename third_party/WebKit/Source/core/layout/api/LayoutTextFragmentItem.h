// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutTextFragmentItem_h
#define LayoutTextFragmentItem_h

#include "core/layout/LayoutTextFragment.h"
#include "core/layout/api/LayoutTextItem.h"

namespace blink {

class FirstLetterPseudoElement;

class LayoutTextFragmentItem : public LayoutTextItem {
 public:
  explicit LayoutTextFragmentItem(LayoutTextFragment* layout_text_fragment)
      : LayoutTextItem(layout_text_fragment) {}

  explicit LayoutTextFragmentItem(const LayoutTextItem& item)
      : LayoutTextItem(item) {
    SECURITY_DCHECK(!item || item.IsTextFragment());
  }

  explicit LayoutTextFragmentItem(std::nullptr_t) : LayoutTextItem(nullptr) {}

  LayoutTextFragmentItem() {}

  void SetTextFragment(PassRefPtr<StringImpl> text,
                       unsigned start,
                       unsigned length) {
    ToTextFragment()->SetTextFragment(std::move(text), start, length);
  }

  FirstLetterPseudoElement* GetFirstLetterPseudoElement() const {
    return ToTextFragment()->GetFirstLetterPseudoElement();
  }

 private:
  LayoutTextFragment* ToTextFragment() {
    return ToLayoutTextFragment(GetLayoutObject());
  }
  const LayoutTextFragment* ToTextFragment() const {
    return ToLayoutTextFragment(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LayoutTextFragmentItem_h
