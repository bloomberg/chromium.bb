// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TableCellPainter.h"

#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/DrawingRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderTableCell.h"

namespace blink {

inline CollapsedBorderValue TableCellPainter::cachedCollapsedLeftBorder(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode()) {
        return styleForCellFlow->isLeftToRightDirection() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSStart)
            : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSEnd);
    }
    return styleForCellFlow->slowIsFlippedBlocksWritingMode() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSAfter)
        : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSBefore);
}

inline CollapsedBorderValue TableCellPainter::cachedCollapsedRightBorder(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode()) {
        return styleForCellFlow->isLeftToRightDirection() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSEnd)
            : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSStart);
    }
    return styleForCellFlow->slowIsFlippedBlocksWritingMode() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSBefore)
        : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSAfter);
}

inline CollapsedBorderValue TableCellPainter::cachedCollapsedTopBorder(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode())
        return styleForCellFlow->slowIsFlippedBlocksWritingMode() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSAfter) : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSBefore);
    return styleForCellFlow->isLeftToRightDirection() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSStart) : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSEnd);
}

inline CollapsedBorderValue TableCellPainter::cachedCollapsedBottomBorder(const RenderStyle* styleForCellFlow) const
{
    if (styleForCellFlow->isHorizontalWritingMode()) {
        return styleForCellFlow->slowIsFlippedBlocksWritingMode() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSBefore)
            : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSAfter);
    }
    return styleForCellFlow->isLeftToRightDirection() ? m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSEnd)
        : m_renderTableCell.section()->cachedCollapsedBorder(&m_renderTableCell, CBSStart);
}

void TableCellPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase != PaintPhaseCollapsedTableBorders);
    BlockPainter(m_renderTableCell).paint(paintInfo, paintOffset);
}

struct CollapsedBorder {
    CollapsedBorderValue borderValue;
    BoxSide side;
    bool shouldPaint;
    int x1;
    int y1;
    int x2;
    int y2;
    EBorderStyle style;
};

class CollapsedBorders {
public:
    CollapsedBorders()
        : m_count(0)
    {
    }

    void addBorder(const CollapsedBorderValue& borderValue, BoxSide borderSide, bool shouldPaint, int x1, int y1, int x2, int y2, EBorderStyle borderStyle)
    {
        if (borderValue.exists() && shouldPaint) {
            m_borders[m_count].borderValue = borderValue;
            m_borders[m_count].side = borderSide;
            m_borders[m_count].shouldPaint = shouldPaint;
            m_borders[m_count].x1 = x1;
            m_borders[m_count].x2 = x2;
            m_borders[m_count].y1 = y1;
            m_borders[m_count].y2 = y2;
            m_borders[m_count].style = borderStyle;
            m_count++;
        }
    }

    CollapsedBorder* nextBorder()
    {
        for (unsigned i = 0; i < m_count; i++) {
            if (m_borders[i].borderValue.exists() && m_borders[i].shouldPaint) {
                m_borders[i].shouldPaint = false;
                return &m_borders[i];
            }
        }

        return 0;
    }

    CollapsedBorder m_borders[4];
    unsigned m_count;
};

static EBorderStyle collapsedBorderStyle(EBorderStyle style)
{
    if (style == OUTSET)
        return GROOVE;
    if (style == INSET)
        return RIDGE;
    return style;
}

void TableCellPainter::paintCollapsedBorders(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseCollapsedTableBorders);

    if (!paintInfo.shouldPaintWithinRoot(&m_renderTableCell) || m_renderTableCell.style()->visibility() != VISIBLE)
        return;

    LayoutRect paintRect = LayoutRect(paintOffset + m_renderTableCell.location(), m_renderTableCell.pixelSnappedSize());
    if (paintRect.y() - m_renderTableCell.table()->outerBorderTop() >= paintInfo.rect.maxY())
        return;

    if (paintRect.maxY() + m_renderTableCell.table()->outerBorderBottom() <= paintInfo.rect.y())
        return;

    if (!m_renderTableCell.table()->currentBorderValue())
        return;

    const RenderStyle* styleForCellFlow = m_renderTableCell.styleForCellFlow();
    CollapsedBorderValue leftVal = cachedCollapsedLeftBorder(styleForCellFlow);
    CollapsedBorderValue rightVal = cachedCollapsedRightBorder(styleForCellFlow);
    CollapsedBorderValue topVal = cachedCollapsedTopBorder(styleForCellFlow);
    CollapsedBorderValue bottomVal = cachedCollapsedBottomBorder(styleForCellFlow);

    // Adjust our x/y/width/height so that we paint the collapsed borders at the correct location.
    int topWidth = topVal.width();
    int bottomWidth = bottomVal.width();
    int leftWidth = leftVal.width();
    int rightWidth = rightVal.width();

    IntRect borderRect = pixelSnappedIntRect(paintRect.x() - leftWidth / 2,
        paintRect.y() - topWidth / 2,
        paintRect.width() + leftWidth / 2 + (rightWidth + 1) / 2,
        paintRect.height() + topWidth / 2 + (bottomWidth + 1) / 2);

    EBorderStyle topStyle = collapsedBorderStyle(topVal.style());
    EBorderStyle bottomStyle = collapsedBorderStyle(bottomVal.style());
    EBorderStyle leftStyle = collapsedBorderStyle(leftVal.style());
    EBorderStyle rightStyle = collapsedBorderStyle(rightVal.style());

    bool renderTop = topStyle > BHIDDEN && !topVal.isTransparent();
    bool renderBottom = bottomStyle > BHIDDEN && !bottomVal.isTransparent();
    bool renderLeft = leftStyle > BHIDDEN && !leftVal.isTransparent();
    bool renderRight = rightStyle > BHIDDEN && !rightVal.isTransparent();

    // We never paint diagonals at the joins.  We simply let the border with the highest
    // precedence paint on top of borders with lower precedence.
    CollapsedBorders borders;
    borders.addBorder(topVal, BSTop, renderTop, borderRect.x(), borderRect.y(), borderRect.maxX(), borderRect.y() + topWidth, topStyle);
    borders.addBorder(bottomVal, BSBottom, renderBottom, borderRect.x(), borderRect.maxY() - bottomWidth, borderRect.maxX(), borderRect.maxY(), bottomStyle);
    borders.addBorder(leftVal, BSLeft, renderLeft, borderRect.x(), borderRect.y(), borderRect.x() + leftWidth, borderRect.maxY(), leftStyle);
    borders.addBorder(rightVal, BSRight, renderRight, borderRect.maxX() - rightWidth, borderRect.y(), borderRect.maxX(), borderRect.maxY(), rightStyle);

    GraphicsContext* graphicsContext = paintInfo.context;
    bool antialias = BoxPainter::shouldAntialiasLines(graphicsContext);

    for (CollapsedBorder* border = borders.nextBorder(); border; border = borders.nextBorder()) {
        if (border->borderValue.isSameIgnoringColor(*m_renderTableCell.table()->currentBorderValue())) {
            ObjectPainter::drawLineForBoxSide(graphicsContext, border->x1, border->y1, border->x2, border->y2, border->side,
                border->borderValue.color().resolve(m_renderTableCell.style()->visitedDependentColor(CSSPropertyColor)), border->style, 0, 0, antialias);
        }
    }
}

