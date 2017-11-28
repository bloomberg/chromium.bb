// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FramePainter_h
#define FramePainter_h

#include "base/macros.h"
#include "core/paint/PaintPhase.h"
#include "platform/heap/Handle.h"

namespace blink {

class CullRect;
class GraphicsContext;
class IntRect;
class LocalFrameView;
class Scrollbar;

class FramePainter {
  STACK_ALLOCATED();

 public:
  explicit FramePainter(const LocalFrameView& frame_view)
      : frame_view_(&frame_view) {}

  void Paint(GraphicsContext&, const GlobalPaintFlags, const CullRect&);
  void PaintScrollbars(GraphicsContext&, const IntRect&);
  void PaintContents(GraphicsContext&, const GlobalPaintFlags, const IntRect&);
  void PaintScrollCorner(GraphicsContext&, const IntRect& corner_rect);

 private:
  void PaintScrollbar(GraphicsContext&, Scrollbar&, const IntRect&);

  const LocalFrameView& GetFrameView();

  Member<const LocalFrameView> frame_view_;
  static bool in_paint_contents_;

  DISALLOW_COPY_AND_ASSIGN(FramePainter);
};

}  // namespace blink

#endif  // FramePainter_h
