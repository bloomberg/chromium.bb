// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReplacedPainter_h
#define ReplacedPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutReplaced;

class ReplacedPainter {
  STACK_ALLOCATED();

 public:
  ReplacedPainter(const LayoutReplaced& layout_replaced)
      : layout_replaced_(layout_replaced) {}

  void Paint(const PaintInfo&, const LayoutPoint&);

  // The adjustedPaintOffset should include the location (offset) of the object
  // itself.
  bool ShouldPaint(const PaintInfo&,
                   const LayoutPoint& adjusted_paint_offset) const;

 private:
  bool ShouldAdjustForPaintOffsetTranslation(
      const PaintInfo&,
      const LayoutPoint& paint_offset) const;

  const LayoutReplaced& layout_replaced_;
};

}  // namespace blink

#endif  // ReplacedPainter_h
