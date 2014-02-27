/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/line/LineWidth.h"

#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderRubyRun.h"

namespace WebCore {

LineWidth::LineWidth(RenderBlockFlow& block, bool isFirstLine, IndentTextOrNot shouldIndentText)
    : m_block(block)
    , m_uncommittedWidth(0)
    , m_committedWidth(0)
    , m_overhangWidth(0)
    , m_left(0)
    , m_right(0)
    , m_availableWidth(0)
    , m_segment(0)
    , m_isFirstLine(isFirstLine)
    , m_shouldIndentText(shouldIndentText)
{
    updateCurrentShapeSegment();
    updateAvailableWidth();
}

void LineWidth::updateAvailableWidth(LayoutUnit replacedHeight)
{
    LayoutUnit height = m_block.logicalHeight();
    LayoutUnit logicalHeight = m_block.minLineHeightForReplacedRenderer(m_isFirstLine, replacedHeight);
    m_left = m_block.logicalLeftOffsetForLine(height, shouldIndentText(), logicalHeight);
    m_right = m_block.logicalRightOffsetForLine(height, shouldIndentText(), logicalHeight);

    if (m_segment) {
        m_left = std::max<float>(m_segment->logicalLeft, m_left);
        m_right = std::min<float>(m_segment->logicalRight, m_right);
    }

    computeAvailableWidthFromLeftAndRight();
}

void LineWidth::shrinkAvailableWidthForNewFloatIfNeeded(FloatingObject* newFloat)
{
    LayoutUnit height = m_block.logicalHeight();
    if (height < m_block.logicalTopForFloat(newFloat) || height >= m_block.logicalBottomForFloat(newFloat))
        return;

    // When floats with shape outside are stacked, the floats are positioned based on the margin box of the float,
    // not the shape's contour. Since we computed the width based on the shape contour when we added the float,
    // when we add a subsequent float on the same line, we need to undo the shape delta in order to position
    // based on the margin box. In order to do this, we need to walk back through the floating object list to find
    // the first previous float that is on the same side as our newFloat.
    ShapeOutsideInfo* previousShapeOutsideInfo = 0;
    const FloatingObjectSet& floatingObjectSet = m_block.m_floatingObjects->set();
    FloatingObjectSetIterator it = floatingObjectSet.end();
    FloatingObjectSetIterator begin = floatingObjectSet.begin();
    LayoutUnit lineHeight = m_block.lineHeight(m_isFirstLine, m_block.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    for (--it; it != begin; --it) {
        FloatingObject* previousFloat = *it;
        if (previousFloat != newFloat && previousFloat->type() == newFloat->type()) {
            previousShapeOutsideInfo = previousFloat->renderer()->shapeOutsideInfo();
            if (previousShapeOutsideInfo)
                previousShapeOutsideInfo->updateDeltasForContainingBlockLine(&m_block, previousFloat, m_block.logicalHeight(), lineHeight);
            break;
        }
    }

    ShapeOutsideInfo* shapeOutsideInfo = newFloat->renderer()->shapeOutsideInfo();
    if (shapeOutsideInfo)
        shapeOutsideInfo->updateDeltasForContainingBlockLine(&m_block, newFloat, m_block.logicalHeight(), lineHeight);

    if (newFloat->type() == FloatingObject::FloatLeft) {
        float newLeft = m_block.logicalRightForFloat(newFloat);
        if (previousShapeOutsideInfo)
            newLeft -= previousShapeOutsideInfo->rightMarginBoxDelta();
        if (shapeOutsideInfo)
            newLeft += shapeOutsideInfo->rightMarginBoxDelta();

        if (shouldIndentText() && m_block.style()->isLeftToRightDirection())
            newLeft += floorToInt(m_block.textIndentOffset());
        m_left = std::max<float>(m_left, newLeft);
    } else {
        float newRight = m_block.logicalLeftForFloat(newFloat);
        if (previousShapeOutsideInfo)
            newRight -= previousShapeOutsideInfo->leftMarginBoxDelta();
        if (shapeOutsideInfo)
            newRight += shapeOutsideInfo->leftMarginBoxDelta();

        if (shouldIndentText() && !m_block.style()->isLeftToRightDirection())
            newRight -= floorToInt(m_block.textIndentOffset());
        m_right = std::min<float>(m_right, newRight);
    }

    computeAvailableWidthFromLeftAndRight();
}

void LineWidth::commit()
{
    m_committedWidth += m_uncommittedWidth;
    m_uncommittedWidth = 0;
}

void LineWidth::applyOverhang(RenderRubyRun* rubyRun, RenderObject* startRenderer, RenderObject* endRenderer)
{
    int startOverhang;
    int endOverhang;
    rubyRun->getOverhang(m_isFirstLine, startRenderer, endRenderer, startOverhang, endOverhang);

    startOverhang = std::min<int>(startOverhang, m_committedWidth);
    m_availableWidth += startOverhang;

    endOverhang = std::max(std::min<int>(endOverhang, m_availableWidth - currentWidth()), 0);
    m_availableWidth += endOverhang;
    m_overhangWidth += startOverhang + endOverhang;
}

inline static float availableWidthAtOffset(const RenderBlockFlow& block, const LayoutUnit& offset, bool shouldIndentText, float& newLineLeft, float& newLineRight)
{
    newLineLeft = block.logicalLeftOffsetForLine(offset, shouldIndentText);
    newLineRight = block.logicalRightOffsetForLine(offset, shouldIndentText);
    return std::max(0.0f, newLineRight - newLineLeft);
}

inline static float availableWidthAtOffset(const RenderBlockFlow& block, const LayoutUnit& offset, bool shouldIndentText)
{
    float newLineLeft = block.logicalLeftOffsetForLine(offset, shouldIndentText);
    float newLineRight = block.logicalRightOffsetForLine(offset, shouldIndentText);
    return std::max(0.0f, newLineRight - newLineLeft);
}

void LineWidth::updateLineDimension(LayoutUnit newLineTop, LayoutUnit newLineWidth, const float& newLineLeft, const float& newLineRight)
{
    if (newLineWidth <= m_availableWidth)
        return;

    m_block.setLogicalHeight(newLineTop);
    m_availableWidth = newLineWidth + m_overhangWidth;
    m_left = newLineLeft;
    m_right = newLineRight;
}

inline static bool isWholeLineFit(const RenderBlockFlow& block, const LayoutUnit& lineTop, LayoutUnit lineHeight, float uncommittedWidth, bool shouldIndentText)
{
    for (LayoutUnit lineBottom = lineTop; lineBottom <= lineTop + lineHeight; lineBottom++) {
        LayoutUnit availableWidthAtBottom = availableWidthAtOffset(block, lineBottom, shouldIndentText);
        if (availableWidthAtBottom < uncommittedWidth)
            return false;
    }
    return true;
}

void LineWidth::wrapNextToShapeOutside(bool isFirstLine)
{
    LayoutUnit lineHeight = m_block.lineHeight(isFirstLine, m_block.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
    LayoutUnit lineLogicalTop = m_block.logicalHeight();
    LayoutUnit newLineTop = lineLogicalTop;
    LayoutUnit floatLogicalBottom = m_block.nextFloatLogicalBottomBelow(lineLogicalTop);

    float newLineWidth;
    float newLineLeft = m_left;
    float newLineRight = m_right;
    while (true) {
        newLineWidth = availableWidthAtOffset(m_block, newLineTop, shouldIndentText(), newLineLeft, newLineRight);
        if (newLineWidth >= m_uncommittedWidth && isWholeLineFit(m_block, newLineTop, lineHeight, m_uncommittedWidth, shouldIndentText()))
            break;

        if (newLineTop >= floatLogicalBottom)
            break;

        newLineTop++;
    }
    updateLineDimension(newLineTop, newLineWidth, newLineLeft, newLineRight);
}

void LineWidth::fitBelowFloats(bool isFirstLine)
{
    ASSERT(!m_committedWidth);
    ASSERT(!fitsOnLine());

    LayoutUnit floatLogicalBottom;
    LayoutUnit lastFloatLogicalBottom = m_block.logicalHeight();
    float newLineWidth = m_availableWidth;
    float newLineLeft = m_left;
    float newLineRight = m_right;

    FloatingObject* lastFloatFromPreviousLine = (m_block.containsFloats() ? m_block.m_floatingObjects->set().last() : 0);
        if (lastFloatFromPreviousLine && lastFloatFromPreviousLine->renderer()->shapeOutsideInfo())
            return wrapNextToShapeOutside(isFirstLine);

    while (true) {
        floatLogicalBottom = m_block.nextFloatLogicalBottomBelow(lastFloatLogicalBottom, ShapeOutsideFloatShapeOffset);
        if (floatLogicalBottom <= lastFloatLogicalBottom)
            break;

        newLineWidth = availableWidthAtOffset(m_block, floatLogicalBottom, shouldIndentText(), newLineLeft, newLineRight);
        lastFloatLogicalBottom = floatLogicalBottom;

        if (newLineWidth >= m_uncommittedWidth) {
            ShapeInsideInfo* shapeInsideInfo = m_block.layoutShapeInsideInfo();
            if (shapeInsideInfo) {
                // To safely update our shape segments, the current segment must be the first in this line, so committedWidth has to be 0
                ASSERT(!m_committedWidth);

                LayoutUnit logicalOffsetFromShapeContainer = m_block.logicalOffsetFromShapeAncestorContainer(shapeInsideInfo->owner()).height();
                LayoutUnit lineHeight = m_block.lineHeight(false, m_block.isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes);
                shapeInsideInfo->updateSegmentsForLine(lastFloatLogicalBottom + logicalOffsetFromShapeContainer, lineHeight);
                updateCurrentShapeSegment();
                updateAvailableWidth();
            }
            break;
        }
    }
    updateLineDimension(lastFloatLogicalBottom, newLineWidth, newLineLeft, newLineRight);
}

void LineWidth::updateCurrentShapeSegment()
{
    if (ShapeInsideInfo* shapeInsideInfo = m_block.layoutShapeInsideInfo())
        m_segment = shapeInsideInfo->currentSegment();
}

void LineWidth::computeAvailableWidthFromLeftAndRight()
{
    m_availableWidth = max(0.0f, m_right - m_left) + m_overhangWidth;
}

}
