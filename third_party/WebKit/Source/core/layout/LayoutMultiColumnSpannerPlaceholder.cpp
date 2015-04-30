// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"

namespace blink {

static void copyMarginProperties(ComputedStyle& placeholderStyle, const ComputedStyle& spannerStyle)
{
    // We really only need the block direction margins, but there are no setters for that in
    // ComputedStyle. Just copy all margin sides. The inline ones don't matter anyway.
    placeholderStyle.setMarginLeft(spannerStyle.marginLeft());
    placeholderStyle.setMarginRight(spannerStyle.marginRight());
    placeholderStyle.setMarginTop(spannerStyle.marginTop());
    placeholderStyle.setMarginBottom(spannerStyle.marginBottom());
}

LayoutMultiColumnSpannerPlaceholder* LayoutMultiColumnSpannerPlaceholder::createAnonymous(const ComputedStyle& parentStyle, LayoutBox& layoutObjectInFlowThread)
{
    LayoutMultiColumnSpannerPlaceholder* newSpanner = new LayoutMultiColumnSpannerPlaceholder(&layoutObjectInFlowThread);
    Document& document = layoutObjectInFlowThread.document();
    newSpanner->setDocumentForAnonymous(&document);
    RefPtr<ComputedStyle> newStyle = ComputedStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK);
    copyMarginProperties(*newStyle, layoutObjectInFlowThread.styleRef());
    newSpanner->setStyle(newStyle);
    return newSpanner;
}

LayoutMultiColumnSpannerPlaceholder::LayoutMultiColumnSpannerPlaceholder(LayoutBox* layoutObjectInFlowThread)
    : LayoutBox(0)
    , m_layoutObjectInFlowThread(layoutObjectInFlowThread)
{
}

void LayoutMultiColumnSpannerPlaceholder::updateMarginProperties()
{
    RefPtr<ComputedStyle> newStyle = ComputedStyle::clone(styleRef());
    copyMarginProperties(*newStyle, m_layoutObjectInFlowThread->styleRef());
    setStyle(newStyle);
}

void LayoutMultiColumnSpannerPlaceholder::willBeRemovedFromTree()
{
    if (m_layoutObjectInFlowThread)
        m_layoutObjectInFlowThread->clearSpannerPlaceholder();
    LayoutBox::willBeRemovedFromTree();
}

bool LayoutMultiColumnSpannerPlaceholder::needsPreferredWidthsRecalculation() const
{
    return m_layoutObjectInFlowThread->needsPreferredWidthsRecalculation();
}

LayoutUnit LayoutMultiColumnSpannerPlaceholder::minPreferredLogicalWidth() const
{
    return m_layoutObjectInFlowThread->minPreferredLogicalWidth();
}

LayoutUnit LayoutMultiColumnSpannerPlaceholder::maxPreferredLogicalWidth() const
{
    return m_layoutObjectInFlowThread->maxPreferredLogicalWidth();
}

void LayoutMultiColumnSpannerPlaceholder::layout()
{
    ASSERT(needsLayout());

    // Lay out the actual column-span:all element.
    m_layoutObjectInFlowThread->layoutIfNeeded();

    // The spanner has now been laid out, so its height is known. Time to update the placeholder's
    // height as well, so that we take up the correct amount of space in the multicol container.
    updateLogicalHeight();

    // Take the overflow from the spanner, so that it gets
    // propagated to the multicol container and beyond.
    m_overflow.clear();
    addVisualOverflow(m_layoutObjectInFlowThread->visualOverflowRect());
    addLayoutOverflow(m_layoutObjectInFlowThread->layoutOverflowRect());

    clearNeedsLayout();
}

void LayoutMultiColumnSpannerPlaceholder::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = m_layoutObjectInFlowThread->logicalHeight();
    computedValues.m_position = logicalTop;
    computedValues.m_margins.m_before = marginBefore();
    computedValues.m_margins.m_after = marginAfter();
}

void LayoutMultiColumnSpannerPlaceholder::invalidateTreeIfNeeded(PaintInvalidationState& paintInvalidationState)
{
    PaintInvalidationState newPaintInvalidationState(paintInvalidationState, *this, paintInvalidationState.paintInvalidationContainer());
    m_layoutObjectInFlowThread->invalidateTreeIfNeeded(newPaintInvalidationState);
    LayoutBox::invalidateTreeIfNeeded(paintInvalidationState);
}

void LayoutMultiColumnSpannerPlaceholder::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!m_layoutObjectInFlowThread->hasSelfPaintingLayer())
        m_layoutObjectInFlowThread->paint(paintInfo, paintOffset);
}

bool LayoutMultiColumnSpannerPlaceholder::nodeAtPoint(HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    return !m_layoutObjectInFlowThread->hasSelfPaintingLayer() && m_layoutObjectInFlowThread->nodeAtPoint(result, locationInContainer, accumulatedOffset, action);
}

}
