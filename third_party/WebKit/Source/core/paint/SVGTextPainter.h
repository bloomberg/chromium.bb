// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTextPainter_h
#define SVGTextPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGText;

class SVGTextPainter {
  STACK_ALLOCATED();

 public:
  SVGTextPainter(const LayoutSVGText& layout_svg_text)
      : layout_svg_text_(layout_svg_text) {}
  void Paint(const PaintInfo&);

 private:
  const LayoutSVGText& layout_svg_text_;
};

}  // namespace blink

#endif  // SVGTextPainter_h
