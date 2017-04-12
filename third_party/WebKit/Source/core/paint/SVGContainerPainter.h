// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGContainerPainter_h
#define SVGContainerPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGContainer;

class SVGContainerPainter {
  STACK_ALLOCATED();

 public:
  SVGContainerPainter(const LayoutSVGContainer& layout_svg_container)
      : layout_svg_container_(layout_svg_container) {}

  void Paint(const PaintInfo&);

 private:
  const LayoutSVGContainer& layout_svg_container_;
};

}  // namespace blink

#endif  // SVGContainerPainter_h
