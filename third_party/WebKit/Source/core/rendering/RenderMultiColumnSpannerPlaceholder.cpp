// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderMultiColumnSpannerPlaceholder.h"

namespace blink {

RenderMultiColumnSpannerPlaceholder* RenderMultiColumnSpannerPlaceholder::createAnonymous(RenderStyle* parentStyle, RenderBox* rendererInFlowThread)
{
    RenderMultiColumnSpannerPlaceholder* newSpanner = new RenderMultiColumnSpannerPlaceholder(rendererInFlowThread);
    Document& document = rendererInFlowThread->document();
    newSpanner->setDocumentForAnonymous(&document);
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK);
    newSpanner->setStyle(newStyle);
    return newSpanner;
}

RenderMultiColumnSpannerPlaceholder::RenderMultiColumnSpannerPlaceholder(RenderBox* rendererInFlowThread)
    : RenderBox(0)
    , m_rendererInFlowThread(rendererInFlowThread)
{
}

void RenderMultiColumnSpannerPlaceholder::spannerWillBeRemoved()
{
    ASSERT(m_rendererInFlowThread);
    RenderBox* renderer = m_rendererInFlowThread;
    m_rendererInFlowThread = 0;
    flowThread()->flowThreadDescendantWillBeRemoved(renderer);
    // |this| should be destroyed by now.
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

void RenderMultiColumnSpannerPlaceholder::layout()
{
    ASSERT(needsLayout());

    // FIXME: actual spanner positioning isn't implemented yet. Just set it to 0,0 for consistency
    // (in case the spanner used to be something else that was laid out properly).
    m_rendererInFlowThread->setLogicalTop(LayoutUnit());
    m_rendererInFlowThread->setLogicalLeft(LayoutUnit());

    // Lay out the actual column-span:all element.
    m_rendererInFlowThread->layoutIfNeeded();

    clearNeedsLayout();
}

void RenderMultiColumnSpannerPlaceholder::invalidateTreeIfNeeded(const PaintInvalidationState& paintInvalidationState)
{
    PaintInvalidationState newPaintInvalidationState(paintInvalidationState, *this, paintInvalidationState.paintInvalidationContainer());
    m_rendererInFlowThread->invalidateTreeIfNeeded(newPaintInvalidationState);
    RenderBox::invalidateTreeIfNeeded(paintInvalidationState);
}

const char* RenderMultiColumnSpannerPlaceholder::renderName() const
{
    return "RenderMultiColumnSpannerPlaceholder";
}

}
