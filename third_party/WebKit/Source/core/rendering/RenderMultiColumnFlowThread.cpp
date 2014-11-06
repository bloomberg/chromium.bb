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
#include "core/rendering/RenderMultiColumnSpannerSet.h"

namespace blink {

RenderMultiColumnFlowThread::RenderMultiColumnFlowThread()
    : m_columnCount(1)
    , m_columnHeightAvailable(0)
    , m_inBalancingPass(false)
    , m_needsColumnHeightsRecalculation(false)
    , m_progressionIsInline(true)
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

RenderMultiColumnSpannerSet* RenderMultiColumnFlowThread::containingColumnSpannerSet(const RenderObject* descendant) const
{
    ASSERT(descendant->isDescendantOf(this));

    // Before we spend time on searching the ancestry, see if there's a quick way to determine
    // whether there might be any spanners at all.
    RenderMultiColumnSet* firstSet = firstMultiColumnSet();
    if (!firstSet || (firstSet == lastMultiColumnSet() && !firstSet->isRenderMultiColumnSpannerSet()))
        return 0;

    // We have spanners. See if the renderer in question is one or inside of one then.
    for (const RenderObject* ancestor = descendant; ancestor && ancestor != this; ancestor = ancestor->parent()) {
        if (RenderMultiColumnSpannerSet* spanner = m_spannerMap.get(ancestor))
            return spanner;
    }
    return 0;
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

    LayoutPoint flowThreadPoint = flipForWritingMode(point);
    LayoutUnit blockOffset = isHorizontalWritingMode() ? flowThreadPoint.y() : flowThreadPoint.x();
    RenderMultiColumnSet* columnSet = columnSetAtBlockOffset(blockOffset);
    if (!columnSet)
        return LayoutSize(0, 0);
    return columnSet->flowThreadTranslationAtOffset(blockOffset);
}

bool RenderMultiColumnFlowThread::needsNewWidth() const
{
    LayoutUnit newWidth;
    unsigned dummyColumnCount; // We only care if used column-width changes.
    calculateColumnCountAndWidth(newWidth, dummyColumnCount);
    return newWidth != logicalWidth();
}

void RenderMultiColumnFlowThread::layoutColumns(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    if (relayoutChildren)
        layoutScope.setChildNeedsLayout(this);

    if (!needsLayout()) {
        // Just before the multicol container (our parent RenderBlockFlow) finishes laying out, it
        // will call recalculateColumnHeights() on us unconditionally, but we only want that method
        // to do any work if we actually laid out the flow thread. Otherwise, the balancing
        // machinery would kick in needlessly, and trigger additional layout passes. Furthermore, we
        // actually depend on a proper flowthread layout pass in order to do balancing, since it's
        // flowthread layout that sets up content runs.
        m_needsColumnHeightsRecalculation = false;
        return;
    }

    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet()) {
        if (!m_inBalancingPass) {
            // This is the initial layout pass. We need to reset the column height, because contents
            // typically have changed.
            columnSet->resetColumnHeight();
        }
    }

    invalidateRegions();
    m_needsColumnHeightsRecalculation = heightIsAuto();
    layout();
}

bool RenderMultiColumnFlowThread::recalculateColumnHeights()
{
    // All column sets that needed layout have now been laid out, so we can finally validate them.
    validateRegions();

    if (!m_needsColumnHeightsRecalculation)
        return false;

    // Column heights may change here because of balancing. We may have to do multiple layout
    // passes, depending on how the contents is fitted to the changed column heights. In most
    // cases, laying out again twice or even just once will suffice. Sometimes we need more
    // passes than that, though, but the number of retries should not exceed the number of
    // columns, unless we have a bug.
    bool needsRelayout = false;
    for (RenderMultiColumnSet* multicolSet = firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        needsRelayout |= multicolSet->recalculateColumnHeight(m_inBalancingPass ? RenderMultiColumnSet::StretchBySpaceShortage : RenderMultiColumnSet::GuessFromFlowThreadPortion);
        if (needsRelayout) {
            // Once a column set gets a new column height, that column set and all successive column
            // sets need to be laid out over again, since their logical top will be affected by
            // this, and therefore their column heights may change as well, at least if the multicol
            // height is constrained.
            multicolSet->setChildNeedsLayout(MarkOnlyThis);
        }
    }

    if (needsRelayout)
        setChildNeedsLayout(MarkOnlyThis);

    m_inBalancingPass = needsRelayout;
    return needsRelayout;
}

