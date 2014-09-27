// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ReplacedPainter.h"

#include "core/paint/BoxPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderReplaced.h"

namespace blink {

void ReplacedPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderReplaced);

    if (!m_renderReplaced.shouldPaint(paintInfo, paintOffset))
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_renderReplaced.location();

    if (m_renderReplaced.hasBoxDecorationBackground() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection))
        m_renderReplaced.paintBoxDecorationBackground(paintInfo, adjustedPaintOffset);

    if (paintInfo.phase == PaintPhaseMask) {
        m_renderReplaced.paintMask(paintInfo, adjustedPaintOffset);
        return;
    }

    if (paintInfo.phase == PaintPhaseClippingMask && (!m_renderReplaced.hasLayer() || !m_renderReplaced.layer()->hasCompositedClippingMask()))
        return;

    LayoutRect paintRect = LayoutRect(adjustedPaintOffset, m_renderReplaced.size());
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && m_renderReplaced.style()->outlineWidth())
        m_renderReplaced.paintOutline(paintInfo, paintRect);

    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection && !m_renderReplaced.canHaveChildren() && paintInfo.phase != PaintPhaseClippingMask)
        return;

    if (!paintInfo.shouldPaintWithinRoot(&m_renderReplaced))
        return;

    bool drawSelectionTint = m_renderReplaced.selectionState() != RenderObject::SelectionNone && !m_renderReplaced.document().printing();
    if (paintInfo.phase == PaintPhaseSelection) {
        if (m_renderReplaced.selectionState() == RenderObject::SelectionNone)
            return;
        drawSelectionTint = false;
    }

    bool completelyClippedOut = false;
    if (m_renderReplaced.style()->hasBorderRadius()) {
        LayoutRect borderRect = LayoutRect(adjustedPaintOffset, m_renderReplaced.size());

        if (borderRect.isEmpty()) {
            completelyClippedOut = true;
        } else {
            // Push a clip if we have a border radius, since we want to round the foreground content that gets painted.
            paintInfo.context->save();
            RoundedRect roundedInnerRect = m_renderReplaced.style()->getRoundedInnerBorderFor(paintRect,
                m_renderReplaced.paddingTop() + m_renderReplaced.borderTop(), m_renderReplaced.paddingBottom() + m_renderReplaced.borderBottom(), m_renderReplaced.paddingLeft() + m_renderReplaced.borderLeft(), m_renderReplaced.paddingRight() + m_renderReplaced.borderRight(), true, true);
            BoxPainter::clipRoundedInnerRect(paintInfo.context, paintRect, roundedInnerRect);
        }
    }

    if (!completelyClippedOut) {
        if (paintInfo.phase == PaintPhaseClippingMask) {
            m_renderReplaced.paintClippingMask(paintInfo, adjustedPaintOffset);
        } else {
            m_renderReplaced.paintReplaced(paintInfo, adjustedPaintOffset);
        }

        if (m_renderReplaced.style()->hasBorderRadius())
            paintInfo.context->restore();
    }

    // The selection tint never gets clipped by border-radius rounding, since we want it to run right up to the edges of
    // surrounding content.
    if (drawSelectionTint) {
        LayoutRect selectionPaintingRect = m_renderReplaced.localSelectionRect();
        selectionPaintingRect.moveBy(adjustedPaintOffset);
        paintInfo.context->fillRect(pixelSnappedIntRect(selectionPaintingRect), m_renderReplaced.selectionBackgroundColor());
    }
}

} // namespace blink
