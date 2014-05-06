/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderMultiColumnFlowThread.h"

#include "core/rendering/RenderMultiColumnSet.h"

namespace WebCore {

RenderMultiColumnFlowThread::RenderMultiColumnFlowThread()
    : m_columnCount(1)
    , m_columnWidth(0)
    , m_columnHeightAvailable(0)
    , m_inBalancingPass(false)
    , m_needsColumnHeightsRecalculation(false)
{
    setFlowThreadState(InsideInFlowThread);
}

RenderMultiColumnFlowThread::~RenderMultiColumnFlowThread()
{
}

RenderMultiColumnFlowThread* RenderMultiColumnFlowThread::createAnonymous(Document& document, RenderStyle* parentStyle)
{
    RenderMultiColumnFlowThread* renderer = new RenderMultiColumnFlowThread();
    renderer->setDocumentForAnonymous(&document);
    renderer->setStyle(RenderStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK));
    return renderer;
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::firstMultiColumnSet() const
{
    for (RenderObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return 0;
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::lastMultiColumnSet() const
{
    for (RenderObject* sibling = multiColumnBlockFlow()->lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return 0;
}

void RenderMultiColumnFlowThread::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    RenderBlockFlow::addChild(newChild, beforeChild);
    if (firstMultiColumnSet())
        return;

    // For now we only create one column set. It's created as soon as the multicol container gets
    // any content at all.
    RenderMultiColumnSet* newSet = RenderMultiColumnSet::createAnonymous(this, multiColumnBlockFlow()->style());

    // Need to skip RenderBlockFlow's implementation of addChild(), or we'd get redirected right
    // back here.
    multiColumnBlockFlow()->RenderBlock::addChild(newSet);

    invalidateRegions();
}

void RenderMultiColumnFlowThread::populate()
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
    ASSERT(!nextSibling());
    // Reparent children preceding the flow thread into the flow thread. It's multicol content
    // now. At this point there's obviously nothing after the flow thread, but renderers (column
    // sets and spanners) will be inserted there as we insert elements into the flow thread.
    multicolContainer->moveChildrenTo(this, multicolContainer->firstChild(), this, true);
}

void RenderMultiColumnFlowThread::evacuateAndDestroy()
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();

    // Remove all sets.
    while (RenderMultiColumnSet* columnSet = firstMultiColumnSet())
        columnSet->destroy();

    ASSERT(!previousSibling());
    ASSERT(!nextSibling());

    // Finally we can promote all flow thread's children. Before we move them to the flow thread's
    // container, we need to unregister the flow thread, so that they aren't just re-added again to
    // the flow thread that we're trying to empty.
    multicolContainer->resetMultiColumnFlowThread();
    moveAllChildrenTo(multicolContainer, true);

    // FIXME: it's scary that neither destroy() nor the move*Children* methods take care of this,
    // and instead leave you with dangling root line box pointers. But since this is how it is done
    // in other parts of the code that deal with reparenting renderers, let's do the cleanup on our
    // own here as well.
    deleteLineBoxTree();

    destroy();
}

LayoutSize RenderMultiColumnFlowThread::columnOffset(const LayoutPoint& point) const
{
    if (!hasValidRegionInfo())
        return LayoutSize(0, 0);

    LayoutPoint flowThreadPoint(point);
    flipForWritingMode(flowThreadPoint);
    LayoutUnit blockOffset = isHorizontalWritingMode() ? flowThreadPoint.y() : flowThreadPoint.x();
    RenderRegion* renderRegion = regionAtBlockOffset(blockOffset);
    if (!renderRegion)
        return LayoutSize(0, 0);
    return toRenderMultiColumnSet(renderRegion)->flowThreadTranslationAtOffset(blockOffset);
}

void RenderMultiColumnFlowThread::layoutColumns(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    // Update the dimensions of our regions before we lay out the flow thread.
    // FIXME: Eventually this is going to get way more complicated, and we will be destroying regions
    // instead of trying to keep them around.
    bool shouldInvalidateRegions = false;
    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet()) {
        if (relayoutChildren || columnSet->needsLayout()) {
            if (!m_inBalancingPass)
                columnSet->prepareForLayout();
            shouldInvalidateRegions = true;
        }
    }

    if (shouldInvalidateRegions)
        invalidateRegions();

    if (relayoutChildren)
        layoutScope.setChildNeedsLayout(this);

    if (requiresBalancing()) {
        // At the end of multicol layout, relayoutForPagination() is called unconditionally, but if
        // no children are to be laid out (e.g. fixed width with layout already being up-to-date),
        // we want to prevent it from doing any work, so that the column balancing machinery doesn't
        // kick in and trigger additional unnecessary layout passes. Actually, it's not just a good
        // idea in general to not waste time on balancing content that hasn't been re-laid out; we
        // are actually required to guarantee this. The calculation of implicit breaks needs to be
        // preceded by a proper layout pass, since it's layout that sets up content runs, and the
        // runs get deleted right after every pass.
        m_needsColumnHeightsRecalculation = shouldInvalidateRegions || needsLayout();
    }

