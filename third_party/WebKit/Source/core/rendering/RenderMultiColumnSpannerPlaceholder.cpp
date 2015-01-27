// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderMultiColumnSpannerPlaceholder.h"

namespace blink {

static void copyMarginProperties(RenderStyle* placeholderStyle, const RenderStyle* spannerStyle)
{
    // We really only need the block direction margins, but there are no setters for that in
    // RenderStyle. Just copy all margin sides. The inline ones don't matter anyway.
    placeholderStyle->setMarginLeft(spannerStyle->marginLeft());
    placeholderStyle->setMarginRight(spannerStyle->marginRight());
    placeholderStyle->setMarginTop(spannerStyle->marginTop());
    placeholderStyle->setMarginBottom(spannerStyle->marginBottom());
}

RenderMultiColumnSpannerPlaceholder* RenderMultiColumnSpannerPlaceholder::createAnonymous(RenderStyle* parentStyle, RenderBox* rendererInFlowThread)
{
    RenderMultiColumnSpannerPlaceholder* newSpanner = new RenderMultiColumnSpannerPlaceholder(rendererInFlowThread);
    Document& document = rendererInFlowThread->document();
    newSpanner->setDocumentForAnonymous(&document);
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK);
    copyMarginProperties(newStyle.get(), rendererInFlowThread->style());
    newSpanner->setStyle(newStyle);
    return newSpanner;
}

RenderMultiColumnSpannerPlaceholder::RenderMultiColumnSpannerPlaceholder(RenderBox* rendererInFlowThread)
    : RenderBox(0)
    , m_rendererInFlowThread(rendererInFlowThread)
{
}

void RenderMultiColumnSpannerPlaceholder::updateMarginProperties()
{
    RefPtr<RenderStyle> newStyle = RenderStyle::clone(style());
    copyMarginProperties(newStyle.get(), m_rendererInFlowThread->style());
    setStyle(newStyle);
}

void RenderMultiColumnSpannerPlaceholder::willBeRemovedFromTree()
{
    if (m_rendererInFlowThread)
        m_rendererInFlowThread->clearSpannerPlaceholder();
    RenderBox::willBeRemovedFromTree();
}

bool RenderMultiColumnSpannerPlaceholder::needsPreferredWidthsRecalculation() const
{
    return m_rendererInFlowThread->needsPreferredWidthsRecalculation();
}

LayoutUnit RenderMultiColumnSpannerPlaceholder::minPreferredLogicalWidth() const
{
    return m_rendererInFlowThread->minPreferredLogicalWidth();
}

LayoutUnit RenderMultiColumnSpannerPlaceholder::maxPreferredLogicalWidth() const
{
    return m_rendererInFlowThread->maxPreferredLogicalWidth();
}

void RenderMultiColumnSpannerPlaceholder::layout()
{
    ASSERT(needsLayout());

    // Lay out the actual column-span:all element.
    m_rendererInFlowThread->layoutIfNeeded();

    // The spanner has now been laid out, so its height is known. Time to update the placeholder's
    // height as well, so that we take up the correct amount of space in the multicol container.
    updateLogicalHeight();

    // Take the overflow from the spanner, so that it gets
    // propagated to the multicol container and beyond.
    m_overflow.clear();
    addVisualOverflow(m_rendererInFlowThread->visualOverflowRect());
    addLayoutOverflow(m_rendererInFlowThread->layoutOverflowRect());

    clearNeedsLayout();
}

void RenderMultiColumnSpannerPlaceholder::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    computedValues.m_extent = m_rendererInFlowThread->logicalHeight();
    computedValues.m_position = logicalTop;
    computedValues.m_margins.m_before = marginBefore();
    computedValues.m_margins.m_after = marginAfter();
}

void RenderMultiColumnSpannerPlaceholder::invalidateTreeIfNeeded(const PaintInvalidationState& paintInvalidationState)
{
    PaintInvalidationState newPaintInvalidationState(paintInvalidationState, *this, paintInvalidationState.paintInvalidationContainer());
    m_rendererInFlowThread->invalidateTreeIfNeeded(newPaintInvalidationState);
    RenderBox::invalidateTreeIfNeeded(paintInvalidationState);
}

void RenderMultiColumnSpannerPlaceholder::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!m_rendererInFlowThread->hasSelfPaintingLayer())
        m_rendererInFlowThread->paint(paintInfo, paintOffset);
}

bool RenderMultiColumnSpannerPlaceholder::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    return !m_rendererInFlowThread->hasSelfPaintingLayer() && m_rendererInFlowThread->nodeAtPoint(request, result, locationInContainer, accumulatedOffset, action);
}

const char* RenderMultiColumnSpannerPlaceholder::renderName() const
{
    return "RenderMultiColumnSpannerPlaceholder";
}

}
