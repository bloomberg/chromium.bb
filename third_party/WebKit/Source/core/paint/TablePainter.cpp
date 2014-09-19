// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TablePainter.h"

#include "core/paint/BoxPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderTable.h"
#include "core/rendering/RenderTableSection.h"
#include "core/rendering/style/CollapsedBorderValue.h"

namespace blink {

void TablePainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderTable);

    LayoutPoint adjustedPaintOffset = paintOffset + m_renderTable.location();

    PaintPhase paintPhase = paintInfo.phase;

    if (!m_renderTable.isDocumentElement()) {
        LayoutRect overflowBox = m_renderTable.visualOverflowRect();
        m_renderTable.flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        if (!overflowBox.intersects(paintInfo.rect))
            return;
    }

    bool pushedClip = m_renderTable.pushContentsClip(paintInfo, adjustedPaintOffset, ForceContentsClip);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        m_renderTable.popContentsClip(paintInfo, paintPhase, adjustedPaintOffset);
}

void TablePainter::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhaseBlockBackground || paintPhase == PaintPhaseChildBlockBackground) && m_renderTable.hasBoxDecorationBackground() && m_renderTable.style()->visibility() == VISIBLE)
        paintBoxDecorationBackground(paintInfo, paintOffset);

    if (paintPhase == PaintPhaseMask) {
        m_renderTable.paintMask(paintInfo, paintOffset);
        return;
    }

    // We're done. We don't bother painting any children.
    if (paintPhase == PaintPhaseBlockBackground)
        return;

    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintPhase == PaintPhaseChildBlockBackgrounds)
        paintPhase = PaintPhaseChildBlockBackground;

    PaintInfo info(paintInfo);
    info.phase = paintPhase;
    info.updatePaintingRootForChildren(&m_renderTable);

    for (RenderObject* child = m_renderTable.firstChild(); child; child = child->nextSibling()) {
        if (child->isBox() && !toRenderBox(child)->hasSelfPaintingLayer() && (child->isTableSection() || child->isTableCaption())) {
            LayoutPoint childPoint = m_renderTable.flipForWritingModeForChild(toRenderBox(child), paintOffset);
            child->paint(info, childPoint);
        }
    }

    if (m_renderTable.collapseBorders() && paintPhase == PaintPhaseChildBlockBackground && m_renderTable.style()->visibility() == VISIBLE) {
        m_renderTable.recalcCollapsedBorders();
        // Using our cached sorted styles, we then do individual passes,
        // painting each style of border from lowest precedence to highest precedence.
        info.phase = PaintPhaseCollapsedTableBorders;
        RenderTable::CollapsedBorderValues collapsedBorders = m_renderTable.collapsedBorders();
        size_t count = collapsedBorders.size();
        for (size_t i = 0; i < count; ++i) {
            // FIXME: pass this value into children rather than storing temporarily on the RenderTable object.
            m_renderTable.setCurrentBorderValue(&collapsedBorders[i]);
            for (RenderTableSection* section = m_renderTable.bottomSection(); section; section = m_renderTable.sectionAbove(section)) {
                LayoutPoint childPoint = m_renderTable.flipForWritingModeForChild(section, paintOffset);
                section->paint(info, childPoint);
            }
        }
        m_renderTable.setCurrentBorderValue(0);
    }

    // Paint outline.
    if ((paintPhase == PaintPhaseOutline || paintPhase == PaintPhaseSelfOutline) && m_renderTable.style()->hasOutline() && m_renderTable.style()->visibility() == VISIBLE)
        m_renderTable.paintOutline(paintInfo, LayoutRect(paintOffset, m_renderTable.size()));
}

void TablePainter::paintBoxDecorationBackground(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(&m_renderTable))
        return;

    LayoutRect rect(paintOffset, m_renderTable.size());
    m_renderTable.subtractCaptionRect(rect);
    BoxPainter(m_renderTable).paintBoxDecorationBackgroundWithRect(paintInfo, paintOffset, rect);
}

void TablePainter::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderTable.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutRect rect(paintOffset, m_renderTable.size());
    m_renderTable.subtractCaptionRect(rect);

    BoxPainter(m_renderTable).paintMaskImages(paintInfo, rect);
}

} // namespace blink
