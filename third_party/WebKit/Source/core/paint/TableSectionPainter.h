// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableSectionPainter_h
#define TableSectionPainter_h

#include "core/paint/PaintPhase.h"
#include "core/style/ShadowData.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CellSpan;
class LayoutPoint;
class LayoutTableCell;
class LayoutTableSection;
struct PaintInfo;

class TableSectionPainter {
  STACK_ALLOCATED();

 public:
  TableSectionPainter(const LayoutTableSection& layout_table_section)
      : layout_table_section_(layout_table_section) {}

  void Paint(const PaintInfo&, const LayoutPoint&);
  void PaintCollapsedBorders(const PaintInfo&, const LayoutPoint&);

 private:
  void PaintObject(const PaintInfo&, const LayoutPoint&);

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const LayoutPoint&,
                                    const CellSpan& dirtied_rows,
                                    const CellSpan& dirtied_columns);
  void PaintBackgroundsBehindCell(const LayoutTableCell&,
                                  const PaintInfo&,
                                  const LayoutPoint&);
  void PaintCell(const LayoutTableCell&, const PaintInfo&, const LayoutPoint&);

  enum ItemToPaint { kPaintCollapsedBorders, kPaintSection };
  void PaintRepeatingHeaderGroup(
      const PaintInfo&,
      const LayoutPoint& paint_offset,
      ItemToPaint);
  void PaintSection(const PaintInfo&, const LayoutPoint&);
  void PaintCollapsedSectionBorders(const PaintInfo&, const LayoutPoint&);

  const LayoutTableSection& layout_table_section_;
};

}  // namespace blink

#endif  // TableSectionPainter_h
