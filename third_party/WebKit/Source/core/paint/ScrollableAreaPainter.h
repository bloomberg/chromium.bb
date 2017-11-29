// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollableAreaPainter_h
#define ScrollableAreaPainter_h

#include "base/macros.h"
#include "platform/heap/Handle.h"

namespace blink {

class CullRect;
class DisplayItemClient;
class GraphicsContext;
class IntPoint;
class IntRect;
class PaintLayerScrollableArea;

class ScrollableAreaPainter {
  STACK_ALLOCATED();

 public:
  explicit ScrollableAreaPainter(
      PaintLayerScrollableArea& paint_layer_scrollable_area)
      : scrollable_area_(&paint_layer_scrollable_area) {}

  void PaintResizer(GraphicsContext&,
                    const IntPoint& paint_offset,
                    const CullRect&);
  void PaintOverflowControls(GraphicsContext&,
                             const IntPoint& paint_offset,
                             const CullRect&,
                             bool painting_overlay_controls);
  void PaintScrollCorner(GraphicsContext&,
                         const IntPoint& paint_offset,
                         const CullRect&);

 private:
  void DrawPlatformResizerImage(GraphicsContext&, IntRect resizer_corner_rect);
  bool OverflowControlsIntersectRect(const CullRect&) const;

  PaintLayerScrollableArea& GetScrollableArea() const;
  const DisplayItemClient& DisplayItemClientForCorner() const;

  Member<PaintLayerScrollableArea> scrollable_area_;

  DISALLOW_COPY_AND_ASSIGN(ScrollableAreaPainter);
};

}  // namespace blink

#endif  // ScrollableAreaPainter_h
