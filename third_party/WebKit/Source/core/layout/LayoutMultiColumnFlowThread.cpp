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
#include "core/layout/LayoutMultiColumnFlowThread.h"

#include "core/layout/LayoutMultiColumnSet.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"

namespace blink {

LayoutMultiColumnFlowThread::LayoutMultiColumnFlowThread()
    : m_lastSetWorkedOn(0)
    , m_columnCount(1)
    , m_columnHeightAvailable(0)
    , m_inBalancingPass(false)
    , m_needsColumnHeightsRecalculation(false)
    , m_progressionIsInline(true)
    , m_isBeingEvacuated(false)
{
    setFlowThreadState(InsideInFlowThread);
}

LayoutMultiColumnFlowThread::~LayoutMultiColumnFlowThread()
{
}

LayoutMultiColumnFlowThread* LayoutMultiColumnFlowThread::createAnonymous(Document& document, const LayoutStyle& parentStyle)
{
    LayoutMultiColumnFlowThread* renderer = new LayoutMultiColumnFlowThread();
    renderer->setDocumentForAnonymous(&document);
    renderer->setStyle(LayoutStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK));
    return renderer;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::firstMultiColumnSet() const
{
    for (LayoutObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->isLayoutMultiColumnSet())
            return toLayoutMultiColumnSet(sibling);
    }
    return 0;
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::lastMultiColumnSet() const
{
    for (LayoutObject* sibling = multiColumnBlockFlow()->lastChild(); sibling; sibling = sibling->previousSibling()) {
        if (sibling->isLayoutMultiColumnSet())
            return toLayoutMultiColumnSet(sibling);
    }
    return 0;
}

static LayoutObject* firstRendererInSet(LayoutMultiColumnSet* multicolSet)
{
    LayoutBox* sibling = multicolSet->previousSiblingMultiColumnBox();
    if (!sibling)
        return multicolSet->flowThread()->firstChild();
    // Adjacent column content sets should not occur. We would have no way of figuring out what each
    // of them contains then.
    ASSERT(sibling->isLayoutMultiColumnSpannerPlaceholder());
    return toLayoutMultiColumnSpannerPlaceholder(sibling)->rendererInFlowThread()->nextInPreOrderAfterChildren(multicolSet->flowThread());
}

static LayoutObject* lastRendererInSet(LayoutMultiColumnSet* multicolSet)
{
    LayoutBox* sibling = multicolSet->nextSiblingMultiColumnBox();
    if (!sibling)
        return 0; // By right we should return lastLeafChild() here, but the caller doesn't care, so just return 0.
    // Adjacent column content sets should not occur. We would have no way of figuring out what each
    // of them contains then.
    ASSERT(sibling->isLayoutMultiColumnSpannerPlaceholder());
    return toLayoutMultiColumnSpannerPlaceholder(sibling)->rendererInFlowThread()->previousInPreOrder(multicolSet->flowThread());
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::findSetRendering(LayoutObject* renderer) const
{
    ASSERT(!containingColumnSpannerPlaceholder(renderer)); // should not be used for spanners or content inside them.
    ASSERT(renderer != this);
    ASSERT(renderer->isDescendantOf(this));
    LayoutMultiColumnSet* multicolSet = firstMultiColumnSet();
    if (!multicolSet)
        return 0;
    if (!multicolSet->nextSiblingMultiColumnSet())
        return multicolSet;

    // This is potentially SLOW! But luckily very uncommon. You would have to dynamically insert a
    // spanner into the middle of column contents to need this.
    for (; multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        LayoutObject* firstRenderer = firstRendererInSet(multicolSet);
        LayoutObject* lastRenderer = lastRendererInSet(multicolSet);
        ASSERT(firstRenderer);

        for (LayoutObject* walker = firstRenderer; walker; walker = walker->nextInPreOrder(this)) {
            if (walker == renderer)
                return multicolSet;
            if (walker == lastRenderer)
                break;
        }
    }

    return 0;
}

LayoutMultiColumnSpannerPlaceholder* LayoutMultiColumnFlowThread::containingColumnSpannerPlaceholder(const LayoutObject* descendant) const
{
    ASSERT(descendant->isDescendantOf(this));

    // Before we spend time on searching the ancestry, see if there's a quick way to determine
    // whether there might be any spanners at all.
    LayoutBox* firstBox = firstMultiColumnBox();
    if (!firstBox || (firstBox == lastMultiColumnBox() && firstBox->isLayoutMultiColumnSet()))
        return 0;

    // We have spanners. See if the renderer in question is one or inside of one then.
    for (const LayoutObject* ancestor = descendant; ancestor && ancestor != this; ancestor = ancestor->parent()) {
        if (LayoutMultiColumnSpannerPlaceholder* placeholder = ancestor->spannerPlaceholder())
            return placeholder;
    }
    return 0;
}

void LayoutMultiColumnFlowThread::populate()
{
    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    ASSERT(!nextSibling());
    // Reparent children preceding the flow thread into the flow thread. It's multicol content
    // now. At this point there's obviously nothing after the flow thread, but renderers (column
    // sets and spanners) will be inserted there as we insert elements into the flow thread.
    multicolContainer->moveChildrenTo(this, multicolContainer->firstChild(), this, true);
}

void LayoutMultiColumnFlowThread::evacuateAndDestroy()
{
    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    m_isBeingEvacuated = true;

    // Remove all sets and spanners.
    while (LayoutBox* columnBox = firstMultiColumnBox()) {
        ASSERT(columnBox->isAnonymous());
        columnBox->destroy();
    }

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

LayoutSize LayoutMultiColumnFlowThread::columnOffset(const LayoutPoint& point) const
{
    if (!hasValidRegionInfo())
        return LayoutSize(0, 0);

    LayoutPoint flowThreadPoint = flipForWritingMode(point);
    LayoutUnit blockOffset = isHorizontalWritingMode() ? flowThreadPoint.y() : flowThreadPoint.x();
    LayoutMultiColumnSet* columnSet = columnSetAtBlockOffset(blockOffset);
    if (!columnSet)
        return LayoutSize(0, 0);
    return columnSet->flowThreadTranslationAtOffset(blockOffset);
}

bool LayoutMultiColumnFlowThread::needsNewWidth() const
{
    LayoutUnit newWidth;
    unsigned dummyColumnCount; // We only care if used column-width changes.
    calculateColumnCountAndWidth(newWidth, dummyColumnCount);
    return newWidth != logicalWidth();
}

LayoutMultiColumnSet* LayoutMultiColumnFlowThread::columnSetAtBlockOffset(LayoutUnit offset) const
{
    if (m_lastSetWorkedOn) {
        // Layout in progress. We are calculating the set heights as we speak, so the column set range
        // information is not up-to-date.
        return m_lastSetWorkedOn;
    }

    ASSERT(!m_regionsInvalidated);
    if (m_multiColumnSetList.isEmpty())
        return 0;
    if (offset <= 0)
        return m_multiColumnSetList.first();

    MultiColumnSetSearchAdapter adapter(offset);
    m_multiColumnSetIntervalTree.allOverlapsWithAdapter<MultiColumnSetSearchAdapter>(adapter);

    // If no set was found, the offset is in the flow thread overflow.
    if (!adapter.result() && !m_multiColumnSetList.isEmpty())
        return m_multiColumnSetList.last();
    return adapter.result();
}

void LayoutMultiColumnFlowThread::layoutColumns(bool relayoutChildren, SubtreeLayoutScope& layoutScope)
{
    if (relayoutChildren)
        layoutScope.setChildNeedsLayout(this);

    m_needsColumnHeightsRecalculation = false;
    if (!needsLayout()) {
        // Just before the multicol container (our parent LayoutBlockFlow) finishes laying out, it
        // will call recalculateColumnHeights() on us unconditionally, but we only want that method
        // to do any work if we actually laid out the flow thread. Otherwise, the balancing
        // machinery would kick in needlessly, and trigger additional layout passes. Furthermore, we
        // actually depend on a proper flowthread layout pass in order to do balancing, since it's
        // flowthread layout that sets up content runs.
        return;
    }

    for (LayoutBox* columnBox = firstMultiColumnBox(); columnBox; columnBox = columnBox->nextSiblingMultiColumnBox()) {
        if (!columnBox->isLayoutMultiColumnSet()) {
            ASSERT(columnBox->isLayoutMultiColumnSpannerPlaceholder()); // no other type is expected.
            m_needsColumnHeightsRecalculation = true;
            continue;
        }
        LayoutMultiColumnSet* columnSet = toLayoutMultiColumnSet(columnBox);
        if (!m_inBalancingPass) {
            // This is the initial layout pass. We need to reset the column height, because contents
            // typically have changed.
            columnSet->resetColumnHeight();
        }
        if (!m_needsColumnHeightsRecalculation)
            m_needsColumnHeightsRecalculation = columnSet->heightIsAuto();
    }

    invalidateRegions();
    layout();
}

bool LayoutMultiColumnFlowThread::recalculateColumnHeights()
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
    for (LayoutMultiColumnSet* multicolSet = firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        needsRelayout |= multicolSet->recalculateColumnHeight(m_inBalancingPass ? StretchBySpaceShortage : GuessFromFlowThreadPortion);
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

void LayoutMultiColumnFlowThread::columnRuleStyleDidChange()
{
    for (LayoutMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->setShouldDoFullPaintInvalidation(PaintInvalidationStyleChange);
}

void LayoutMultiColumnFlowThread::calculateColumnCountAndWidth(LayoutUnit& width, unsigned& count) const
{
    LayoutBlock* columnBlock = multiColumnBlockFlow();
    const LayoutStyle* columnStyle = columnBlock->style();
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

void LayoutMultiColumnFlowThread::createAndInsertMultiColumnSet(LayoutBox* insertBefore)
{
    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    LayoutMultiColumnSet* newSet = LayoutMultiColumnSet::createAnonymous(*this, multicolContainer->styleRef());
    multicolContainer->LayoutBlock::addChild(newSet, insertBefore);
    invalidateRegions();

    // We cannot handle immediate column set siblings (and there's no need for it, either).
    // There has to be at least one spanner separating them.
    ASSERT(!newSet->previousSiblingMultiColumnBox() || !newSet->previousSiblingMultiColumnBox()->isLayoutMultiColumnSet());
    ASSERT(!newSet->nextSiblingMultiColumnBox() || !newSet->nextSiblingMultiColumnBox()->isLayoutMultiColumnSet());
}

void LayoutMultiColumnFlowThread::createAndInsertSpannerPlaceholder(LayoutBox* spanner, LayoutBox* insertBefore)
{
    LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    LayoutMultiColumnSpannerPlaceholder* newPlaceholder = LayoutMultiColumnSpannerPlaceholder::createAnonymous(multicolContainer->styleRef(), *spanner);
    multicolContainer->LayoutBlock::addChild(newPlaceholder, insertBefore);
    spanner->setSpannerPlaceholder(*newPlaceholder);
}

bool LayoutMultiColumnFlowThread::descendantIsValidColumnSpanner(LayoutObject* descendant) const
{
    // We assume that we're inside the flow thread. This function is not to be called otherwise.
    ASSERT(descendant->isDescendantOf(this));

    // We're evaluating if the descendant should be turned into a proper spanner. It shouldn't
    // already be one.
    ASSERT(!descendant->spannerPlaceholder());

    // The spec says that column-span only applies to in-flow block-level elements.
    if (descendant->style()->columnSpan() != ColumnSpanAll || !descendant->isBox() || descendant->isInline() || descendant->isFloatingOrOutOfFlowPositioned())
        return false;

    if (!descendant->containingBlock()->isLayoutBlockFlow()) {
        // Needs to be in a block-flow container, and not e.g. a table.
        return false;
    }

    // This looks like a spanner, but if we're inside something unbreakable, it's not to be treated as one.
    for (LayoutBlock* ancestor = descendant->containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (ancestor->isLayoutFlowThread()) {
            ASSERT(ancestor == this);
            return true;
        }
        if (ancestor->spannerPlaceholder()) {
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

void LayoutMultiColumnFlowThread::addRegionToThread(LayoutMultiColumnSet* columnSet)
{
    if (LayoutMultiColumnSet* nextSet = columnSet->nextSiblingMultiColumnSet()) {
        LayoutMultiColumnSetList::iterator it = m_multiColumnSetList.find(nextSet);
        ASSERT(it != m_multiColumnSetList.end());
        m_multiColumnSetList.insertBefore(it, columnSet);
    } else {
        m_multiColumnSetList.add(columnSet);
    }
    columnSet->setIsValid(true);
}

void LayoutMultiColumnFlowThread::willBeRemovedFromTree()
{
    // Detach all column sets from the flow thread. Cannot destroy them at this point, since they
    // are siblings of this object, and there may be pointers to this object's sibling somewhere
    // further up on the call stack.
    for (LayoutMultiColumnSet* columnSet = firstMultiColumnSet(); columnSet; columnSet = columnSet->nextSiblingMultiColumnSet())
        columnSet->detachRegion();
    multiColumnBlockFlow()->resetMultiColumnFlowThread();
    LayoutFlowThread::willBeRemovedFromTree();
}

LayoutUnit LayoutMultiColumnFlowThread::skipColumnSpanner(LayoutBox* renderer, LayoutUnit logicalTopInFlowThread)
{
    ASSERT(renderer->isColumnSpanAll());
    LayoutMultiColumnSpannerPlaceholder* placeholder = renderer->spannerPlaceholder();
    LayoutUnit adjustment;
    LayoutBox* previousColumnBox = placeholder->previousSiblingMultiColumnBox();
    if (previousColumnBox && previousColumnBox->isLayoutMultiColumnSet()) {
        // Pad flow thread offset to a column boundary, so that any column content that's supposed
        // to come after the spanner doesn't bleed into the column row preceding the spanner.
        LayoutMultiColumnSet* previousSet = toLayoutMultiColumnSet(previousColumnBox);
        if (previousSet->pageLogicalHeight()) {
            LayoutUnit columnLogicalTopInFlowThread = previousSet->pageLogicalTopForOffset(logicalTopInFlowThread);
            if (columnLogicalTopInFlowThread != logicalTopInFlowThread) {
                adjustment = columnLogicalTopInFlowThread + previousSet->pageLogicalHeight() - logicalTopInFlowThread;
                logicalTopInFlowThread += adjustment;
            }
        }
        previousSet->endFlow(logicalTopInFlowThread);
    }
    LayoutBox* nextColumnBox = placeholder->nextSiblingMultiColumnBox();
    if (nextColumnBox && nextColumnBox->isLayoutMultiColumnSet()) {
        LayoutMultiColumnSet* nextSet = toLayoutMultiColumnSet(nextColumnBox);
        m_lastSetWorkedOn = nextSet;
        nextSet->beginFlow(logicalTopInFlowThread);
    }
    return adjustment;
}

void LayoutMultiColumnFlowThread::flowThreadDescendantWasInserted(LayoutObject* descendant)
{
    ASSERT(!m_isBeingEvacuated);
    LayoutObject* nextRenderer = descendant->nextInPreOrderAfterChildren(this);
    // This method ensures that the list of column sets and spanner placeholders reflects the
    // multicol content after having inserted a descendant (or descendant subtree). See the header
    // file for more information. Go through the subtree that was just inserted and create column
    // sets (needed by regular column content) and spanner placeholders (one needed by each spanner)
    // where needed.
    for (LayoutObject* renderer = descendant; renderer; renderer = renderer->nextInPreOrder(descendant)) {
        if (containingColumnSpannerPlaceholder(renderer))
            continue; // Inside a column spanner. Nothing to do, then.
        if (descendantIsValidColumnSpanner(renderer)) {
            // This renderer is a spanner, so it needs to establish a spanner placeholder.
            LayoutBox* insertBefore = 0;
            LayoutMultiColumnSet* setToSplit = 0;
            if (nextRenderer) {
                // The spanner is inserted before something. Figure out what this entails. If the
                // next renderer is a spanner too, it means that we can simply insert a new spanner
                // placeholder in front of its placeholder.
                insertBefore = nextRenderer->spannerPlaceholder();
                if (!insertBefore) {
                    // The next renderer isn't a spanner; it's regular column content. Examine what
                    // comes right before us in the flow thread, then.
                    LayoutObject* previousRenderer = renderer->previousInPreOrder(this);
                    if (!previousRenderer || previousRenderer == this) {
                        // The spanner is inserted as the first child of the multicol container,
                        // which means that we simply insert a new spanner placeholder at the
                        // beginning.
                        insertBefore = firstMultiColumnBox();
                    } else if (LayoutMultiColumnSpannerPlaceholder* previousPlaceholder = containingColumnSpannerPlaceholder(previousRenderer)) {
                        // Before us is another spanner. We belong right after it then.
                        insertBefore = previousPlaceholder->nextSiblingMultiColumnBox();
                    } else {
                        // We're inside regular column content with both feet. Find out which column
                        // set this is. It needs to be split it into two sets, so that we can insert
                        // a new spanner placeholder between them.
                        setToSplit = findSetRendering(previousRenderer);
                        ASSERT(setToSplit == findSetRendering(nextRenderer));
                        setToSplit->setNeedsLayoutAndFullPaintInvalidation();
                        insertBefore = setToSplit->nextSiblingMultiColumnBox();
                        // We've found out which set that needs to be split. Now proceed to
                        // inserting the spanner placeholder, and then insert a second column set.
                    }
                }
                ASSERT(setToSplit || insertBefore);
            }
            createAndInsertSpannerPlaceholder(toLayoutBox(renderer), insertBefore);
            if (setToSplit)
                createAndInsertMultiColumnSet(insertBefore);
            continue;
        }
        // This renderer is regular column content (i.e. not a spanner). Create a set if necessary.
        if (nextRenderer) {
            if (LayoutMultiColumnSpannerPlaceholder* placeholder = nextRenderer->spannerPlaceholder()) {
                // If inserted right before a spanner, we need to make sure that there's a set for us there.
                LayoutBox* previous = placeholder->previousSiblingMultiColumnBox();
                if (!previous || !previous->isLayoutMultiColumnSet())
                    createAndInsertMultiColumnSet(placeholder);
            } else {
                // Otherwise, since |nextRenderer| isn't a spanner, it has to mean that there's
                // already a set for that content. We can use it for this renderer too.
                ASSERT(findSetRendering(nextRenderer));
                ASSERT(findSetRendering(renderer) == findSetRendering(nextRenderer));
            }
        } else {
            // Inserting at the end. Then we just need to make sure that there's a column set at the end.
            LayoutBox* lastColumnBox = lastMultiColumnBox();
            if (!lastColumnBox || !lastColumnBox->isLayoutMultiColumnSet())
                createAndInsertMultiColumnSet();
        }
    }
}

void LayoutMultiColumnFlowThread::flowThreadDescendantWillBeRemoved(LayoutObject* descendant)
{
    // This method ensures that the list of column sets and spanner placeholders reflects the
    // multicol content that we'll be left with after removal of a descendant (or descendant
    // subtree). See the header file for more information. Removing content may mean that we need to
    // remove column sets and/or spanner placeholders.
    if (m_isBeingEvacuated)
        return;
    bool hadContainingPlaceholder = containingColumnSpannerPlaceholder(descendant);
    LayoutObject* next;
    // Remove spanner placeholders that are no longer needed, and merge column sets around them.
    for (LayoutObject* renderer = descendant; renderer; renderer = next) {
        LayoutMultiColumnSpannerPlaceholder* placeholder = renderer->spannerPlaceholder();
        if (!placeholder) {
            next = renderer->nextInPreOrder(descendant);
            continue;
        }
        next = renderer->nextInPreOrderAfterChildren(descendant); // It's a spanner. Its children are of no interest to us.
        if (LayoutBox* nextColumnBox = placeholder->nextSiblingMultiColumnBox()) {
            LayoutBox* previousColumnBox = placeholder->previousSiblingMultiColumnBox();
            if (nextColumnBox && nextColumnBox->isLayoutMultiColumnSet()
                && previousColumnBox && previousColumnBox->isLayoutMultiColumnSet()) {
                // Need to merge two column sets.
                nextColumnBox->destroy();
                previousColumnBox->setNeedsLayout();
                invalidateRegions();
            }
        }
        placeholder->destroy();
    }
    if (hadContainingPlaceholder)
        return; // We're only removing a spanner (or something inside one), which means that no column content will be removed.

    // Column content will be removed. Does this mean that we should destroy a column set?
    LayoutMultiColumnSpannerPlaceholder* adjacentPreviousSpannerPlaceholder = 0;
    LayoutObject* previousRenderer = descendant->previousInPreOrder(this);
    if (previousRenderer && previousRenderer != this) {
        adjacentPreviousSpannerPlaceholder = containingColumnSpannerPlaceholder(previousRenderer);
        if (!adjacentPreviousSpannerPlaceholder)
            return; // Preceded by column content. Set still needed.
    }
    LayoutMultiColumnSpannerPlaceholder* adjacentNextSpannerPlaceholder = 0;
    LayoutObject* nextRenderer = descendant->nextInPreOrderAfterChildren(this);
    if (nextRenderer) {
        adjacentNextSpannerPlaceholder = containingColumnSpannerPlaceholder(nextRenderer);
        if (!adjacentNextSpannerPlaceholder)
            return; // Followed by column content. Set still needed.
    }
    // We have now determined that, with the removal of |descendant|, we should remove a column
    // set. Locate it and remove it. Do it without involving findSetRendering(), as that might be
    // very slow. Deduce the right set from the spanner placeholders that we've already found.
    LayoutMultiColumnSet* columnSetToRemove;
    if (adjacentNextSpannerPlaceholder) {
        columnSetToRemove = toLayoutMultiColumnSet(adjacentNextSpannerPlaceholder->previousSiblingMultiColumnBox());
        ASSERT(!adjacentPreviousSpannerPlaceholder || columnSetToRemove == adjacentPreviousSpannerPlaceholder->nextSiblingMultiColumnBox());
    } else if (adjacentPreviousSpannerPlaceholder) {
        columnSetToRemove = toLayoutMultiColumnSet(adjacentPreviousSpannerPlaceholder->nextSiblingMultiColumnBox());
    } else {
        // If there were no adjacent spanners, it has to mean that there's only one column set,
        // since it's only spanners that may cause creation of multiple sets.
        columnSetToRemove = firstMultiColumnSet();
        ASSERT(columnSetToRemove);
        ASSERT(!columnSetToRemove->nextSiblingMultiColumnSet());
    }
    ASSERT(columnSetToRemove);
    columnSetToRemove->destroy();
}

void LayoutMultiColumnFlowThread::computePreferredLogicalWidths()
{
    LayoutFlowThread::computePreferredLogicalWidths();

    // The min/max intrinsic widths calculated really tell how much space elements need when
    // laid out inside the columns. In order to eventually end up with the desired column width,
    // we need to convert them to values pertaining to the multicol container.
    const LayoutBlockFlow* multicolContainer = multiColumnBlockFlow();
    const LayoutStyle* multicolStyle = multicolContainer->style();
    int columnCount = multicolStyle->hasAutoColumnCount() ? 1 : multicolStyle->columnCount();
    LayoutUnit columnWidth;
    LayoutUnit gapExtra = (columnCount - 1) * multicolContainer->columnGap();
    if (multicolStyle->hasAutoColumnWidth()) {
        m_minPreferredLogicalWidth = m_minPreferredLogicalWidth * columnCount + gapExtra;
    } else {
        columnWidth = multicolStyle->columnWidth();
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, columnWidth);
    }
    // Note that if column-count is auto here, we should resolve it to calculate the maximum
    // intrinsic width, instead of pretending that it's 1. The only way to do that is by performing
    // a layout pass, but this is not an appropriate time or place for layout. The good news is that
    // if height is unconstrained and there are no explicit breaks, the resolved column-count really
    // should be 1.
    m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, columnWidth) * columnCount + gapExtra;
}

void LayoutMultiColumnFlowThread::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    // We simply remain at our intrinsic height.
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

void LayoutMultiColumnFlowThread::updateLogicalWidth()
{
    LayoutUnit columnWidth;
    calculateColumnCountAndWidth(columnWidth, m_columnCount);
    setLogicalWidth(columnWidth);
}

void LayoutMultiColumnFlowThread::layout()
{
    ASSERT(!m_lastSetWorkedOn);
    m_lastSetWorkedOn = firstMultiColumnSet();
    if (m_lastSetWorkedOn)
        m_lastSetWorkedOn->beginFlow(LayoutUnit());
    LayoutFlowThread::layout();
    if (LayoutMultiColumnSet* lastSet = lastMultiColumnSet()) {
        ASSERT(lastSet == m_lastSetWorkedOn);
        if (!lastSet->nextSiblingMultiColumnBox()) {
            lastSet->endFlow(logicalHeight());
            lastSet->expandToEncompassFlowThreadContentsIfNeeded();
        }
    }
    m_lastSetWorkedOn = 0;
}

void LayoutMultiColumnFlowThread::setPageBreak(LayoutUnit offset, LayoutUnit spaceShortage)
{
    // Only positive values are interesting (and allowed) here. Zero space shortage may be reported
    // when we're at the top of a column and the element has zero height. Ignore this, and also
    // ignore any negative values, which may occur when we set an early break in order to honor
    // widows in the next column.
    if (spaceShortage <= 0)
        return;

    if (LayoutMultiColumnSet* multicolSet = columnSetAtBlockOffset(offset))
        multicolSet->recordSpaceShortage(offset, spaceShortage);
}

void LayoutMultiColumnFlowThread::updateMinimumPageHeight(LayoutUnit offset, LayoutUnit minHeight)
{
    if (LayoutMultiColumnSet* multicolSet = columnSetAtBlockOffset(offset))
        multicolSet->updateMinimumColumnHeight(offset, minHeight);
}

bool LayoutMultiColumnFlowThread::addForcedRegionBreak(LayoutUnit offset, LayoutObject* /*breakChild*/, bool /*isBefore*/, LayoutUnit* offsetBreakAdjustment)
{
    if (LayoutMultiColumnSet* multicolSet = columnSetAtBlockOffset(offset)) {
        multicolSet->addContentRun(offset);
        if (offsetBreakAdjustment)
            *offsetBreakAdjustment = pageLogicalHeightForOffset(offset) ? pageRemainingLogicalHeightForOffset(offset, IncludePageBoundary) : LayoutUnit();
        return true;
    }
    return false;
}

bool LayoutMultiColumnFlowThread::isPageLogicalHeightKnown() const
{
    if (LayoutMultiColumnSet* columnSet = lastMultiColumnSet())
        return columnSet->pageLogicalHeight();
    return false;
}

}
