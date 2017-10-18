// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPainterBase_h
#define ObjectPainterBase_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ComputedStyle;
class Color;
class DisplayItemClient;
class GraphicsContext;
struct PaintInfo;

// Base class for object painting. Has no dependencies on the layout tree and
// thus provides functionality and definitions that can be shared between both
// legacy layout and LayoutNG.
class ObjectPainterBase {
  STACK_ALLOCATED();

 public:
  static void DrawLineForBoxSide(GraphicsContext&,
                                 float x1,
                                 float y1,
                                 float x2,
                                 float y2,
                                 BoxSide,
                                 Color,
                                 EBorderStyle,
                                 int adjbw1,
                                 int adjbw2,
                                 bool antialias = false);

 protected:
  ObjectPainterBase() {}
  void PaintOutlineRects(const PaintInfo&,
                         const Vector<LayoutRect>&,
                         const ComputedStyle&,
                         const DisplayItemClient&);
};

}  // namespace blink

#endif  // ObjectPainterBase_h
