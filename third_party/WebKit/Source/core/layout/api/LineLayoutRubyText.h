// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutRubyText_h
#define LineLayoutRubyText_h

#include "core/layout/LayoutRubyText.h"
#include "core/layout/api/LineLayoutBlockFlow.h"

namespace blink {

class LineLayoutRubyText : public LineLayoutBlockFlow {
 public:
  explicit LineLayoutRubyText(LayoutRubyText* layout_ruby_text)
      : LineLayoutBlockFlow(layout_ruby_text) {}

  explicit LineLayoutRubyText(const LineLayoutItem& item)
      : LineLayoutBlockFlow(item) {
    SECURITY_DCHECK(!item || item.IsRubyText());
  }

  explicit LineLayoutRubyText(std::nullptr_t) : LineLayoutBlockFlow(nullptr) {}

  LineLayoutRubyText() = default;

 private:
  LayoutRubyText* ToRubyText() { return ToLayoutRubyText(GetLayoutObject()); }

  const LayoutRubyText* ToRubyText() const {
    return ToLayoutRubyText(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutRubyText_h
