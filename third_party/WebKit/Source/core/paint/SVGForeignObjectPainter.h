// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGForeignObjectPainter_h
#define SVGForeignObjectPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGForeignObject;

class SVGForeignObjectPainter {
  STACK_ALLOCATED();

 public:
  SVGForeignObjectPainter(
      const LayoutSVGForeignObject& layout_svg_foreign_object)
      : layout_svg_foreign_object_(layout_svg_foreign_object) {}
  void Paint(const PaintInfo&);

 private:
  const LayoutSVGForeignObject& layout_svg_foreign_object_;
};

}  // namespace blink

#endif  // SVGForeignObjectPainter_h
