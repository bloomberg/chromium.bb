// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FieldsetPainter.h"

#include "core/paint/BoxDecorationData.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/DrawingRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderFieldset.h"

namespace blink {

void FieldsetPainter::paintBoxDecorationBackground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_renderFieldset))
        return;

    LayoutRect paintRect(paintOffset, m_renderFieldset.size());
    RenderBox* legend = m_renderFieldset.findLegend();
    if (!legend)
        return BoxPainter(m_renderFieldset).paintBoxDecorationBackground(paintInfo, paintOffset);

    // FIXME: We need to work with "rl" and "bt" block flow directions.  In those
    // cases the legend is embedded in the right and bottom borders respectively.
    // https://bugs.webkit.org/show_bug.cgi?id=47236
    if (m_renderFieldset.style()->isHorizontalWritingMode()) {
        LayoutUnit yOff = (legend->y() > 0) ? LayoutUnit() : (legend->height() - m_renderFieldset.borderTop()) / 2;
        paintRect.setHeight(paintRect.height() - yOff);
        paintRect.setY(paintRect.y() + yOff);
    } else {
        LayoutUnit xOff = (legend->x() > 0) ? LayoutUnit() : (legend->width() - m_renderFieldset.borderLeft()) / 2;
        paintRect.setWidth(paintRect.width() - xOff);
        paintRect.setX(paintRect.x() + xOff);
    }

    BoxDecorationData boxDecorationData(*m_renderFieldset.style(), m_renderFieldset.canRenderBorderImage(), m_renderFieldset.backgroundHasOpaqueTopLayer(), paintInfo.context);
    DrawingRecorder recorder(paintInfo.context, &m_renderFieldset, paintInfo.phase, pixelSnappedIntRect(paintOffset, paintRect.size()));

    if (boxDecorationData.bleedAvoidance() == BackgroundBleedNone)
        BoxPainter::paintBoxShadow(paintInfo, paintRect, m_renderFieldset.style(), Normal);
    BoxPainter(m_renderFieldset).paintFillLayers(paintInfo, boxDecorationData.backgroundColor, m_renderFieldset.style()->backgroundLayers(), paintRect);
    BoxPainter::paintBoxShadow(paintInfo, paintRect, m_renderFieldset.style(), Inset);

    if (!boxDecorationData.hasBorder)
        return;

    // Create a clipping region around the legend and paint the border as normal
    GraphicsContext* graphicsContext = paintInfo.context;
    GraphicsContextStateSaver stateSaver(*graphicsContext);

    // FIXME: We need to work with "rl" and "bt" block flow directions.  In those
    // cases the legend is embedded in the right and bottom borders respectively.
    // https://bugs.webkit.org/show_bug.cgi?id=47236
    if (m_renderFieldset.style()->isHorizontalWritingMode()) {
        LayoutUnit clipTop = paintRect.y();
        LayoutUnit clipHeight = max(static_cast<LayoutUnit>(m_renderFieldset.style()->borderTopWidth()), legend->height() - ((legend->height() - m_renderFieldset.borderTop()) / 2));
        graphicsContext->clipOut(pixelSnappedIntRect(paintRect.x() + legend->x(), clipTop, legend->width(), clipHeight));
    } else {
        LayoutUnit clipLeft = paintRect.x();
        LayoutUnit clipWidth = max(static_cast<LayoutUnit>(m_renderFieldset.style()->borderLeftWidth()), legend->width());
        graphicsContext->clipOut(pixelSnappedIntRect(clipLeft, paintRect.y() + legend->y(), clipWidth, legend->height()));
    }

    BoxPainter::paintBorder(m_renderFieldset, paintInfo, paintRect, m_renderFieldset.style());
}

void FieldsetPainter::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderFieldset.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, m_renderFieldset.size());
    RenderBox* legend = m_renderFieldset.findLegend();
    if (!legend)
        return BoxPainter(m_renderFieldset).paintMask(paintInfo, paintOffset);

    // FIXME: We need to work with "rl" and "bt" block flow directions.  In those
    // cases the legend is embedded in the right and bottom borders respectively.
    // https://bugs.webkit.org/show_bug.cgi?id=47236
    if (m_renderFieldset.style()->isHorizontalWritingMode()) {
        LayoutUnit yOff = (legend->y() > 0) ? LayoutUnit() : (legend->height() - m_renderFieldset.borderTop()) / 2;
        paintRect.expand(0, -yOff);
        paintRect.move(0, yOff);
    } else {
        LayoutUnit xOff = (legend->x() > 0) ? LayoutUnit() : (legend->width() - m_renderFieldset.borderLeft()) / 2;
        paintRect.expand(-xOff, 0);
        paintRect.move(xOff, 0);
    }

    BoxPainter(m_renderFieldset).paintMaskImages(paintInfo, paintRect);
}

} // namespace blink