void RenderMultiColumnFlowThread::calculateColumnCountAndWidth(LayoutUnit& width, unsigned& count) const
{
    RenderBlock* columnBlock = multiColumnBlockFlow();
    const RenderStyle* columnStyle = columnBlock->style();
    LayoutUnit availableWidth = columnBlock->contentLogicalWidth();
    LayoutUnit columnGap = columnBlock->columnGap();
    LayoutUnit computedColumnWidth = max<LayoutUnit>(1, LayoutUnit(columnStyle->columnWidth()));
    unsigned computedColumnCount = max<int>(1, columnStyle->columnCount());

    ASSERT(!columnStyle->hasAutoColumnCount() || !columnStyle->hasAutoColumnWidth());
    if (columnStyle->hasAutoColumnWidth() && !columnStyle->hasAutoColumnCount()) {
        count = computedColumnCount;
        width = std::max<LayoutUnit>(0, (availableWidth - ((count - 1) * columnGap)) / count);
    } else if (!columnStyle->hasAutoColumnWidth() && columnStyle->hasAutoColumnCount()) {
        count = std::max<LayoutUnit>(1, (availableWidth + columnGap) / (computedColumnWidth + columnGap));
        width = ((availableWidth + columnGap) / count) - columnGap;
    } else {
        count = std::max<LayoutUnit>(std::min<LayoutUnit>(computedColumnCount, (availableWidth + columnGap) / (computedColumnWidth + columnGap)), 1);
        width = ((availableWidth + columnGap) / count) - columnGap;
    }
}

void RenderMultiColumnFlowThread::createAndInsertMultiColumnSet()
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
    RenderMultiColumnSet* newSet = RenderMultiColumnSet::createAnonymous(this, multicolContainer->style());
    multicolContainer->RenderBlock::addChild(newSet);
    invalidateRegions();

    // We cannot handle immediate column set siblings (and there's no need for it, either).
    // There has to be at least one spanner separating them.
    ASSERT(!newSet->previousSiblingMultiColumnSet() || newSet->previousSiblingMultiColumnSet()->isRenderMultiColumnSpannerSet());
    ASSERT(!newSet->nextSiblingMultiColumnSet() || newSet->nextSiblingMultiColumnSet()->isRenderMultiColumnSpannerSet());
}

void RenderMultiColumnFlowThread::createAndInsertSpannerSet(RenderBox* spanner)
{
    RenderBlockFlow* multicolContainer = multiColumnBlockFlow();
    RenderMultiColumnSpannerSet* newSpannerSet = RenderMultiColumnSpannerSet::createAnonymous(this, multicolContainer->style(), spanner);
    multicolContainer->RenderBlock::addChild(newSpannerSet);
    m_spannerMap.add(spanner, newSpannerSet);
    invalidateRegions();
}

