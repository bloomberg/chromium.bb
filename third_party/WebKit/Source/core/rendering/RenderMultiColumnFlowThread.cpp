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
    , m_needsRebalancing(false)
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
    for (RenderBox* sibling = nextSiblingBox(); sibling;) {
        RenderBox* nextSibling = sibling->nextSiblingBox();
        if (sibling->isRenderMultiColumnSet())
            sibling->destroy();
        sibling = nextSibling;
    }

    ASSERT(!previousSibling());
    ASSERT(!nextSibling());

    // Finally we can promote all flow thread's children. Before we move them to the flow thread's
    // container, we need to unregister the flow thread, so that they aren't just re-added again to
    // the flow thread that we're trying to empty.
    multicolContainer->resetMultiColumnFlowThread();
    moveAllChildrenTo(multicolContainer, true);

    destroy();
}

void RenderMultiColumnFlowThread::layoutColumns(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    // Update the dimensions of our regions before we lay out the flow thread.
    // FIXME: Eventually this is going to get way more complicated, and we will be destroying regions
    // instead of trying to keep them around.
    RenderBlockFlow* container = multiColumnBlockFlow();
    bool shouldInvalidateRegions = false;
    for (RenderBox* childBox = container->firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
        if (childBox == this)
            continue;

        if (relayoutChildren || childBox->needsLayout()) {
            if (!m_inBalancingPass && childBox->isRenderMultiColumnSet())
                toRenderMultiColumnSet(childBox)->prepareForLayout();
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
        m_needsRebalancing = shouldInvalidateRegions || needsLayout();
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
    if (!m_needsRebalancing)
        return false;

    // Column heights may change here because of balancing. We may have to do multiple layout
    // passes, depending on how the contents is fitted to the changed column heights. In most
    // cases, laying out again twice or even just once will suffice. Sometimes we need more
    // passes than that, though, but the number of retries should not exceed the number of
    // columns, unless we have a bug.
    bool needsRelayout = false;
    for (RenderBox* childBox = multiColumnBlockFlow()->firstChildBox(); childBox; childBox = childBox->nextSiblingBox()) {
        if (childBox != this && childBox->isRenderMultiColumnSet()) {
            RenderMultiColumnSet* multicolSet = toRenderMultiColumnSet(childBox);
            if (multicolSet->recalculateBalancedHeight(!m_inBalancingPass)) {
                multicolSet->setChildNeedsLayout(MarkOnlyThis);
                needsRelayout = true;
            }
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

void RenderMultiColumnFlowThread::autoGenerateRegionsToBlockOffset(LayoutUnit /*offset*/)
{
    // This function ensures we have the correct column set information at all times.
    // For a simple multi-column layout in continuous media, only one column set child is required.
    // Once a column is nested inside an enclosing pagination context, the number of column sets
    // required becomes 2n-1, where n is the total number of nested pagination contexts. For example:
    //
    // Column layout with no enclosing pagination model = 2 * 1 - 1 = 1 column set.
    // Columns inside pages = 2 * 2 - 1 = 3 column sets (bottom of first page, all the subsequent pages, then the last page).
    // Columns inside columns inside pages = 2 * 3 - 1 = 5 column sets.
    //
    // In addition, column spans will force a column set to "split" into before/after sets around the spanning element.
    //
    // Finally, we will need to deal with columns inside regions. If regions have variable widths, then there will need
    // to be unique column sets created inside any region whose width is different from its surrounding regions. This is
    // actually pretty similar to the spanning case, in that we break up the column sets whenever the width varies.
    //
    // FIXME: For now just make one column set. This matches the old multi-column code.
    // Right now our goal is just feature parity with the old multi-column code so that we can switch over to the
    // new code as soon as possible.
    RenderMultiColumnSet* firstSet = toRenderMultiColumnSet(firstRegion());
    if (firstSet)
        return;

    invalidateRegions();

    RenderBlockFlow* parentBlock = multiColumnBlockFlow();
    firstSet = RenderMultiColumnSet::createAnonymous(this);
    firstSet->setStyle(RenderStyle::createAnonymousStyleWithDisplay(parentBlock->style(), BLOCK));
    parentBlock->RenderBlock::addChild(firstSet);

    // Even though we aren't placed yet, we can go ahead and set up our size. At this point we're
    // typically in the middle of laying out the thread, attempting to paginate, and we need to do
    // some rudimentary "layout" of the set now, so that pagination will work.
    firstSet->prepareForLayout();

    validateRegions();
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
    for (RenderBox* renderer = parentBox()->lastChildBox(); renderer; renderer = renderer->previousSiblingBox()) {
        if (renderer->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(renderer)->computedColumnHeight();
    }
    // A column set hasn't been created yet. Height may already be known if column-fill is 'auto', though.
    return !requiresBalancing();
}

}
