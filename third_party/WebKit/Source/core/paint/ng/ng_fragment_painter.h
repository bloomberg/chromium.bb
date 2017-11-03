// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ng_fragment_painter_h
#define ng_fragment_painter_h

#include "core/paint/ObjectPainterBase.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
class NGPaintFragment;
struct PaintInfo;

// Generic fragment painter for paint logic shared between all types of
// fragments. LayoutNG version of ObjectPainter, based on ObjectPainterBase.
class NGFragmentPainter : public ObjectPainterBase {
  STACK_ALLOCATED();

 public:
  NGFragmentPainter(const NGPaintFragment& paint_fragment)
      : paint_fragment_(paint_fragment) {}

  void PaintOutline(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  const NGPaintFragment& paint_fragment_;
};

}  // namespace blink

#endif
