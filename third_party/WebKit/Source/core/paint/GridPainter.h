// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridPainter_h
#define GridPainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutGrid;

class GridPainter {
  STACK_ALLOCATED();

 public:
  GridPainter(const LayoutGrid& layout_grid) : layout_grid_(layout_grid) {}

  void PaintChildren(const PaintInfo&, const LayoutPoint&);

 private:
  const LayoutGrid& layout_grid_;
};

}  // namespace blink

#endif  // GridPainter_h
