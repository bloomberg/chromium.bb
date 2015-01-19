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

#include "core/rendering/RenderFlowThread.h"

#include "core/rendering/RenderMultiColumnSet.h"
#include "core/rendering/RenderView.h"

namespace blink {

RenderFlowThread::RenderFlowThread()
    : RenderBlockFlow(0)
    , m_regionsInvalidated(false)
    , m_regionsHaveUniformLogicalHeight(true)
    , m_pageLogicalSizeChanged(false)
{
    setFlowThreadState(InsideOutOfFlowThread);
}

void RenderFlowThread::removeRegionFromThread(RenderMultiColumnSet* columnSet)
{
    ASSERT(columnSet);
    m_multiColumnSetList.remove(columnSet);
}

void RenderFlowThread::invalidateRegions()
{
    if (m_regionsInvalidated) {
        ASSERT(selfNeedsLayout());
        return;
    }

    setNeedsLayoutAndFullPaintInvalidation();

    m_regionsInvalidated = true;
}

void RenderFlowThread::validateRegions()
{
    if (m_regionsInvalidated) {
        m_regionsInvalidated = false;
        m_regionsHaveUniformLogicalHeight = true;

        if (hasRegions()) {
            LayoutUnit previousRegionLogicalHeight = 0;
            bool firstRegionVisited = false;

            for (RenderMultiColumnSetList::iterator iter = m_multiColumnSetList.begin(); iter != m_multiColumnSetList.end(); ++iter) {
                RenderMultiColumnSet* columnSet = *iter;
                LayoutUnit regionLogicalHeight = columnSet->pageLogicalHeight();

                if (!firstRegionVisited) {
                    firstRegionVisited = true;
                } else {
                    if (m_regionsHaveUniformLogicalHeight && previousRegionLogicalHeight != regionLogicalHeight)
                        m_regionsHaveUniformLogicalHeight = false;
                }

                previousRegionLogicalHeight = regionLogicalHeight;
            }
        }
    }

    updateLogicalWidth(); // Called to get the maximum logical width for the columnSet.
    updateRegionsFlowThreadPortionRect();
}

void RenderFlowThread::mapRectToPaintInvalidationBacking(const RenderLayerModelObject* paintInvalidationContainer, LayoutRect& rect, const PaintInvalidationState* paintInvalidationState) const
{
    ASSERT(paintInvalidationContainer != this); // A flow thread should never be an invalidation container.
    rect = fragmentsBoundingBox(rect);
    RenderBlockFlow::mapRectToPaintInvalidationBacking(paintInvalidationContainer, rect, paintInvalidationState);
}

void RenderFlowThread::layout()
{
    m_pageLogicalSizeChanged = m_regionsInvalidated && everHadLayout();
    RenderBlockFlow::layout();
    m_pageLogicalSizeChanged = false;
}

void RenderFlowThread::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_position = logicalTop;
    computedValues.m_extent = 0;

    for (RenderMultiColumnSetList::const_iterator iter = m_multiColumnSetList.begin(); iter != m_multiColumnSetList.end(); ++iter) {
        RenderMultiColumnSet* columnSet = *iter;
        computedValues.m_extent += columnSet->logicalHeightInFlowThread();
    }
}

bool RenderFlowThread::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (hitTestAction == HitTestBlockBackground)
        return false;
    return RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction);
}

LayoutUnit RenderFlowThread::pageLogicalHeightForOffset(LayoutUnit offset)
{
    RenderMultiColumnSet* columnSet = columnSetAtBlockOffset(offset);
    if (!columnSet)
        return 0;

    return columnSet->pageLogicalHeight();
}