    layoutIfNeeded();
}

bool RenderMultiColumnFlowThread::computeColumnCountAndWidth()
{
    RenderBlock* columnBlock = multiColumnBlockFlow();
    LayoutUnit oldColumnWidth = m_columnWidth;

    // Calculate our column width and column count.
    m_columnCount = 1;
    m_columnWidth = columnBlock->contentLogicalWidth();

    const RenderStyle* columnStyle = columnBlock->style();
    ASSERT(!columnStyle->hasAutoColumnCount() || !columnStyle->hasAutoColumnWidth());

    LayoutUnit availWidth = m_columnWidth;
    LayoutUnit colGap = columnBlock->columnGap();
    LayoutUnit colWidth = max<LayoutUnit>(1, LayoutUnit(columnStyle->columnWidth()));
    int colCount = max<int>(1, columnStyle->columnCount());

    if (columnStyle->hasAutoColumnWidth() && !columnStyle->hasAutoColumnCount()) {
        m_columnCount = colCount;
        m_columnWidth = std::max<LayoutUnit>(0, (availWidth - ((m_columnCount - 1) * colGap)) / m_columnCount);
    } else if (!columnStyle->hasAutoColumnWidth() && columnStyle->hasAutoColumnCount()) {
        m_columnCount = std::max<LayoutUnit>(1, (availWidth + colGap) / (colWidth + colGap));
        m_columnWidth = ((availWidth + colGap) / m_columnCount) - colGap;
    } else {
        m_columnCount = std::max<LayoutUnit>(std::min<LayoutUnit>(colCount, (availWidth + colGap) / (colWidth + colGap)), 1);
        m_columnWidth = ((availWidth + colGap) / m_columnCount) - colGap;
    }

    return m_columnWidth != oldColumnWidth;
}

bool RenderMultiColumnFlowThread::recalculateColumnHeights()
{
    if (!m_needsColumnHeightsRecalculation)
        return false;

    // Column heights may change here because of balancing. We may have to do multiple layout
    // passes, depending on how the contents is fitted to the changed column heights. In most
    // cases, laying out again twice or even just once will suffice. Sometimes we need more
    // passes than that, though, but the number of retries should not exceed the number of
    // columns, unless we have a bug.
    bool needsRelayout = false;
    for (RenderMultiColumnSet* multicolSet = firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        if (multicolSet->recalculateColumnHeight(!m_inBalancingPass)) {
            multicolSet->setChildNeedsLayout(MarkOnlyThis);
            needsRelayout = true;
        }
    }

    if (needsRelayout)
        setChildNeedsLayout(MarkOnlyThis);

    m_inBalancingPass = needsRelayout;
    return needsRelayout;
}

const char* RenderMultiColumnFlowThread::renderName() const
{
    return "RenderMultiColumnFlowThread";
}

void RenderMultiColumnFlowThread::addRegionToThread(RenderRegion* renderRegion)
{
    RenderMultiColumnSet* columnSet = toRenderMultiColumnSet(renderRegion);
    if (RenderMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        RenderRegionList::iterator it = m_regionList.find(nextSet);
        ASSERT(it != m_regionList.end());
        m_regionList.insertBefore(it, columnSet);
    } else {
        m_regionList.add(columnSet);
    }
    renderRegion->setIsValid(true);
}

void RenderMultiColumnFlowThread::willBeRemovedFromTree()
{
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachRegion();
    multiColumnBlockFlow()->resetMultiColumnFlowThread();
    RenderFlowThread::willBeRemovedFromTree();
}

void RenderMultiColumnFlowThread::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // We simply remain at our intrinsic height.
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

LayoutUnit RenderMultiColumnFlowThread::initialLogicalWidth() const
{
    return columnWidth();
}

void RenderMultiColumnFlowThread::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(offset)))
        multicolSet->recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnFlowThread::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(offset)))
        multicolSet->updateMinimumColumnHeight(minHeight);
}

RenderRegion* RenderMultiColumnFlowThread::regionAtBlockOffset(LayoutUnit /*offset*/) const
{
    // For now there's only one column set, so this is easy:
    return firstMultiColumnSet();
}

bool RenderMultiColumnFlowThread::addForcedRegionBreak(LayoutUnit offset, RenderObject* /*breakChild*/, bool /*isBefore*/, LayoutUnit* offsetBreakAdjustment)
{
    if (RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(regionAtBlockOffset(offset))) {
        multicolSet->addForcedBreak(offset);
        if (offsetBreakAdjustment)
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : LayoutUnit();
        return true;
    }
    return false;
}

bool RenderMultiColumnFlowThread::isPageLogicalHeightKnown() const
{
    if (RenderMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->computedColumnHeight();
    return false;
}

}
