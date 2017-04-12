// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RootInlineBoxPainter_h
#define RootInlineBoxPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutUnit;
class RootInlineBox;

class RootInlineBoxPainter {
  STACK_ALLOCATED();

 public:
  RootInlineBoxPainter(const RootInlineBox& root_inline_box)
      : root_inline_box_(root_inline_box) {}

  void Paint(const PaintInfo&,
             const LayoutPoint&,
             LayoutUnit line_top,
             LayoutUnit line_bottom);

 private:
  void PaintEllipsisBox(const PaintInfo&,
                        const LayoutPoint& paint_offset,
                        LayoutUnit line_top,
                        LayoutUnit line_bottom) const;

  const RootInlineBox& root_inline_box_;
};

}  // namespace blink

#endif  // RootInlineBoxPainter_h