void TableCellPainter::paintBackgroundsBehindCell(PaintInfo& paintInfo, const LayoutPoint& paintOffset, RenderObject* backgroundObject)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_renderTableCell))
        return;

    if (!backgroundObject)
        return;

    if (m_renderTableCell.style()->visibility() != VISIBLE)
        return;

    RenderTable* tableElt = m_renderTableCell.table();
    if (!tableElt->collapseBorders() && m_renderTableCell.style()->emptyCells() == HIDE && !m_renderTableCell.firstChild())
        return;

    LayoutPoint adjustedPaintOffset = paintOffset;
    if (backgroundObject != &m_renderTableCell)
        adjustedPaintOffset.moveBy(m_renderTableCell.location());

    Color c = backgroundObject->resolveColor(CSSPropertyBackgroundColor);
    const FillLayer& bgLayer = backgroundObject->style()->backgroundLayers();

    if (bgLayer.hasImage() || c.alpha()) {
        // We have to clip here because the background would paint
        // on top of the borders otherwise.  This only matters for cells and rows.
        bool shouldClip = backgroundObject->hasLayer() && (backgroundObject == &m_renderTableCell || backgroundObject == m_renderTableCell.parent()) && tableElt->collapseBorders();
        GraphicsContextStateSaver stateSaver(*paintInfo.context, shouldClip);
        if (shouldClip) {
            LayoutRect clipRect(adjustedPaintOffset.x() + m_renderTableCell.borderLeft(), adjustedPaintOffset.y() + m_renderTableCell.borderTop(),
                m_renderTableCell.width() - m_renderTableCell.borderLeft() - m_renderTableCell.borderRight(),
                m_renderTableCell.height() - m_renderTableCell.borderTop() - m_renderTableCell.borderBottom());
            paintInfo.context->clip(clipRect);
        }
        BoxPainter(m_renderTableCell).paintFillLayers(paintInfo, c, bgLayer, LayoutRect(adjustedPaintOffset, m_renderTableCell.pixelSnappedSize()), BackgroundBleedNone, CompositeSourceOver, backgroundObject);
    }
}

void TableCellPainter::paintBoxDecorationBackground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_renderTableCell))
        return;

    RenderTable* tableElt = m_renderTableCell.table();
    if (!tableElt->collapseBorders() && m_renderTableCell.style()->emptyCells() == HIDE && !m_renderTableCell.firstChild())
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, m_renderTableCell.pixelSnappedSize());
    DrawingRecorder recorder(paintInfo.context, &m_renderTableCell, paintInfo.phase, pixelSnappedIntRect(paintRect));
    BoxPainter::paintBoxShadow(paintInfo, paintRect, m_renderTableCell.style(), Normal);

    // Paint our cell background.
    paintBackgroundsBehindCell(paintInfo, paintOffset, &m_renderTableCell);

    BoxPainter::paintBoxShadow(paintInfo, paintRect, m_renderTableCell.style(), Inset);

    if (!m_renderTableCell.style()->hasBorder() || tableElt->collapseBorders())
        return;

    BoxPainter::paintBorder(m_renderTableCell, paintInfo, paintRect, m_renderTableCell.style());
}

void TableCellPainter::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderTableCell.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    RenderTable* tableElt = m_renderTableCell.table();
    if (!tableElt->collapseBorders() && m_renderTableCell.style()->emptyCells() == HIDE && !m_renderTableCell.firstChild())
        return;

    BoxPainter(m_renderTableCell).paintMaskImages(paintInfo, LayoutRect(paintOffset, m_renderTableCell.pixelSnappedSize()));
}

} // namespace blink

