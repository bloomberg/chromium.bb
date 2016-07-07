// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RootViewportScrollCallback_h
#define RootViewportScrollCallback_h

#include "core/page/scrolling/ViewportScrollCallback.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class FloatSize;
class ScrollableArea;
class ScrollState;
class TopControls;
class OverscrollController;
class RootFrameViewport;

// The ViewportScrollCallback used by the one root frame on the page. This
// callback provides scrolling of the frame as well as associated actions like
// top controls movement and overscroll glow.
class RootViewportScrollCallback : public ViewportScrollCallback {
public:
    // The TopControls and OverscrollController are given to the
    // RootViewportScrollCallback but are not owned or kept alive by it.
    static RootViewportScrollCallback* create(
        TopControls* topControls,
        OverscrollController* overscrollController,
        RootFrameViewport& rootFrameViewport)
    {
        return new RootViewportScrollCallback(
            topControls, overscrollController, rootFrameViewport);
    }

    virtual ~RootViewportScrollCallback();

    void handleEvent(ScrollState*) override;
    void setScroller(ScrollableArea*) override;

    DECLARE_VIRTUAL_TRACE();

private:
    // RootViewportScrollCallback does not assume ownership of TopControls or of
    // OverscrollController.
    RootViewportScrollCallback(TopControls*, OverscrollController*, RootFrameViewport&);

    bool shouldScrollTopControls(const FloatSize&, ScrollGranularity) const;
    bool scrollTopControls(ScrollState&);

    WeakMember<TopControls> m_topControls;
    WeakMember<OverscrollController> m_overscrollController;
    WeakMember<RootFrameViewport> m_rootFrameViewport;
};

} // namespace blink

#endif // RootViewportScrollCallback_h
