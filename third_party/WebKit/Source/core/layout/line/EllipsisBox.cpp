/**
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/layout/line/EllipsisBox.h"

#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/TextRunConstructor.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/layout/style/ShadowList.h"
#include "core/paint/EllipsisBoxPainter.h"
#include "platform/fonts/Font.h"
#include "platform/text/TextRun.h"

namespace blink {

void EllipsisBox::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    EllipsisBoxPainter(*this).paint(paintInfo, paintOffset, lineTop, lineBottom);
}

InlineBox* EllipsisBox::markupBox() const
{
    if (!m_shouldPaintMarkupBox || !layoutObject().isLayoutBlock())
        return 0;

    LayoutBlock& block = toLayoutBlock(layoutObject());
    RootInlineBox* lastLine = block.lineAtIndex(block.lineCount() - 1);
    if (!lastLine)
        return 0;

    // If the last line-box on the last line of a block is a link, -webkit-line-clamp paints that box after the ellipsis.
    // It does not actually move the link.
    InlineBox* anchorBox = lastLine->lastChild();
    if (!anchorBox || !anchorBox->layoutObject().style()->isLink())
        return 0;

    return anchorBox;
}

IntRect EllipsisBox::selectionRect()
{
    const LayoutStyle& style = layoutObject().styleRef(isFirstLineStyle());
    const Font& font = style.font();
    return enclosingIntRect(font.selectionRectForText(constructTextRun(&layoutObject(), font, m_str, style, TextRun::AllowTrailingExpansion), IntPoint(logicalLeft(), logicalTop() + root().selectionTopAdjustedForPrecedingBlock()), root().selectionHeightAdjustedForPrecedingBlock()));
}

bool EllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    // FIXME: the call to roundedLayoutPoint() below is temporary and should be removed once
    // the transition to LayoutUnit-based types is complete (crbug.com/321237)
    LayoutPoint adjustedLocation = accumulatedOffset + topLeft().roundedLayoutPoint();

    // Hit test the markup box.
    if (InlineBox* markupBox = this->markupBox()) {
        const LayoutStyle& style = layoutObject().styleRef(isFirstLineStyle());
        LayoutUnit mtx = adjustedLocation.x() + m_logicalWidth - markupBox->x();
        LayoutUnit mty = adjustedLocation.y() + style.fontMetrics().ascent() - (markupBox->y() + markupBox->layoutObject().style(isFirstLineStyle())->fontMetrics().ascent());
        if (markupBox->nodeAtPoint(request, result, locationInContainer, LayoutPoint(mtx, mty), lineTop, lineBottom)) {
            layoutObject().updateHitTestResult(result, locationInContainer.point() - LayoutSize(mtx, mty));
            return true;
        }
    }

    FloatPointWillBeLayoutPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.moveBy(accumulatedOffset);
    FloatRectWillBeLayoutRect boundsRect(boxOrigin, size());
    if (visibleToHitTestRequest(request) && boundsRect.intersects(FloatRectWillBeLayoutRect(HitTestLocation::rectForPoint(locationInContainer.point(), 0, 0, 0, 0)))) {
        layoutObject().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        // FIXME: the call to rawValue() below is temporary and should be removed once the transition
        // to LayoutUnit-based types is complete (crbug.com/321237)
        if (!result.addNodeToListBasedTestResult(layoutObject().node(), request, locationInContainer, boundsRect.rawValue()))
            return true;
    }

    return false;
}

} // namespace blink
