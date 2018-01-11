// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutListMarker_h
#define LineLayoutListMarker_h

#include "core/layout/LayoutListMarker.h"
#include "core/layout/api/LineLayoutBox.h"

namespace blink {

class LineLayoutListMarker : public LineLayoutBox {
 public:
  explicit LineLayoutListMarker(LayoutListMarker* layout_list_marker)
      : LineLayoutBox(layout_list_marker) {}

  explicit LineLayoutListMarker(const LineLayoutItem& item)
      : LineLayoutBox(item) {
    SECURITY_DCHECK(!item || item.IsListMarker());
  }

  explicit LineLayoutListMarker(std::nullptr_t) : LineLayoutBox(nullptr) {}

  LineLayoutListMarker() = default;

  bool IsInside() const { return ToListMarker()->IsInside(); }

 private:
  LayoutListMarker* ToListMarker() {
    return ToLayoutListMarker(GetLayoutObject());
  }

  const LayoutListMarker* ToListMarker() const {
    return ToLayoutListMarker(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutListMarker_h
