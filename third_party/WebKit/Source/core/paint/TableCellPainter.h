// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableCellPainter_h
#define TableCellPainter_h

#include "core/style/CollapsedBorderValue.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class CollapsedBorderValue;
class LayoutPoint;
class LayoutRect;
class LayoutTableCell;
class LayoutObject;
class ComputedStyle;

class TableCellPainter {
    STACK_ALLOCATED();
public:
    TableCellPainter(const LayoutTableCell& layoutTableCell) : m_layoutTableCell(layoutTableCell) { }

    void paint(const PaintInfo&, const LayoutPoint&);

    void paintCollapsedBorders(const PaintInfo&, const LayoutPoint&, const CollapsedBorderValue&);
    void paintBackgroundsBehindCell(const PaintInfo&, const LayoutPoint&, const LayoutObject* backgroundObject, DisplayItem::Type);
    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint& paintOffset);
    void paintMask(const PaintInfo&, const LayoutPoint& paintOffset);

    enum PaintBoundOffsetBehavior { AddOffsetFromParent, DoNotAddOffsetFromParent };
    // Returns the bonds of the table cell for painting, offset by paintOffset, and if desired, the offset from the cell
    // to its parent.
    LayoutRect paintBounds(const LayoutPoint& paintOffset, PaintBoundOffsetBehavior);

private:
    const CollapsedBorderValue& cachedCollapsedLeftBorder(const ComputedStyle&) const;
    const CollapsedBorderValue& cachedCollapsedRightBorder(const ComputedStyle&) const;
    const CollapsedBorderValue& cachedCollapsedTopBorder(const ComputedStyle&) const;
    const CollapsedBorderValue& cachedCollapsedBottomBorder(const ComputedStyle&) const;

    const LayoutTableCell& m_layoutTableCell;
};

} // namespace blink

#endif // TableCellPainter_h
