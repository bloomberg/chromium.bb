// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutBR_h
#define LineLayoutBR_h

#include "core/layout/LayoutBR.h"
#include "core/layout/api/LineLayoutText.h"

namespace blink {

class LineLayoutBR : public LineLayoutText {
 public:
  explicit LineLayoutBR(LayoutBR* layout_br) : LineLayoutText(layout_br) {}

  explicit LineLayoutBR(const LineLayoutItem& item) : LineLayoutText(item) {
    SECURITY_DCHECK(!item || item.IsBR());
  }

  explicit LineLayoutBR(std::nullptr_t) : LineLayoutText(nullptr) {}

  LineLayoutBR() = default;

  int LineHeight(bool first_line) const {
    return ToBR()->LineHeight(first_line);
  }

 private:
  LayoutBR* ToBR() { return ToLayoutBR(GetLayoutObject()); }

  const LayoutBR* ToBR() const { return ToLayoutBR(GetLayoutObject()); }
};

}  // namespace blink

#endif  // LineLayoutBR_h
