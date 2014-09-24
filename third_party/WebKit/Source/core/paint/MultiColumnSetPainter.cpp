// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/MultiColumnSetPainter.h"

#include "core/paint/BlockPainter.h"
#include "core/paint/BoxPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderMultiColumnSet.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

void MultiColumnSetPainter::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderMultiColumnSet.style()->visibility() != VISIBLE)
        return;

    BlockPainter(m_renderMultiColumnSet).paintObject(paintInfo, paintOffset);

    // FIXME: Right now we're only painting in the foreground phase.
    // Columns should technically respect phases and allow for background/float/foreground overlap etc., just like
    // RenderBlocks do. Note this is a pretty minor issue, since the old column implementation clipped columns
    // anyway, thus making it impossible for them to overlap one another. It's also really unlikely that the columns
    // would overlap another block.
    if (!m_renderMultiColumnSet.flowThread() || !m_renderMultiColumnSet.isValid() || (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection))
        return;

    paintColumnRules(paintInfo, paintOffset);
}

void MultiColumnSetPainter::paintColumnRules(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_renderMultiColumnSet.flowThread()->isRenderPagedFlowThread())
        return;

    RenderStyle* blockStyle = m_renderMultiColumnSet.multiColumnBlockFlow()->style();
    const Color& ruleColor = m_renderMultiColumnSet.resolveColor(blockStyle, CSSPropertyWebkitColumnRuleColor);
    bool ruleTransparent = blockStyle->columnRuleIsTransparent();
    EBorderStyle ruleStyle = blockStyle->columnRuleStyle();
    LayoutUnit ruleThickness = blockStyle->columnRuleWidth();
    LayoutUnit colGap = m_renderMultiColumnSet.columnGap();
    bool renderRule = ruleStyle > BHIDDEN && !ruleTransparent;
    if (!renderRule)
        return;

    unsigned colCount = m_renderMultiColumnSet.actualColumnCount();
    if (colCount <= 1)
        return;

    bool antialias = BoxPainter::shouldAntialiasLines(paintInfo.context);

    bool leftToRight = m_renderMultiColumnSet.style()->isLeftToRightDirection();
    LayoutUnit currLogicalLeftOffset = leftToRight ? LayoutUnit() : m_renderMultiColumnSet.contentLogicalWidth();
    LayoutUnit ruleAdd = m_renderMultiColumnSet.borderAndPaddingLogicalLeft();
    LayoutUnit ruleLogicalLeft = leftToRight ? LayoutUnit() : m_renderMultiColumnSet.contentLogicalWidth();
    LayoutUnit inlineDirectionSize = m_renderMultiColumnSet.pageLogicalWidth();
    BoxSide boxSide = m_renderMultiColumnSet.isHorizontalWritingMode()
        ? leftToRight ? BSLeft : BSRight
        : leftToRight ? BSTop : BSBottom;

    for (unsigned i = 0; i < colCount; i++) {
        // Move to the next position.
        if (leftToRight) {
            ruleLogicalLeft += inlineDirectionSize + colGap / 2;
            currLogicalLeftOffset += inlineDirectionSize + colGap;
        } else {
            ruleLogicalLeft -= (inlineDirectionSize + colGap / 2);
            currLogicalLeftOffset -= (inlineDirectionSize + colGap);
        }

        // Now paint the column rule.
        if (i < colCount - 1) {
            LayoutUnit ruleLeft = m_renderMultiColumnSet.isHorizontalWritingMode() ? paintOffset.x() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd : paintOffset.x() + m_renderMultiColumnSet.borderLeft() + m_renderMultiColumnSet.paddingLeft();
            LayoutUnit ruleRight = m_renderMultiColumnSet.isHorizontalWritingMode() ? ruleLeft + ruleThickness : ruleLeft + m_renderMultiColumnSet.contentWidth();
            LayoutUnit ruleTop = m_renderMultiColumnSet.isHorizontalWritingMode() ? paintOffset.y() + m_renderMultiColumnSet.borderTop() + m_renderMultiColumnSet.paddingTop() : paintOffset.y() + ruleLogicalLeft - ruleThickness / 2 + ruleAdd;
            LayoutUnit ruleBottom = m_renderMultiColumnSet.isHorizontalWritingMode() ? ruleTop + m_renderMultiColumnSet.contentHeight() : ruleTop + ruleThickness;
            IntRect pixelSnappedRuleRect = pixelSnappedIntRectFromEdges(ruleLeft, ruleTop, ruleRight, ruleBottom);
            ObjectPainter::drawLineForBoxSide(paintInfo.context, pixelSnappedRuleRect.x(), pixelSnappedRuleRect.y(), pixelSnappedRuleRect.maxX(), pixelSnappedRuleRect.maxY(), boxSide, ruleColor, ruleStyle, 0, 0, antialias);
        }

        ruleLogicalLeft = currLogicalLeftOffset;
    }
}

} // namespace blink
