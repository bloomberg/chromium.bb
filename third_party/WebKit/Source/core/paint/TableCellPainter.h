// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableCellPainter_h
#define TableCellPainter_h

#include "core/rendering/style/CollapsedBorderValue.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class LayoutRect;
class RenderObject;
class RenderStyle;
class RenderTableCell;

class TableCellPainter {
public:
    TableCellPainter(RenderTableCell& renderTableCell) : m_renderTableCell(renderTableCell) { }

    void paint(const PaintInfo&, const LayoutPoint&);

    void paintCollapsedBorders(const PaintInfo&, const LayoutPoint&);
    void paintBackgroundsBehindCell(const PaintInfo&, const LayoutPoint&, RenderObject* backgroundObject);
    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint& paintOffset);
    void paintMask(const PaintInfo&, const LayoutPoint& paintOffset);

    enum PaintBoundOffsetBehavior { AddOffsetFromParent, DoNotAddOffsetFromParent };
    // Returns the bonds of the table cell for painting, offset by paintOffset, and if desired, the offset from the cell
    // to its parent.
    LayoutRect paintBounds(const LayoutPoint& paintOffset, PaintBoundOffsetBehavior);

private:
    CollapsedBorderValue cachedCollapsedLeftBorder(const RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedRightBorder(const RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedTopBorder(const RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedBottomBorder(const RenderStyle*) const;

    RenderTableCell& m_renderTableCell;
};

} // namespace blink

#endif // TableCellPainter_h
