// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TableCellPainter_h
#define TableCellPainter_h

#include "core/rendering/style/CollapsedBorderValue.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderObject;
class RenderStyle;
class RenderTableCell;

class TableCellPainter {
public:
    TableCellPainter(RenderTableCell& renderTableCell) : m_renderTableCell(renderTableCell) { }

    void paint(PaintInfo&, const LayoutPoint&);

    void paintCollapsedBorders(PaintInfo&, const LayoutPoint&);
    void paintBackgroundsBehindCell(PaintInfo&, const LayoutPoint&, RenderObject* backgroundObject);
    void paintBoxDecorationBackground(PaintInfo&, const LayoutPoint& paintOffset);
    void paintMask(PaintInfo&, const LayoutPoint& paintOffset);

private:
    CollapsedBorderValue cachedCollapsedLeftBorder(const RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedRightBorder(const RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedTopBorder(const RenderStyle*) const;
    CollapsedBorderValue cachedCollapsedBottomBorder(const RenderStyle*) const;

    RenderTableCell& m_renderTableCell;
};

} // namespace blink

#endif // TableCellPainter_h
