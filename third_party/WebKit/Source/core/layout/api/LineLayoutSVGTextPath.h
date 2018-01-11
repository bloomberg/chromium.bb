// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutSVGTextPath_h
#define LineLayoutSVGTextPath_h

#include "core/layout/api/LineLayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGTextPath.h"
#include <memory>

namespace blink {

class LineLayoutSVGTextPath : public LineLayoutSVGInline {
 public:
  explicit LineLayoutSVGTextPath(LayoutSVGTextPath* layout_svg_text_path)
      : LineLayoutSVGInline(layout_svg_text_path) {}

  explicit LineLayoutSVGTextPath(const LineLayoutItem& item)
      : LineLayoutSVGInline(item) {
    SECURITY_DCHECK(!item || item.IsSVGTextPath());
  }

  explicit LineLayoutSVGTextPath(std::nullptr_t)
      : LineLayoutSVGInline(nullptr) {}

  LineLayoutSVGTextPath() = default;

  std::unique_ptr<PathPositionMapper> LayoutPath() const {
    return ToSVGTextPath()->LayoutPath();
  }

  float CalculateStartOffset(float path_length) const {
    return ToSVGTextPath()->CalculateStartOffset(path_length);
  }

 private:
  LayoutSVGTextPath* ToSVGTextPath() {
    return ToLayoutSVGTextPath(GetLayoutObject());
  }

  const LayoutSVGTextPath* ToSVGTextPath() const {
    return ToLayoutSVGTextPath(GetLayoutObject());
  }
};

}  // namespace blink

#endif  // LineLayoutSVGTextPath_h
