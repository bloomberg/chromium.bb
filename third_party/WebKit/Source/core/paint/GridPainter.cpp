// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/GridPainter.h"

#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderGrid.h"

namespace blink {

static GridSpan dirtiedGridAreas(const Vector<LayoutUnit>& coordinates, LayoutUnit start, LayoutUnit end)
{
    // This function does a binary search over the coordinates.
    // This doesn't work with grid items overflowing their grid areas, but that is managed with m_gridItemsOverflowingGridArea.

    size_t startGridAreaIndex = std::upper_bound(coordinates.begin(), coordinates.end() - 1, start) - coordinates.begin();
    if (startGridAreaIndex > 0)
        --startGridAreaIndex;

    size_t endGridAreaIndex = std::upper_bound(coordinates.begin() + startGridAreaIndex, coordinates.end() - 1, end) - coordinates.begin();
    if (endGridAreaIndex > 0)
        --endGridAreaIndex;

    return GridSpan(startGridAreaIndex, endGridAreaIndex);
}

class GridItemsSorter {
public:
    bool operator()(const std::pair<RenderBox*, size_t>& firstChild, const std::pair<RenderBox*, size_t>& secondChild) const
    {
        if (firstChild.first->style()->order() != secondChild.first->style()->order())
            return firstChild.first->style()->order() < secondChild.first->style()->order();

        return firstChild.second < secondChild.second;
    }
};

void GridPainter::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!m_renderGrid.gridIsDirty());

    LayoutRect localPaintInvalidationRect = paintInfo.rect;
    localPaintInvalidationRect.moveBy(-paintOffset);

    GridSpan dirtiedColumns = dirtiedGridAreas(m_renderGrid.columnPositions(), localPaintInvalidationRect.x(), localPaintInvalidationRect.maxX());
    GridSpan dirtiedRows = dirtiedGridAreas(m_renderGrid.rowPositions(), localPaintInvalidationRect.y(), localPaintInvalidationRect.maxY());

    Vector<std::pair<RenderBox*, size_t> > gridItemsToBePainted;

    for (GridSpan::iterator row = dirtiedRows.begin(); row != dirtiedRows.end(); ++row) {
        for (GridSpan::iterator column = dirtiedColumns.begin(); column != dirtiedColumns.end(); ++column) {
            const Vector<RenderBox*, 1>& children = m_renderGrid.gridCell(row.toInt(), column.toInt());
            for (size_t j = 0; j < children.size(); ++j)
                gridItemsToBePainted.append(std::make_pair(children[j], m_renderGrid.paintIndexForGridItem(children[j])));
        }
    }

    for (Vector<RenderBox*>::const_iterator it = m_renderGrid.itemsOverflowingGridArea().begin(); it != m_renderGrid.itemsOverflowingGridArea().end(); ++it) {
        if ((*it)->frameRect().intersects(localPaintInvalidationRect))
            gridItemsToBePainted.append(std::make_pair(*it, m_renderGrid.paintIndexForGridItem(*it)));
    }

    // Sort grid items following order-modified document order.
    // See http://www.w3.org/TR/css-flexbox/#order-modified-document-order
    std::stable_sort(gridItemsToBePainted.begin(), gridItemsToBePainted.end(), GridItemsSorter());

    RenderBox* previous = 0;
    for (Vector<std::pair<RenderBox*, size_t> >::const_iterator it = gridItemsToBePainted.begin(); it != gridItemsToBePainted.end(); ++it) {
        // We might have duplicates because of spanning children are included in all cells they span.
        // Skip them here to avoid painting items several times.
        RenderBox* current = it->first;
        if (current == previous)
            continue;

        paintChild(current, paintInfo, paintOffset);
        previous = current;
    }
}

void GridPainter::paintChild(RenderBox* child, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint childPoint = m_renderGrid.flipForWritingModeForChild(child, paintOffset);
    if (!child->hasSelfPaintingLayer() && !child->isFloating())
        child->paint(paintInfo, childPoint);
}

} // namespace blink
