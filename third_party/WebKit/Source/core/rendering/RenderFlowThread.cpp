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

#include "core/dom/Node.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/HitTestRequest.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderMultiColumnSet.h"
#include "core/rendering/RenderView.h"
#include "platform/PODIntervalTree.h"
#include "platform/geometry/TransformState.h"

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

class CurrentRenderFlowThreadDisabler {
    WTF_MAKE_NONCOPYABLE(CurrentRenderFlowThreadDisabler);
public:
    CurrentRenderFlowThreadDisabler(RenderView* view)
        : m_view(view)
        , m_renderFlowThread(0)
    {
        m_renderFlowThread = m_view->flowThreadController()->currentRenderFlowThread();
        if (m_renderFlowThread)
            view->flowThreadController()->setCurrentRenderFlowThread(0);
    }
    ~CurrentRenderFlowThreadDisabler()
    {
        if (m_renderFlowThread)
            m_view->flowThreadController()->setCurrentRenderFlowThread(m_renderFlowThread);
    }
private:
    RenderView* m_view;
    RenderFlowThread* m_renderFlowThread;
};

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

void RenderFlowThread::layout()
{
    m_pageLogicalSizeChanged = m_regionsInvalidated && everHadLayout();

    CurrentRenderFlowThreadMaintainer currentFlowThreadSetter(this);
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

bool RenderFlowThread::shouldIssuePaintInvalidations(const LayoutRect& r) const
{
    if (view()->document().printing() || r.isEmpty())
        return false;

    return true;
}

void RenderFlowThread::paintInvalidationRectangleInRegions(const LayoutRect& paintInvalidationRect) const
{
    if (!shouldIssuePaintInvalidations(paintInvalidationRect) || !hasValidRegionInfo())
        return;

    // We can't use currentFlowThread as it is possible to have interleaved flow threads and the wrong one could be used.
    // Let each columnSet figure out the proper enclosing flow thread.
    CurrentRenderFlowThreadDisabler disabler(view());

    for (RenderMultiColumnSetList::const_iterator iter = m_multiColumnSetList.begin(); iter != m_multiColumnSetList.end(); ++iter) {
        RenderMultiColumnSet* columnSet = *iter;

        columnSet->paintInvalidationForFlowThreadContent(paintInvalidationRect);
    }
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

LayoutRect RenderFlowThread::fragmentsBoundingBox(const LayoutRect& layerBoundingBox)
{
    ASSERT(!m_regionsInvalidated);

    LayoutRect result;
    for (RenderMultiColumnSetList::const_iterator iter = m_multiColumnSetList.begin(); iter != m_multiColumnSetList.end(); ++iter) {
        RenderMultiColumnSet* columnSet = *iter;
        LayerFragments fragments;
        columnSet->collectLayerFragments(fragments, layerBoundingBox, PaintInfo::infiniteRect());
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

bool RenderFlowThread::cachedOffsetFromLogicalTopOfFirstRegion(const RenderBox* box, LayoutUnit& result) const
{
    RenderBoxToOffsetMap::const_iterator offsetIterator = m_boxesToOffsetMap.find(box);
    if (offsetIterator == m_boxesToOffsetMap.end())
        return false;

    result = offsetIterator->value;
    return true;
}

void RenderFlowThread::setOffsetFromLogicalTopOfFirstRegion(const RenderBox* box, LayoutUnit offset)
{
    m_boxesToOffsetMap.set(box, offset);
}

void RenderFlowThread::clearOffsetFromLogicalTopOfFirstRegion(const RenderBox* box)
{
    ASSERT(m_boxesToOffsetMap.contains(box));
    m_boxesToOffsetMap.remove(box);
}

const RenderBox* RenderFlowThread::currentStatePusherRenderBox() const
{
    const RenderObject* currentObject = m_statePusherObjectsStack.isEmpty() ? 0 : m_statePusherObjectsStack.last();
    if (currentObject && currentObject->isBox())
        return toRenderBox(currentObject);

    return 0;
}

void RenderFlowThread::pushFlowThreadLayoutState(const RenderObject& object)
{
    if (const RenderBox* currentBoxDescendant = currentStatePusherRenderBox()) {
        LayoutState* layoutState = currentBoxDescendant->view()->layoutState();
        if (layoutState && layoutState->isPaginated()) {
            ASSERT(layoutState->renderer() == currentBoxDescendant);
            LayoutSize offsetDelta = layoutState->layoutOffset() - layoutState->pageOffset();
            setOffsetFromLogicalTopOfFirstRegion(currentBoxDescendant, currentBoxDescendant->isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width());
        }
    }

    m_statePusherObjectsStack.add(&object);
}

void RenderFlowThread::popFlowThreadLayoutState()
{
    m_statePusherObjectsStack.removeLast();

    if (const RenderBox* currentBoxDescendant = currentStatePusherRenderBox()) {
        LayoutState* layoutState = currentBoxDescendant->view()->layoutState();
        if (layoutState && layoutState->isPaginated())
            clearOffsetFromLogicalTopOfFirstRegion(currentBoxDescendant);
    }
}

LayoutUnit RenderFlowThread::offsetFromLogicalTopOfFirstRegion(const RenderBlock* currentBlock) const
{
    // First check if we cached the offset for the block if it's an ancestor containing block of the box
    // being currently laid out.
    LayoutUnit offset;
    if (cachedOffsetFromLogicalTopOfFirstRegion(currentBlock, offset))
        return offset;

    // If it's the current box being laid out, use the layout state.
    const RenderBox* currentBoxDescendant = currentStatePusherRenderBox();
    if (currentBlock == currentBoxDescendant) {
        LayoutState* layoutState = view()->layoutState();
        ASSERT(layoutState->renderer() == currentBlock);
        ASSERT(layoutState && layoutState->isPaginated());
        LayoutSize offsetDelta = layoutState->layoutOffset() - layoutState->pageOffset();
        return currentBoxDescendant->isHorizontalWritingMode() ? offsetDelta.height() : offsetDelta.width();
    }

    // As a last resort, take the slow path.
    LayoutRect blockRect(0, 0, currentBlock->width(), currentBlock->height());
    while (currentBlock && !currentBlock->isRenderFlowThread()) {
        RenderBlock* containerBlock = currentBlock->containingBlock();
        ASSERT(containerBlock);
        if (!containerBlock)
            return 0;
        LayoutPoint currentBlockLocation = currentBlock->location();

        if (containerBlock->style()->writingMode() != currentBlock->style()->writingMode()) {
            // We have to put the block rect in container coordinates
            // and we have to take into account both the container and current block flipping modes
            if (containerBlock->style()->isFlippedBlocksWritingMode()) {
                if (containerBlock->isHorizontalWritingMode())
                    blockRect.setY(currentBlock->height() - blockRect.maxY());
                else
                    blockRect.setX(currentBlock->width() - blockRect.maxX());
            }
            currentBlock->flipForWritingMode(blockRect);
        }
        blockRect.moveBy(currentBlockLocation);
        currentBlock = containerBlock;
    }

    return currentBlock->isHorizontalWritingMode() ? blockRect.y() : blockRect.x();
}

void RenderFlowThread::RegionSearchAdapter::collectIfNeeded(const MultiColumnSetInterval& interval)
{
    if (m_result)
        return;
    if (interval.low() <= m_offset && interval.high() > m_offset)
        m_result = interval.data();
}

CurrentRenderFlowThreadMaintainer::CurrentRenderFlowThreadMaintainer(RenderFlowThread* renderFlowThread)
    : m_renderFlowThread(renderFlowThread)
    , m_previousRenderFlowThread(0)
{
    if (!m_renderFlowThread)
        return;
    RenderView* view = m_renderFlowThread->view();
    m_previousRenderFlowThread = view->flowThreadController()->currentRenderFlowThread();
    view->flowThreadController()->setCurrentRenderFlowThread(m_renderFlowThread);
}

CurrentRenderFlowThreadMaintainer::~CurrentRenderFlowThreadMaintainer()
{
    if (!m_renderFlowThread)
        return;
    RenderView* view = m_renderFlowThread->view();
    ASSERT(view->flowThreadController()->currentRenderFlowThread() == m_renderFlowThread);
    view->flowThreadController()->setCurrentRenderFlowThread(m_previousRenderFlowThread);
}


} // namespace blink
