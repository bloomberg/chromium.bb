// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollbarPainter_h
#define ScrollbarPainter_h

#include "base/macros.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/Scrollbar.h"

namespace blink {

class GraphicsContext;
class IntRect;
class LayoutPoint;
class LayoutRect;
class LayoutScrollbar;
class LayoutScrollbarPart;

class ScrollbarPainter {
  STACK_ALLOCATED();

 public:
  explicit ScrollbarPainter(const LayoutScrollbar& layout_scrollbar)
      : layout_scrollbar_(&layout_scrollbar) {}

  void PaintPart(GraphicsContext&, ScrollbarPart, const IntRect&);
  static void PaintIntoRect(const LayoutScrollbarPart&,
                            GraphicsContext&,
                            const LayoutPoint& paint_offset,
                            const LayoutRect&);

 private:
  Member<const LayoutScrollbar> layout_scrollbar_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarPainter);
};

}  // namespace blink

#endif  // ScrollbarPainter_h
