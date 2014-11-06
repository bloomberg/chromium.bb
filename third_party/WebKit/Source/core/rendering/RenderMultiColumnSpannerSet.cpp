// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderMultiColumnSpannerSet.h"

namespace blink {

RenderMultiColumnSpannerSet* RenderMultiColumnSpannerSet::createAnonymous(RenderMultiColumnFlowThread* flowThread, RenderStyle* parentStyle, RenderBox* rendererInFlowThread)
{
    RenderMultiColumnSpannerSet* newSpanner = new RenderMultiColumnSpannerSet(flowThread, rendererInFlowThread);
    Document& document = flowThread->document();
    newSpanner->setDocumentForAnonymous(&document);
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK);
    newSpanner->setStyle(newStyle);
    return newSpanner;
}

RenderMultiColumnSpannerSet::RenderMultiColumnSpannerSet(RenderMultiColumnFlowThread* flowThread, RenderBox* rendererInFlowThread)
    : RenderMultiColumnSet(flowThread)
    , m_rendererInFlowThread(rendererInFlowThread)
{
}

bool RenderMultiColumnSpannerSet::recalculateColumnHeight(BalancedHeightCalculation calculationMode)
{
    return false;
}

const char* RenderMultiColumnSpannerSet::renderName() const
{
    return "RenderMultiColumnSpannerSet";
}

}
