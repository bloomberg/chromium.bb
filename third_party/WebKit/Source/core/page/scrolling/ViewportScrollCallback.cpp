// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ViewportScrollCallback.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/Settings.h"
#include "core/frame/TopControls.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ScrollState.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

ViewportScrollCallback::ViewportScrollCallback(
    TopControls* topControls,
    OverscrollController* overscrollController,
    RootFrameViewport& rootFrameViewport)
    : m_topControls(topControls)
    , m_overscrollController(overscrollController)
    , m_rootFrameViewport(&rootFrameViewport)
{
}

ViewportScrollCallback::~ViewportScrollCallback()
{
}

DEFINE_TRACE(ViewportScrollCallback)
{
    visitor->trace(m_topControls);
    visitor->trace(m_overscrollController);
    visitor->trace(m_rootFrameViewport);
    ScrollStateCallback::trace(visitor);
}

bool ViewportScrollCallback::shouldScrollTopControls(const FloatSize& delta,
    ScrollGranularity granularity) const
{
    if (granularity != ScrollByPixel && granularity != ScrollByPrecisePixel)
        return false;

    if (!m_rootFrameViewport)
        return false;

    DoublePoint maxScroll = m_rootFrameViewport->maximumScrollPositionDouble();
    DoublePoint scrollPosition = m_rootFrameViewport->scrollPositionDouble();

    // Always give the delta to the top controls if the scroll is in
    // the direction to show the top controls. If it's in the
    // direction to hide the top controls, only give the delta to the
    // top controls when the frame can scroll.
    return delta.height() < 0 || scrollPosition.y() < maxScroll.y();
}

bool ViewportScrollCallback::scrollTopControls(ScrollState& state)
{
    // Scroll top controls.
    if (m_topControls) {
        if (state.isBeginning())
            m_topControls->scrollBegin();

        FloatSize delta(state.deltaX(), state.deltaY());
        ScrollGranularity granularity =
            ScrollGranularity(static_cast<int>(state.deltaGranularity()));
        if (shouldScrollTopControls(delta, granularity)) {
            FloatSize remainingDelta = m_topControls->scrollBy(delta);
            FloatSize consumed = delta - remainingDelta;
            state.consumeDeltaNative(consumed.width(), consumed.height());
            return !consumed.isZero();
        }
    }

    return false;
}

void ViewportScrollCallback::handleEvent(ScrollState* state)
{
    DCHECK(state);
    if (!m_rootFrameViewport)
        return;

    bool topControlsDidScroll = scrollTopControls(*state);

    ScrollResult result = performNativeScroll(*state);

    // We consider top controls movement to be scrolling.
    result.didScrollY |= topControlsDidScroll;

    // Handle Overscroll.
    if (m_overscrollController) {
        FloatPoint position(state->positionX(), state->positionY());
        FloatSize velocity(state->velocityX(), state->velocityY());
        m_overscrollController->handleOverscroll(result, position, velocity);
    }
}

void ViewportScrollCallback::setScroller(ScrollableArea* scroller)
{
    DCHECK(scroller);
    m_rootFrameViewport->setLayoutViewport(*scroller);
}

ScrollResult ViewportScrollCallback::performNativeScroll(ScrollState& state)
{
    DCHECK(m_rootFrameViewport);

    FloatSize delta(state.deltaX(), state.deltaY());
    ScrollGranularity granularity =
        ScrollGranularity(static_cast<int>(state.deltaGranularity()));

    ScrollResult result = m_rootFrameViewport->userScroll(granularity, delta);

    // The viewport consumes everything.
    // TODO(bokan): This isn't actually consuming everything but doing so breaks
    // the main thread pull-to-refresh action. crbug.com/607210.
    state.consumeDeltaNative(
        delta.width() - result.unusedScrollDeltaX,
        delta.height() - result.unusedScrollDeltaY);

    return result;
}

} // namespace blink
