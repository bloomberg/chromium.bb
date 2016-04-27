// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ViewportScrollCallback.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/TopControls.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ScrollState.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

ViewportScrollCallback::ViewportScrollCallback(Document& document)
    : m_document(&document)
{
    // Only the root document can have a viewport scroll callback for now.
    ASSERT(!document.ownerElement());
}

ViewportScrollCallback::~ViewportScrollCallback()
{
}

DEFINE_TRACE(ViewportScrollCallback)
{
    visitor->trace(m_document);
    ScrollStateCallback::trace(visitor);
}

bool ViewportScrollCallback::shouldScrollTopControls(const FloatSize& delta,
    ScrollGranularity granularity) const
{
    if (granularity != ScrollByPixel && granularity != ScrollByPrecisePixel)
        return false;

    ScrollableArea* rootFrameViewport = getRootFrameViewport();
    if (!rootFrameViewport)
        return false;

    DoublePoint maxScroll = rootFrameViewport->maximumScrollPositionDouble();
    DoublePoint scrollPosition = rootFrameViewport->scrollPositionDouble();

    // Always give the delta to the top controls if the scroll is in
    // the direction to show the top controls. If it's in the
    // direction to hide the top controls, only give the delta to the
    // top controls when the frame can scroll.
    return delta.height() < 0 || scrollPosition.y() < maxScroll.y();
}

void ViewportScrollCallback::handleEvent(ScrollState* state)
{
    if (!m_document || !m_document->frameHost())
        return;

    TopControls& topControls = m_document->frameHost()->topControls();
    OverscrollController& overscrollController =
        m_document->frameHost()->overscrollController();

    // Scroll top controls.
    if (state->isBeginning())
        topControls.scrollBegin();

    FloatSize delta(state->deltaX(), state->deltaY());
    ScrollGranularity granularity =
        ScrollGranularity(static_cast<int>(state->deltaGranularity()));
    FloatSize remainingDelta = delta;

    if (shouldScrollTopControls(delta, granularity))
        remainingDelta = topControls.scrollBy(delta);

    bool topControlsConsumedScroll = remainingDelta.height() != delta.height();

    // Do the native scroll.
    ScrollableArea* rootFrameViewport = getRootFrameViewport();
    if (!rootFrameViewport)
        return;

    ScrollResult result =
        rootFrameViewport->userScroll(granularity, remainingDelta);

    // We consider top controls movement to be scrolling.
    result.didScrollY |= topControlsConsumedScroll;

    // Handle Overscroll.
    FloatPoint position(state->positionX(), state->positionY());
    FloatSize velocity(state->velocityX(), state->velocityY());
    overscrollController.handleOverscroll(result, position, velocity);

    // The viewport consumes everything.
    state->consumeDeltaNative(
        state->deltaX() - result.unusedScrollDeltaX,
        state->deltaY() - result.unusedScrollDeltaY);
}

ScrollableArea* ViewportScrollCallback::getRootFrameViewport() const
{
    if (m_document->layoutViewItem().isNull())
        return nullptr;

    FrameView* frameView = m_document->layoutViewItem().frameView();
    if (!frameView)
        return nullptr;

    ScrollableArea* rootFrameViewport = frameView->getScrollableArea();
    ASSERT(rootFrameViewport);

    return rootFrameViewport;
}

} // namespace blink
