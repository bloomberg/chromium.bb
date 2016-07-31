// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/ResizeViewportAnchor.h"

#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/page/Page.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatSize.h"

namespace blink {

void ResizeViewportAnchor::resizeFrameView(IntSize size)
{
    FrameView* frameView = rootFrameView();
    DCHECK(frameView);

    ScrollableArea* rootViewport = frameView->getScrollableArea();
    DoublePoint position = rootViewport->scrollPositionDouble();

    frameView->resize(size);
    m_drift += rootViewport->scrollPositionDouble() - position;
}

void ResizeViewportAnchor::endScope()
{
    if (--m_scopeCount > 0)
        return;

    FrameView* frameView = rootFrameView();
    if (!frameView)
        return;

    VisualViewport& visualViewport = m_page->frameHost().visualViewport();
    DoublePoint visualViewportInDocument =
        frameView->getScrollableArea()->scrollPositionDouble() - m_drift;

    // TODO(bokan): Don't use RootFrameViewport::setScrollPosition since it
    // assumes we can just set a sub-pixel precision offset on the FrameView.
    // While we "can" do this, the offset that will be shipped to CC will be the
    // truncated number and this class is used to handle TopControl movement
    // which needs the two threads to match exactly pixel-for-pixel. We can
    // replace this with RFV::setScrollPosition once Blink is sub-pixel scroll
    // offset aware. crbug.com/414283.

    ScrollableArea* rootViewport = frameView->getScrollableArea();
    ScrollableArea* layoutViewport =
        frameView->layoutViewportScrollableArea();

    // Clamp the scroll offset of each viewport now so that we force any invalid
    // offsets to become valid so we can compute the correct deltas.
    visualViewport.clampToBoundaries();
    layoutViewport->setScrollPosition(
        layoutViewport->scrollPositionDouble(), ProgrammaticScroll);

    DoubleSize delta = visualViewportInDocument
        - rootViewport->scrollPositionDouble();

    visualViewport.move(toFloatSize(delta));

    delta = visualViewportInDocument
        - rootViewport->scrollPositionDouble();

    // Since the main thread FrameView has integer scroll offsets, scroll it to
    // the next pixel and then we'll scroll the visual viewport again to
    // compensate for the sub-pixel offset. We need this "overscroll" to ensure
    // the pixel of which we want to be partially in appears fully inside the
    // FrameView since the VisualViewport is bounded by the FrameView.
    IntSize layoutDelta = IntSize(
        delta.width() < 0 ? floor(delta.width()) : ceil(delta.width()),
        delta.height() < 0 ? floor(delta.height()) : ceil(delta.height()));

    layoutViewport->setScrollPosition(
        layoutViewport->scrollPosition() + layoutDelta,
        ProgrammaticScroll);

    delta = visualViewportInDocument
        - rootViewport->scrollPositionDouble();
    visualViewport.move(toFloatSize(delta));
    m_drift = DoubleSize();
}

FrameView* ResizeViewportAnchor::rootFrameView()
{
    if (Frame* frame = m_page->mainFrame()) {
        if (frame->isLocalFrame())
            return toLocalFrame(frame)->view();
    }
    return nullptr;
}

} // namespace blink
