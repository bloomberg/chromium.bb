// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TablePainter_h
#define TablePainter_h

#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutPoint;
class LayoutTable;
struct PaintInfo;

class TablePainter {
  STACK_ALLOCATED();

 public:
  TablePainter(const LayoutTable& layout_table) : layout_table_(layout_table) {}

  void PaintObject(const PaintInfo&, const LayoutPoint&);
  void PaintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
  void PaintMask(const PaintInfo&, const LayoutPoint&);

 private:
  void PaintCollapsedBorders(const PaintInfo&, const LayoutPoint&);

  const LayoutTable& layout_table_;
};

}  // namespace blink

#endif  // TablePainter_h
