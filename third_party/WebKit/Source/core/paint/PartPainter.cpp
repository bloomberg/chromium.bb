// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/PartPainter.h"

#include "core/layout/Layer.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/RenderDrawingRecorder.h"
#include "core/paint/RoundedInnerRectClipper.h"
#include "core/paint/ScrollableAreaPainter.h"
#include "core/paint/TransformRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderPart.h"

namespace blink {

void PartPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderPart);

    if (!m_renderPart.shouldPaint(paintInfo, paintOffset))
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_renderPart.location();
    LayoutRect borderRect(adjustedPaintOffset, m_renderPart.size());

    if (m_renderPart.hasBoxDecorationBackground() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection))
        BoxPainter(m_renderPart).paintBoxDecorationBackground(paintInfo, adjustedPaintOffset);

    if (paintInfo.phase == PaintPhaseMask) {
        BoxPainter(m_renderPart).paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && m_renderPart.style()->hasOutline())
        ObjectPainter(m_renderPart).paintOutline(paintInfo, borderRect);

    if (paintInfo.phase != PaintPhaseForeground)
        return;

    {
        OwnPtr<RoundedInnerRectClipper> clipper;
        if (m_renderPart.style()->hasBorderRadius()) {
            if (borderRect.isEmpty())
                return;

            FloatRoundedRect roundedInnerRect = m_renderPart.style()->getRoundedInnerBorderFor(borderRect,
                m_renderPart.paddingTop() + m_renderPart.borderTop(), m_renderPart.paddingBottom() + m_renderPart.borderBottom(), m_renderPart.paddingLeft() + m_renderPart.borderLeft(), m_renderPart.paddingRight() + m_renderPart.borderRight(), true, true);
            clipper = adoptPtr(new RoundedInnerRectClipper(m_renderPart, paintInfo, borderRect, roundedInnerRect, ApplyToDisplayListIfEnabled));
        }

        if (m_renderPart.widget())
            m_renderPart.paintContents(paintInfo, paintOffset);
    }

    // Paint a partially transparent wash over selected widgets.
    if (m_renderPart.isSelected() && !m_renderPart.document().printing()) {
        LayoutRect rect = m_renderPart.localSelectionRect();
        rect.moveBy(adjustedPaintOffset);
        IntRect selectionRect = pixelSnappedIntRect(rect);
        RenderDrawingRecorder recorder(paintInfo.context, m_renderPart, paintInfo.phase, selectionRect);
        paintInfo.context->fillRect(selectionRect, m_renderPart.selectionBackgroundColor());
    }

    if (m_renderPart.canResize())
        ScrollableAreaPainter(*m_renderPart.layer()->scrollableArea()).paintResizer(paintInfo.context, roundedIntPoint(adjustedPaintOffset), paintInfo.rect);
}

void PartPainter::paintContents(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + m_renderPart.location();

    Widget* widget = m_renderPart.widget();
    RELEASE_ASSERT(widget);

    // Tell the widget to paint now. This is the only time the widget is allowed
    // to paint itself. That way it will composite properly with z-indexed layers.
    IntPoint widgetLocation = widget->frameRect().location();
    IntPoint paintLocation(roundToInt(adjustedPaintOffset.x() + m_renderPart.borderLeft() + m_renderPart.paddingLeft()),
        roundToInt(adjustedPaintOffset.y() + m_renderPart.borderTop() + m_renderPart.paddingTop()));
    IntRect paintRect = paintInfo.rect;

    IntSize widgetPaintOffset = paintLocation - widgetLocation;
    // When painting widgets into compositing layers, tx and ty are relative to the enclosing compositing layer,
    // not the root. In this case, shift the CTM and adjust the paintRect to be root-relative to fix plug-in drawing.
    TransformRecorder transform(*paintInfo.context, m_renderPart.displayItemClient(),
        AffineTransform::translation(widgetPaintOffset.width(), widgetPaintOffset.height()));
    paintRect.move(-widgetPaintOffset);
    widget->paint(paintInfo.context, paintRect);
}

} // namespace blink
