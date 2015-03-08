// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FrameSetPainter.h"

#include "core/html/HTMLFrameSetElement.h"
#include "core/layout/LayoutFrameSet.h"
#include "core/layout/PaintInfo.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"

namespace blink {

static Color borderStartEdgeColor()
{
    return Color(170, 170, 170);
}

static Color borderEndEdgeColor()
{
    return Color::black;
}

static Color borderFillColor()
{
    return Color(208, 208, 208);
}

void FrameSetPainter::paintColumnBorder(const PaintInfo& paintInfo, const IntRect& borderRect)
{
    if (!paintInfo.rect.intersects(borderRect))
        return;

    // FIXME: We should do something clever when borders from distinct framesets meet at a join.

    // Fill first.
    GraphicsContext* context = paintInfo.context;
    context->fillRect(borderRect, m_layoutFrameSet.frameSet()->hasBorderColor() ? m_layoutFrameSet.resolveColor(CSSPropertyBorderLeftColor) : borderFillColor());

    // Now stroke the edges but only if we have enough room to paint both edges with a little
    // bit of the fill color showing through.
    if (borderRect.width() >= 3) {
        context->fillRect(IntRect(borderRect.location(), IntSize(1, m_layoutFrameSet.size().height())), borderStartEdgeColor());
        context->fillRect(IntRect(IntPoint(borderRect.maxX() - 1, borderRect.y()), IntSize(1, m_layoutFrameSet.size().height())), borderEndEdgeColor());
    }
}

void FrameSetPainter::paintRowBorder(const PaintInfo& paintInfo, const IntRect& borderRect)
{
    // FIXME: We should do something clever when borders from distinct framesets meet at a join.

    // Fill first.
    GraphicsContext* context = paintInfo.context;
    context->fillRect(borderRect, m_layoutFrameSet.frameSet()->hasBorderColor() ? m_layoutFrameSet.resolveColor(CSSPropertyBorderLeftColor) : borderFillColor());

    // Now stroke the edges but only if we have enough room to paint both edges with a little
    // bit of the fill color showing through.
    if (borderRect.height() >= 3) {
        context->fillRect(IntRect(borderRect.location(), IntSize(m_layoutFrameSet.size().width(), 1)), borderStartEdgeColor());
        context->fillRect(IntRect(IntPoint(borderRect.x(), borderRect.maxY() - 1), IntSize(m_layoutFrameSet.size().width(), 1)), borderEndEdgeColor());
    }
}

void FrameSetPainter::paintBorders(const PaintInfo& paintInfo, const LayoutPoint& adjustedPaintOffset)
{
    LayoutRect adjustedFrameRect(adjustedPaintOffset, m_layoutFrameSet.size());
    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutFrameSet, paintInfo.phase, adjustedFrameRect);
    if (recorder.canUseCachedDrawing())
        return;

    LayoutObject* child = m_layoutFrameSet.firstChild();
    size_t rows = m_layoutFrameSet.rows().m_sizes.size();
    size_t cols = m_layoutFrameSet.columns().m_sizes.size();
    LayoutUnit borderThickness = m_layoutFrameSet.frameSet()->border();
    LayoutUnit yPos = 0;
    for (size_t r = 0; r < rows; r++) {
        LayoutUnit xPos = 0;
        for (size_t c = 0; c < cols; c++) {
            xPos += m_layoutFrameSet.columns().m_sizes[c];
            if (borderThickness && m_layoutFrameSet.columns().m_allowBorder[c + 1]) {
                paintColumnBorder(paintInfo, pixelSnappedIntRect(
                    LayoutRect(adjustedPaintOffset.x() + xPos, adjustedPaintOffset.y() + yPos, borderThickness, m_layoutFrameSet.size().height())));
                xPos += borderThickness;
            }
            child = child->nextSibling();
            if (!child)
                return;
        }
        yPos += m_layoutFrameSet.rows().m_sizes[r];
        if (borderThickness && m_layoutFrameSet.rows().m_allowBorder[r + 1]) {
            paintRowBorder(paintInfo, pixelSnappedIntRect(
                LayoutRect(adjustedPaintOffset.x(), adjustedPaintOffset.y() + yPos, m_layoutFrameSet.size().width(), borderThickness)));
            yPos += borderThickness;
        }
    }
}

void FrameSetPainter::paintChildren(const PaintInfo& paintInfo, const LayoutPoint& adjustedPaintOffset)
{
    // Paint only those children that fit in the grid.
    // Remaining frames are "hidden".
    // See also LayoutFrameSet::positionFrames.
    LayoutObject* child = m_layoutFrameSet.firstChild();
    size_t rows = m_layoutFrameSet.rows().m_sizes.size();
    size_t cols = m_layoutFrameSet.columns().m_sizes.size();
    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            child->paint(paintInfo, adjustedPaintOffset);
            child = child->nextSibling();
            if (!child)
                return;
        }
    }
}

void FrameSetPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutFrameSet);

    if (paintInfo.phase != PaintPhaseForeground)
        return;

    LayoutObject* child = m_layoutFrameSet.firstChild();
    if (!child)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutFrameSet.location();
    paintChildren(paintInfo, adjustedPaintOffset);
    paintBorders(paintInfo, adjustedPaintOffset);
}

} // namespace blink
