// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInlineFlowBoxPainter_h
#define SVGInlineFlowBoxPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class SVGInlineFlowBox;

class SVGInlineFlowBoxPainter {
  STACK_ALLOCATED();

 public:
  SVGInlineFlowBoxPainter(const SVGInlineFlowBox& svg_inline_flow_box)
      : svg_inline_flow_box_(svg_inline_flow_box) {}

  void PaintSelectionBackground(const PaintInfo&);
  void Paint(const PaintInfo&, const LayoutPoint&);

 private:
  const SVGInlineFlowBox& svg_inline_flow_box_;
};

}  // namespace blink

#endif  // SVGInlineFlowBoxPainter_h
