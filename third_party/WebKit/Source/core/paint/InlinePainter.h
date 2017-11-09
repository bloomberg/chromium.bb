// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlinePainter_h
#define InlinePainter_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
struct PaintInfo;
class LayoutInline;

class InlinePainter {
  STACK_ALLOCATED();

 public:
  InlinePainter(const LayoutInline& layout_inline)
      : layout_inline_(layout_inline) {}

  void Paint(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  const LayoutInline& layout_inline_;
};

}  // namespace blink

#endif  // InlinePainter_h
