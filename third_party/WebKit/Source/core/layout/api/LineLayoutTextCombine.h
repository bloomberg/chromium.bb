// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutTextCombine_h
#define LineLayoutTextCombine_h

#include "core/layout/LayoutTextCombine.h"
#include "core/layout/api/LineLayoutText.h"

namespace blink {

class LineLayoutTextCombine : public LineLayoutText {
 public:
  explicit LineLayoutTextCombine(LayoutTextCombine* layout_text_combine)
      : LineLayoutText(layout_text_combine) {}

  explicit LineLayoutTextCombine(const LineLayoutItem& item)
      : LineLayoutText(item) {
    SECURITY_DCHECK(!item || item.IsCombineText());
  }

  explicit LineLayoutTextCombine(std::nullptr_t) : LineLayoutText(nullptr) {}

  LineLayoutTextCombine() = default;

  bool IsCombined() const { return ToTextCombine()->IsCombined(); }

 private:
  LayoutTextCombine* ToTextCombine() {
    return ToLayoutTextCombine(GetLayoutObject());
  }

  const LayoutTextCombine* ToTextCombine() const {
    return ToLayoutTextCombine(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutTextCombine_h
