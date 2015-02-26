/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "core/layout/LayoutRegion.h"

#include "core/layout/LayoutFlowThread.h"

namespace blink {

LayoutRegion::LayoutRegion(Element* element, LayoutFlowThread* flowThread)
    : LayoutBlockFlow(element)
    , m_flowThread(flowThread)
    , m_isValid(false)
{
}

LayoutUnit LayoutRegion::pageLogicalWidth() const
{
    ASSERT(m_flowThread);
    return m_flowThread->isHorizontalWritingMode() ? contentWidth() : contentHeight();
}

LayoutUnit LayoutRegion::pageLogicalHeight() const
{
    ASSERT(m_flowThread);
    return m_flowThread->isHorizontalWritingMode() ? contentHeight() : contentWidth();
}

LayoutRect LayoutRegion::flowThreadPortionOverflowRect() const
{
    return overflowRectForFlowThreadPortion(flowThreadPortionRect(), isFirstRegion(), isLastRegion());
}

LayoutRect LayoutRegion::overflowRectForFlowThreadPortion(const LayoutRect& flowThreadPortionRect, bool isFirstPortion, bool isLastPortion) const
{
    ASSERT(isValid());

    if (hasOverflowClip())
        return flowThreadPortionRect;

    LayoutRect flowThreadOverflow = m_flowThread->visualOverflowRect();

    // Only clip along the flow thread axis.
    LayoutRect clipRect;
    if (m_flowThread->isHorizontalWritingMode()) {
        LayoutUnit minY = isFirstPortion ? flowThreadOverflow.y() : flowThreadPortionRect.y();
        LayoutUnit maxY = isLastPortion ? std::max(flowThreadPortionRect.maxY(), flowThreadOverflow.maxY()) : flowThreadPortionRect.maxY();
        LayoutUnit minX = std::min(flowThreadPortionRect.x(), flowThreadOverflow.x());
        LayoutUnit maxX = std::max(flowThreadPortionRect.maxX(), flowThreadOverflow.maxX());
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    } else {
        LayoutUnit minX = isFirstPortion ? flowThreadOverflow.x() : flowThreadPortionRect.x();
        LayoutUnit maxX = isLastPortion ? std::max(flowThreadPortionRect.maxX(), flowThreadOverflow.maxX()) : flowThreadPortionRect.maxX();
        LayoutUnit minY = std::min(flowThreadPortionRect.y(), (flowThreadOverflow.y()));
        LayoutUnit maxY = std::max(flowThreadPortionRect.y(), (flowThreadOverflow.maxY()));
        clipRect = LayoutRect(minX, minY, maxX - minX, maxY - minY);
    }

    return clipRect;
}

bool LayoutRegion::isFirstRegion() const
{
    ASSERT(isValid());

    return m_flowThread->firstRegion() == this;
}

bool LayoutRegion::isLastRegion() const
{
    ASSERT(isValid());

    return m_flowThread->lastRegion() == this;
}

void LayoutRegion::layoutBlock(bool relayoutChildren)
{
    LayoutBlockFlow::layoutBlock(relayoutChildren);

    // FIXME: We need to find a way to set up overflow properly. Our flow thread hasn't gotten a layout
    // yet, so we can't look to it for correct information. It's possible we could wait until after the LayoutFlowThread
    // gets a layout, and then try to propagate overflow information back to the region, and then mark for a second layout.
    // That second layout would then be able to use the information from the LayoutFlowThread to set up overflow.
    //
    // The big problem though is that overflow needs to be region-specific. We can't simply use the LayoutFlowThread's global
    // overflow values, since then we'd always think any narrow region had huge overflow (all the way to the width of the
    // LayoutFlowThread itself).
}

void LayoutRegion::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (!isValid()) {
        LayoutBlockFlow::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);
        return;
    }

    minLogicalWidth = m_flowThread->minPreferredLogicalWidth();
    maxLogicalWidth = m_flowThread->maxPreferredLogicalWidth();
}

} // namespace blink