LayoutUnit RenderFlowThread::pageRemainingLogicalHeightForOffset(LayoutUnit offset, PageBoundaryRule pageBoundaryRule)
{
    RenderMultiColumnSet* columnSet = columnSetAtBlockOffset(offset);
    if (!columnSet)
        return 0;

    LayoutUnit pageLogicalTop = columnSet->pageLogicalTopForOffset(offset);
    LayoutUnit pageLogicalHeight = columnSet->pageLogicalHeight();
    LayoutUnit pageLogicalBottom = pageLogicalTop + pageLogicalHeight;
    LayoutUnit remainingHeight = pageLogicalBottom - offset;
    if (pageBoundaryRule == IncludePageBoundary) {
        // If IncludePageBoundary is set, the line exactly on the top edge of a
        // columnSet will act as being part of the previous columnSet.
        remainingHeight = intMod(remainingHeight, pageLogicalHeight);
    }
    return remainingHeight;
}

RenderRegion* RenderFlowThread::firstRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    return m_multiColumnSetList.first();
}

RenderRegion* RenderFlowThread::lastRegion() const
{
    if (!hasValidRegionInfo())
        return 0;
    return m_multiColumnSetList.last();
}

void RenderFlowThread::updateRegionsFlowThreadPortionRect()
{
    LayoutUnit logicalHeight = 0;
    // FIXME: Optimize not to clear the interval all the time. This implies manually managing the tree nodes lifecycle.
    m_multiColumnSetIntervalTree.clear();
    m_multiColumnSetIntervalTree.initIfNeeded();
    for (RenderMultiColumnSetList::iterator iter = m_multiColumnSetList.begin(); iter != m_multiColumnSetList.end(); ++iter) {
        RenderMultiColumnSet* columnSet = *iter;

        LayoutUnit columnSetLogicalWidth = columnSet->pageLogicalWidth();
        LayoutUnit columnSetLogicalHeight = std::min<LayoutUnit>(RenderFlowThread::maxLogicalHeight() - logicalHeight, columnSet->logicalHeightInFlowThread());

        LayoutRect columnSetRect(style()->direction() == LTR ? LayoutUnit() : logicalWidth() - columnSetLogicalWidth, logicalHeight, columnSetLogicalWidth, columnSetLogicalHeight);

        columnSet->setFlowThreadPortionRect(isHorizontalWritingMode() ? columnSetRect : columnSetRect.transposedRect());

        m_multiColumnSetIntervalTree.add(MultiColumnSetIntervalTree::createInterval(logicalHeight, logicalHeight + columnSetLogicalHeight, columnSet));

        logicalHeight += columnSetLogicalHeight;
    }
}

void RenderFlowThread::collectLayerFragments(LayerFragments& layerFragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect)
{
    ASSERT(!m_regionsInvalidated);

    for (RenderMultiColumnSetList::const_iterator iter = m_multiColumnSetList.begin(); iter != m_multiColumnSetList.end(); ++iter) {
        RenderMultiColumnSet* columnSet = *iter;
        columnSet->collectLayerFragments(layerFragments, layerBoundingBox, dirtyRect);
    }
}

LayoutRect RenderFlowThread::fragmentsBoundingBox(const LayoutRect& layerBoundingBox) const
{
    ASSERT(!m_regionsInvalidated);

    LayoutRect result;
    for (RenderMultiColumnSetList::const_iterator iter = m_multiColumnSetList.begin(); iter != m_multiColumnSetList.end(); ++iter) {
        RenderMultiColumnSet* columnSet = *iter;
        LayerFragments fragments;
        columnSet->collectLayerFragments(fragments, layerBoundingBox, LayoutRect::infiniteIntRect());
        for (size_t i = 0; i < fragments.size(); ++i) {
            const LayerFragment& fragment = fragments.at(i);
            LayoutRect fragmentRect(layerBoundingBox);
            fragmentRect.intersect(fragment.paginationClip);
            fragmentRect.moveBy(fragment.paginationOffset);
            result.unite(fragmentRect);
        }
    }

    return result;
}

void RenderFlowThread::MultiColumnSetSearchAdapter::collectIfNeeded(const MultiColumnSetInterval& interval)
{
    if (m_result)
        return;
    if (interval.low() <= m_offset && interval.high() > m_offset)
        m_result = interval.data();
}

} // namespace blink
