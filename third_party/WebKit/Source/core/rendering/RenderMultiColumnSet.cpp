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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderMultiColumnSet.h"

#include "core/layout/MultiColumnFragmentainerGroup.h"
#include "core/paint/MultiColumnSetPainter.h"
#include "core/rendering/RenderMultiColumnFlowThread.h"

namespace blink {

RenderMultiColumnSet::RenderMultiColumnSet(RenderFlowThread* flowThread)
    : RenderRegion(0, flowThread)
    , m_fragmentainerGroups(*this)
{
}

RenderMultiColumnSet* RenderMultiColumnSet::createAnonymous(RenderFlowThread& flowThread, const LayoutStyle& parentStyle)
{
    Document& document = flowThread.document();
    RenderMultiColumnSet* renderer = new RenderMultiColumnSet(&flowThread);
    renderer->setDocumentForAnonymous(&document);
    renderer->setStyle(LayoutStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK));
    return renderer;
}

MultiColumnFragmentainerGroup& RenderMultiColumnSet::fragmentainerGroupAtFlowThreadOffset(LayoutUnit)
{
    // FIXME: implement this, once we have support for multiple rows.
    return m_fragmentainerGroups.first();
}

const MultiColumnFragmentainerGroup& RenderMultiColumnSet::fragmentainerGroupAtFlowThreadOffset(LayoutUnit) const
{
    // FIXME: implement this, once we have support for multiple rows.
    return m_fragmentainerGroups.first();
}

LayoutUnit RenderMultiColumnSet::pageLogicalHeight() const
{
    // FIXME: pageLogicalHeight() needs to take a flow thread offset parameter, so that we can
    // locate the right row.
    return firstFragmentainerGroup().logicalHeight();
}

RenderMultiColumnSet* RenderMultiColumnSet::nextSiblingMultiColumnSet() const
{
    for (LayoutObject* sibling = nextSibling(); sibling; sibling = sibling->nextSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return 0;
}

RenderMultiColumnSet* RenderMultiColumnSet::previousSiblingMultiColumnSet() const
{
    for (LayoutObject* sibling = previousSibling(); sibling; sibling = sibling->previousSibling()) {
        if (sibling->isRenderMultiColumnSet())
            return toRenderMultiColumnSet(sibling);
    }
    return 0;
}

LayoutUnit RenderMultiColumnSet::logicalTopInFlowThread() const
{
    return firstFragmentainerGroup().logicalTopInFlowThread();
}

LayoutUnit RenderMultiColumnSet::logicalBottomInFlowThread() const
{
    return lastFragmentainerGroup().logicalBottomInFlowThread();
}

bool RenderMultiColumnSet::heightIsAuto() const
{
    RenderMultiColumnFlowThread* flowThread = multiColumnFlowThread();
    if (!flowThread->isRenderPagedFlowThread()) {
        if (multiColumnBlockFlow()->style()->columnFill() == ColumnFillBalance)
            return true;
        if (RenderBox* next = nextSiblingBox()) {
            if (next->isRenderMultiColumnSpannerPlaceholder()) {
                // If we're followed by a spanner, we need to balance.
                return true;
            }
        }
    }
    return !flowThread->columnHeightAvailable();
}

LayoutSize RenderMultiColumnSet::flowThreadTranslationAtOffset(LayoutUnit blockOffset) const
{
    const MultiColumnFragmentainerGroup& row = fragmentainerGroupAtFlowThreadOffset(blockOffset);
    return row.offsetFromColumnSet() + row.flowThreadTranslationAtOffset(blockOffset);
}

void RenderMultiColumnSet::updateMinimumColumnHeight(LayoutUnit offsetInFlowThread, LayoutUnit height)
{
    fragmentainerGroupAtFlowThreadOffset(offsetInFlowThread).updateMinimumColumnHeight(height);
}

LayoutUnit RenderMultiColumnSet::pageLogicalTopForOffset(LayoutUnit offset) const
{
    return fragmentainerGroupAtFlowThreadOffset(offset).columnLogicalTopForOffset(offset);
}

void RenderMultiColumnSet::addContentRun(LayoutUnit endOffsetFromFirstPage)
{
    if (!heightIsAuto())
        return;
    fragmentainerGroupAtFlowThreadOffset(endOffsetFromFirstPage).addContentRun(endOffsetFromFirstPage);
}

bool RenderMultiColumnSet::recalculateColumnHeight(BalancedColumnHeightCalculation calculationMode)
{
    bool changed = false;
    for (auto& group : m_fragmentainerGroups)
        changed = group.recalculateColumnHeight(calculationMode) || changed;
    return changed;
}

void RenderMultiColumnSet::recordSpaceShortage(LayoutUnit offsetInFlowThread, LayoutUnit spaceShortage)
{
    fragmentainerGroupAtFlowThreadOffset(offsetInFlowThread).recordSpaceShortage(spaceShortage);
}

void RenderMultiColumnSet::resetColumnHeight()
{
    m_fragmentainerGroups.deleteExtraGroups();
    m_fragmentainerGroups.first().resetColumnHeight();
}

void RenderMultiColumnSet::beginFlow(LayoutUnit offsetInFlowThread)
{
    // At this point layout is exactly at the beginning of this set. Store block offset from flow
    // thread start.
    m_fragmentainerGroups.first().setLogicalTopInFlowThread(offsetInFlowThread);
}

void RenderMultiColumnSet::endFlow(LayoutUnit offsetInFlowThread)
{
    // At this point layout is exactly at the end of this set. Store block offset from flow thread
    // start. This set is now considered "flowed", although we may have to revisit it later (with
    // beginFlow()), e.g. if a subtree in the flow thread has to be laid out over again because the
    // initial margin collapsing estimates were wrong.
    m_fragmentainerGroups.last().setLogicalBottomInFlowThread(offsetInFlowThread);
}

void RenderMultiColumnSet::expandToEncompassFlowThreadContentsIfNeeded()
{
    ASSERT(multiColumnFlowThread()->lastMultiColumnSet() == this);
    // FIXME: this may result in the need for creating additional rows, since there may not be
    // enough space remaining in the currently last row.
    m_fragmentainerGroups.last().expandToEncompassFlowThreadOverflow();
}

void RenderMultiColumnSet::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    LayoutUnit logicalHeight;
    for (const auto& group : m_fragmentainerGroups)
        logicalHeight += group.logicalHeight();
    computedValues.m_extent = logicalHeight;
    computedValues.m_position = logicalTop;
}

