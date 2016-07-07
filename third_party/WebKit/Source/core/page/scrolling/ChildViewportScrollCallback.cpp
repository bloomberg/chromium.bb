// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ChildViewportScrollCallback.h"

#include "core/page/scrolling/ScrollState.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

ChildViewportScrollCallback::ChildViewportScrollCallback()
{
}

ChildViewportScrollCallback::~ChildViewportScrollCallback()
{
}

DEFINE_TRACE(ChildViewportScrollCallback)
{
    visitor->trace(m_scroller);
    ViewportScrollCallback::trace(visitor);
}


void ChildViewportScrollCallback::handleEvent(ScrollState* state)
{
    DCHECK(state);

    if (!m_scroller)
        return;

    performNativeScroll(*state, *m_scroller);
}

void ChildViewportScrollCallback::setScroller(ScrollableArea* scroller)
{
    m_scroller = scroller;
}

} // namespace blink
