// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutRubyBase_h
#define LineLayoutRubyBase_h

#include "core/layout/LayoutRubyBase.h"
#include "core/layout/api/LineLayoutBlockFlow.h"

namespace blink {

class LineLayoutRubyBase : public LineLayoutBlockFlow {
 public:
  explicit LineLayoutRubyBase(LayoutRubyBase* layout_ruby_base)
      : LineLayoutBlockFlow(layout_ruby_base) {}

  explicit LineLayoutRubyBase(const LineLayoutItem& item)
      : LineLayoutBlockFlow(item) {
    SECURITY_DCHECK(!item || item.IsRubyBase());
  }

  explicit LineLayoutRubyBase(std::nullptr_t) : LineLayoutBlockFlow(nullptr) {}

  LineLayoutRubyBase() = default;

 private:
  LayoutRubyBase* ToRubyBase() { return ToLayoutRubyBase(GetLayoutObject()); }

  const LayoutRubyBase* ToRubyBase() const {
    return ToLayoutRubyBase(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutRubyBase_h
