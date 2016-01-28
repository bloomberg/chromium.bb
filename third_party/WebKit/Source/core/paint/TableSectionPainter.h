// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableSectionPainter_h
#define TableSectionPainter_h

#include "core/paint/PaintPhase.h"
#include "wtf/Allocator.h"

namespace blink {

class CollapsedBorderValue;
class LayoutPoint;
class LayoutTableCell;
class LayoutTableSection;
struct PaintInfo;

class TableSectionPainter {
    STACK_ALLOCATED();
public:
    TableSectionPainter(const LayoutTableSection& layoutTableSection) : m_layoutTableSection(layoutTableSection) { }

    void paint(const PaintInfo&, const LayoutPoint&);
    void paintCollapsedBorders(const PaintInfo&, const LayoutPoint&, const CollapsedBorderValue&);

private:
    void paintObject(const PaintInfo&, const LayoutPoint&);
    void paintCell(const LayoutTableCell&, PaintPhase originalPaintPhase, const PaintInfo&, const LayoutPoint&);

    const LayoutTableSection& m_layoutTableSection;
};

} // namespace blink

#endif // TableSectionPainter_h
