// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGRootInlineBoxPainter_h
#define SVGRootInlineBoxPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class SVGRootInlineBox;

class SVGRootInlineBoxPainter {
  STACK_ALLOCATED();

 public:
  SVGRootInlineBoxPainter(const SVGRootInlineBox& svg_root_inline_box)
      : svg_root_inline_box_(svg_root_inline_box) {}

  void Paint(const PaintInfo&, const LayoutPoint&);

 private:
  const SVGRootInlineBox& svg_root_inline_box_;
};

}  // namespace blink

#endif  // SVGRootInlineBoxPainter_h
