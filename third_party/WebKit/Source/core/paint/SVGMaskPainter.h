// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGMaskPainter_h
#define SVGMaskPainter_h

#include "platform/geometry/FloatRect.h"
#include "wtf/Allocator.h"

namespace blink {

class GraphicsContext;
class LayoutObject;
class LayoutSVGResourceMasker;

class SVGMaskPainter {
  STACK_ALLOCATED();

 public:
  SVGMaskPainter(LayoutSVGResourceMasker& mask) : mask_(mask) {}

  bool PrepareEffect(const LayoutObject&, GraphicsContext&);
  void FinishEffect(const LayoutObject&, GraphicsContext&);

 private:
  void DrawMaskForLayoutObject(GraphicsContext&,
                               const LayoutObject&,
                               const FloatRect& target_bounding_box,
                               const FloatRect& target_visual_rect);

  LayoutSVGResourceMasker& mask_;
};

}  // namespace blink

#endif  // SVGMaskPainter_h
