// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TableCellPainter.h"

#include "core/layout/LayoutTableCell.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"

namespace blink {

inline CollapsedBorderValue TableCellPainter::cachedCollapsedLeftBorder(const LayoutStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode()) {
        return styleForCellFlow.isLeftToRightDirection() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSStart)
            : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSEnd);
    }
    return styleForCellFlow.isFlippedBlocksWritingMode() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSAfter)
        : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSBefore);
}

inline CollapsedBorderValue TableCellPainter::cachedCollapsedRightBorder(const LayoutStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode()) {
        return styleForCellFlow.isLeftToRightDirection() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSEnd)
            : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSStart);
    }
    return styleForCellFlow.isFlippedBlocksWritingMode() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSBefore)
        : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSAfter);
}

inline CollapsedBorderValue TableCellPainter::cachedCollapsedTopBorder(const LayoutStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isFlippedBlocksWritingMode() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSAfter) : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSBefore);
    return styleForCellFlow.isLeftToRightDirection() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSStart) : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSEnd);
}

inline CollapsedBorderValue TableCellPainter::cachedCollapsedBottomBorder(const LayoutStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode()) {
        return styleForCellFlow.isFlippedBlocksWritingMode() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSBefore)
            : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSAfter);
    }
    return styleForCellFlow.isLeftToRightDirection() ?  m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSEnd)
        : m_layoutTableCell.section()->cachedCollapsedBorder(&m_layoutTableCell, CBSStart);
}

void TableCellPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase != PaintPhaseCollapsedTableBorders);
    BlockPainter(m_layoutTableCell).paint(paintInfo, paintOffset);
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

void TableCellPainter::paintCollapsedBorders(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhaseCollapsedTableBorders);

    if (!paintInfo.shouldPaintWithinRoot(&m_layoutTableCell) || m_layoutTableCell.style()->visibility() != VISIBLE)
        return;

    LayoutRect paintRect = paintBounds(paintOffset, AddOffsetFromParent);
    if (paintRect.y() - m_layoutTableCell.table()->outerBorderTop() >= paintInfo.rect.maxY())
        return;

    if (paintRect.maxY() + m_layoutTableCell.table()->outerBorderBottom() <= paintInfo.rect.y())
        return;

    if (!m_layoutTableCell.table()->currentBorderValue())
        return;

    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableCell, paintInfo.phase, paintRect);
    if (recorder.canUseCachedDrawing())
        return;

    const LayoutStyle& styleForCellFlow = m_layoutTableCell.styleForCellFlow();
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
        if (border->borderValue.isSameIgnoringColor(*m_layoutTableCell.table()->currentBorderValue())) {
            ObjectPainter::drawLineForBoxSide(graphicsContext, border->x1, border->y1, border->x2, border->y2, border->side,
                border->borderValue.color().resolve(m_layoutTableCell.resolveColor(CSSPropertyColor)), border->style, 0, 0, antialias);
        }
    }
}

void TableCellPainter::paintBackgroundsBehindCell(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutObject* backgroundObject)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_layoutTableCell))
        return;

    if (!backgroundObject)
        return;

    if (m_layoutTableCell.style()->visibility() != VISIBLE)
        return;

    LayoutTable* tableElt = m_layoutTableCell.table();
    if (!tableElt->collapseBorders() && m_layoutTableCell.style()->emptyCells() == HIDE && !m_layoutTableCell.firstChild())
        return;

    Color c = backgroundObject->resolveColor(CSSPropertyBackgroundColor);
    const FillLayer& bgLayer = backgroundObject->style()->backgroundLayers();

    LayoutRect paintRect = paintBounds(paintOffset, backgroundObject != &m_layoutTableCell ? AddOffsetFromParent : DoNotAddOffsetFromParent);

    if (bgLayer.hasImage() || c.alpha()) {
        // We have to clip here because the background would paint
        // on top of the borders otherwise.  This only matters for cells and rows.
        bool shouldClip = backgroundObject->hasLayer() && (backgroundObject == &m_layoutTableCell || backgroundObject == m_layoutTableCell.parent()) && tableElt->collapseBorders();
        GraphicsContextStateSaver stateSaver(*paintInfo.context, shouldClip);
        if (shouldClip) {
            LayoutRect clipRect(paintRect.location(), m_layoutTableCell.size());
            clipRect.expand(m_layoutTableCell.borderInsets());
            paintInfo.context->clip(clipRect);
        }
        BoxPainter(m_layoutTableCell).paintFillLayers(paintInfo, c, bgLayer, paintRect, BackgroundBleedNone, SkXfermode::kSrcOver_Mode, backgroundObject);
    }
}

void TableCellPainter::paintBoxDecorationBackground(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_layoutTableCell))
        return;

    LayoutTable* tableElt = m_layoutTableCell.table();
    if (!tableElt->collapseBorders() && m_layoutTableCell.style()->emptyCells() == HIDE && !m_layoutTableCell.firstChild())
        return;

    LayoutRect paintRect = paintBounds(paintOffset, DoNotAddOffsetFromParent);
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTableCell, DisplayItem::BoxDecorationBackground, pixelSnappedIntRect(paintRect));
    if (recorder.canUseCachedDrawing())
        return;

    BoxPainter::paintBoxShadow(paintInfo, paintRect, m_layoutTableCell.styleRef(), Normal);

    // Paint our cell background.
    paintBackgroundsBehindCell(paintInfo, paintOffset, &m_layoutTableCell);

    BoxPainter::paintBoxShadow(paintInfo, paintRect, m_layoutTableCell.styleRef(), Inset);

    if (!m_layoutTableCell.style()->hasBorder() || tableElt->collapseBorders())
        return;

    BoxPainter::paintBorder(m_layoutTableCell, paintInfo, paintRect, m_layoutTableCell.styleRef());
}

void TableCellPainter::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_layoutTableCell.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutTable* tableElt = m_layoutTableCell.table();
    if (!tableElt->collapseBorders() && m_layoutTableCell.style()->emptyCells() == HIDE && !m_layoutTableCell.firstChild())
        return;

    BoxPainter(m_layoutTableCell).paintMaskImages(paintInfo, paintBounds(paintOffset, DoNotAddOffsetFromParent));
}

LayoutRect TableCellPainter::paintBounds(const LayoutPoint& paintOffset, PaintBoundOffsetBehavior paintBoundOffsetBehavior)
{
    LayoutPoint adjustedPaintOffset = paintOffset;
    if (paintBoundOffsetBehavior == AddOffsetFromParent)
        adjustedPaintOffset.moveBy(m_layoutTableCell.location());
    return LayoutRect(adjustedPaintOffset, LayoutSize(m_layoutTableCell.pixelSnappedSize()));
}

} // namespace blink