bool RenderMultiColumnFlowThread::descendantIsValidColumnSpanner(RenderObject* descendant) const
{
    // We assume that we're inside the flow thread. This function is not to be called otherwise.
    ASSERT(descendant->isDescendantOf(this));

    // The spec says that column-span only applies to in-flow block-level elements.
    if (descendant->style()->columnSpan() != ColumnSpanAll || !descendant->isBox() || descendant->isInline() || descendant->isFloatingOrOutOfFlowPositioned())
        return false;

    if (!descendant->containingBlock()->isRenderBlockFlow()) {
        // Needs to be in a block-flow container, and not e.g. a table.
        return false;
    }

    // This looks like a spanner, but if we're inside something unbreakable, it's not to be treated as one.
    for (RenderBlock* ancestor = descendant->containingBlock(); ancestor && ancestor->flowThreadContainingBlock() == this; ancestor = ancestor->containingBlock()) {
        if (ancestor->isRenderFlowThread()) {
            ASSERT(ancestor == this);
            return true;
        }
        if (m_spannerMap.get(ancestor)) {
            // FIXME: do we want to support nested spanners in a different way? The outer spanner
            // has already broken out from the columns to become sized by the multicol container,
            // which may be good enough for the inner spanner. But margins, borders, padding and
            // explicit widths on the outer spanner, or on any children between the outer and inner
            // spanner, will affect the width of the inner spanner this way, which might be
            // undesirable. The spec has nothing to say on the matter.
            return false; // Ignore nested spanners.
        }
        if (ancestor->isUnsplittableForPagination())
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

const char* RenderMultiColumnFlowThread::renderName() const
{
    return "RenderMultiColumnFlowThread";
}

void RenderMultiColumnFlowThread::addRegionToThread(RenderMultiColumnSet* columnSet)
{
    if (RenderMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        RenderMultiColumnSetList::iterator it = m_multiColumnSetList.find(nextSet);
        ASSERT(it != m_multiColumnSetList.end());
        m_multiColumnSetList.insertBefore(it, columnSet);
    } else {
        m_multiColumnSetList.add(columnSet);
    }
    columnSet->setIsValid(true);
}

void RenderMultiColumnFlowThread::willBeRemovedFromTree()
{
    m_spannerMap.clear();
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (RenderMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachRegion();
    multiColumnBlockFlow()->resetMultiColumnFlowThread();
    RenderFlowThread::willBeRemovedFromTree();
}

void RenderMultiColumnFlowThread::flowThreadDescendantWasInserted(RenderObject* descendant)
{
    // Go through the subtree that was just inserted and create column sets (needed by regular
    // column content) and spanner sets (one needed by each spanner).
    for (RenderObject* renderer = descendant; renderer; renderer = renderer->nextInPreOrder(descendant)) {
        if (containingColumnSpannerSet(renderer))
            continue; // Inside a column spanner set. Nothing to do, then.
        if (descendantIsValidColumnSpanner(renderer)) {
            // This renderer is a spanner, so it needs to establish a spanner set.
            createAndInsertSpannerSet(toRenderBox(renderer));
            continue;
        }
        // This renderer is regular column content (i.e. not a spanner). Create a set if necessary.
        RenderMultiColumnSet* lastSet = lastMultiColumnSet();
        if (!lastSet || lastSet->isRenderMultiColumnSpannerSet())
            createAndInsertMultiColumnSet();
    }
}

void RenderMultiColumnFlowThread::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // We simply remain at our intrinsic height.
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

void RenderMultiColumnFlowThread::updateLogicalWidth()
{
    LayoutUnit columnWidth;
    calculateColumnCountAndWidth(columnWidth, m_columnCount);
    setLogicalWidth(columnWidth);
}

void RenderMultiColumnFlowThread::layout()
{
    RenderFlowThread::layout();
    if (RenderMultiColumnSet* lastSet = lastMultiColumnSet())
        lastSet->expandToEncompassFlowThreadContentsIfNeeded();
}

void RenderMultiColumnFlowThread::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    // Only positive values are interesting (and allowed) here. Zero space shortage may be reported
    // when we're at the top of a column and the element has zero height. Ignore this, and also
    // ignore any negative values, which may occur when we set an early break in order to honor
    // widows in the next column.
    if (spaceShortage <= 0)
        return;

    if (RenderMultiColumnSet* multicolSet = columnSetAtBlockOffset(offset))
        multicolSet->recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnFlowThread::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (RenderMultiColumnSet* multicolSet = columnSetAtBlockOffset(offset))
        multicolSet->updateMinimumColumnHeight(minHeight);
}

RenderMultiColumnSet* RenderMultiColumnFlowThread::columnSetAtBlockOffset(LayoutUnit /*offset*/) const
{
    // For now there's only one column set, so this is easy:
    return firstMultiColumnSet();
}

bool RenderMultiColumnFlowThread::addForcedRegionBreak(LayoutUnit offset, RenderObject* /*breakChild*/, bool /*isBefore*/, LayoutUnit* offsetBreakAdjustment)
{
    if (RenderMultiColumnSet* multicolSet = columnSetAtBlockOffset(offset)) {
        multicolSet->addContentRun(offset);
        if (offsetBreakAdjustment)
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : LayoutUnit();
        return true;
    }
    return false;
}

bool RenderMultiColumnFlowThread::isPageLogicalHeightKnown() const
{
    if (RenderMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->pageLogicalHeight();
    return false;
}

}
