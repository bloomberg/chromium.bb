// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EllipsisBoxPainter_h
#define EllipsisBoxPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;

class EllipsisBox;
class LayoutPoint;
class LayoutUnit;
class ComputedStyle;

class EllipsisBoxPainter {
  STACK_ALLOCATED();

 public:
  EllipsisBoxPainter(const EllipsisBox& ellipsis_box)
      : ellipsis_box_(ellipsis_box) {}

  void Paint(const PaintInfo&,
             const LayoutPoint&,
             LayoutUnit line_top,
             LayoutUnit line_bottom);

 private:
  void PaintEllipsis(const PaintInfo&,
                     const LayoutPoint& paint_offset,
                     LayoutUnit line_top,
                     LayoutUnit line_bottom,
                     const ComputedStyle&);

  const EllipsisBox& ellipsis_box_;
};

}  // namespace blink

#endif  // EllipsisBoxPainter_h
