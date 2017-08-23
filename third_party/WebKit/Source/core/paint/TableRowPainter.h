// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableRowPainter_h
#define TableRowPainter_h

#include "core/style/ShadowData.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CellSpan;
class LayoutPoint;
class LayoutTableCell;
class LayoutTableRow;
struct PaintInfo;

class TableRowPainter {
  STACK_ALLOCATED();

 public:
  TableRowPainter(const LayoutTableRow& layout_table_row)
      : layout_table_row_(layout_table_row) {}

  void Paint(const PaintInfo&, const LayoutPoint&);
  void PaintOutline(const PaintInfo&, const LayoutPoint&);
  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint&,
                                    const CellSpan& dirtied_columns);
  void PaintCollapsedBorders(const PaintInfo&,
                             const LayoutPoint&,
                             const CellSpan& dirtied_columns);

 private:
  void PaintBackgroundBehindCell(const LayoutTableCell&,
                                 const PaintInfo&,
                                 const LayoutPoint&);

  void HandleChangedPartialPaint(const PaintInfo&,
                                 const CellSpan& dirtied_columns);

  const LayoutTableRow& layout_table_row_;
};

}  // namespace blink

#endif  // TableRowPainter_h
