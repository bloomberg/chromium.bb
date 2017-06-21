// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableCellPainter_h
#define TableCellPainter_h

#include "platform/graphics/paint/DisplayItem.h"
#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutRect;
class LayoutTableCell;
class LayoutObject;

class TableCellPainter {
  STACK_ALLOCATED();

 public:
  TableCellPainter(const LayoutTableCell& layout_table_cell)
      : layout_table_cell_(layout_table_cell) {}

  void Paint(const PaintInfo&, const LayoutPoint&);

  void PaintContainerBackgroundBehindCell(
      const PaintInfo&,
      const LayoutPoint&,
      const LayoutObject& background_object);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint& paint_offset);
  void PaintMask(const PaintInfo&, const LayoutPoint& paint_offset);

 private:
  const DisplayItemClient& DisplayItemClientForBorders() const;
  LayoutRect PaintRectNotIncludingVisualOverflow(
      const LayoutPoint& paint_offset);
  void PaintBackground(const PaintInfo&,
                       const LayoutRect&,
                       const LayoutObject& background_object);

  const LayoutTableCell& layout_table_cell_;
};

}  // namespace blink

#endif  // TableCellPainter_h
