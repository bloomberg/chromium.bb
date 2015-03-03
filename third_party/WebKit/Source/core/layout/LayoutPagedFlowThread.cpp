// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/LayoutPagedFlowThread.h"

namespace blink {

LayoutPagedFlowThread* LayoutPagedFlowThread::createAnonymous(Document& document, const LayoutStyle& parentStyle)
{
    LayoutPagedFlowThread* renderer = new LayoutPagedFlowThread();
    renderer->setDocumentForAnonymous(&document);
    renderer->setStyle(LayoutStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK));
    return renderer;
}

bool LayoutPagedFlowThread::needsNewWidth() const
{
    return progressionIsInline() != pagedBlockFlow()->style()->hasInlinePaginationAxis();
}

void LayoutPagedFlowThread::updateLogicalWidth()
{
    // As long as we inherit from LayoutMultiColumnFlowThread, we need to bypass its implementation
    // here. We're not split into columns, so the flow thread width will just be whatever is
    // available in the containing block.
    LayoutFlowThread::updateLogicalWidth();
}

void LayoutPagedFlowThread::layout()
{
    setProgressionIsInline(pagedBlockFlow()->style()->hasInlinePaginationAxis());
    LayoutMultiColumnFlowThread::layout();
}

} // namespace blink