LayoutUnit RenderMultiColumnSet::columnGap() const
{
    RenderBlockFlow* parentBlock = multiColumnBlockFlow();
    if (parentBlock->style()->hasNormalColumnGap())
        return parentBlock->style()->fontDescription().computedPixelSize(); // "1em" is recommended as the normal gap setting. Matches <p> margins.
    return parentBlock->style()->columnGap();
}

unsigned RenderMultiColumnSet::actualColumnCount() const
{
    // FIXME: remove this method. It's a meaningless question to ask the set "how many columns do
    // you actually have?", since that may vary for each row.
    return firstFragmentainerGroup().actualColumnCount();
}

void RenderMultiColumnSet::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    MultiColumnSetPainter(*this).paintObject(paintInfo, paintOffset);
}

void RenderMultiColumnSet::collectLayerFragments(LayerFragments& fragments, const LayoutRect& layerBoundingBox, const LayoutRect& dirtyRect)
{
    for (const auto& group : m_fragmentainerGroups)
        group.collectLayerFragments(fragments, layerBoundingBox, dirtyRect);
}

void RenderMultiColumnSet::addOverflowFromChildren()
{
    LayoutRect overflowRect;
    for (const auto& group : m_fragmentainerGroups) {
        LayoutRect rect = group.calculateOverflow();
        rect.move(group.offsetFromColumnSet());
        overflowRect.unite(rect);
    }
    addLayoutOverflow(overflowRect);
    if (!hasOverflowClip())
        addVisualOverflow(overflowRect);
}

const char* RenderMultiColumnSet::renderName() const
{
    return "RenderMultiColumnSet";
}

void RenderMultiColumnSet::insertedIntoTree()
{
    RenderRegion::insertedIntoTree();

    attachRegion();
}

void RenderMultiColumnSet::willBeRemovedFromTree()
{
    RenderRegion::willBeRemovedFromTree();

    detachRegion();
}

void RenderMultiColumnSet::attachRegion()
{
    if (documentBeingDestroyed())
        return;

    // A region starts off invalid.
    setIsValid(false);

    if (!m_flowThread)
        return;

    // Only after adding the region to the thread, the region is marked to be valid.
    m_flowThread->addRegionToThread(this);
}

void RenderMultiColumnSet::detachRegion()
{
    if (m_flowThread) {
        m_flowThread->removeRegionFromThread(this);
        m_flowThread = 0;
    }
}

LayoutRect RenderMultiColumnSet::flowThreadPortionRect() const
{
    LayoutRect portionRect(LayoutUnit(), logicalTopInFlowThread(), pageLogicalWidth(), logicalHeightInFlowThread());
    if (!isHorizontalWritingMode())
        return portionRect.transposedRect();
    return portionRect;
}

}
