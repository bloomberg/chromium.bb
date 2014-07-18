// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderPagedFlowThread.h"

namespace blink {

RenderPagedFlowThread* RenderPagedFlowThread::createAnonymous(Document& document, RenderStyle* parentStyle)
{
    RenderPagedFlowThread* renderer = new RenderPagedFlowThread();
    renderer->setDocumentForAnonymous(&document);
    renderer->setStyle(RenderStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK));
    return renderer;
}

const char* RenderPagedFlowThread::renderName() const
{
    return "RenderPagedFlowThread";
}

bool RenderPagedFlowThread::needsNewWidth() const
{
    return progressionIsInline() != pagedBlockFlow()->style()->hasInlinePaginationAxis();
}

void RenderPagedFlowThread::updateLogicalWidth()
{
    // As long as we inherit from RenderMultiColumnFlowThread, we need to bypass its implementation
    // here. We're not split into columns, so the flow thread width will just be whatever is
    // available in the containing block.
    RenderFlowThread::updateLogicalWidth();
}

void RenderPagedFlowThread::layout()
{
    setProgressionIsInline(pagedBlockFlow()->style()->hasInlinePaginationAxis());
    RenderMultiColumnFlowThread::layout();
}

} // namespace blink
