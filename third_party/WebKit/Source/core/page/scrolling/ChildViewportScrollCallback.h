// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ChildViewportScrollCallback_h
#define ChildViewportScrollCallback_h

#include "core/page/scrolling/ViewportScrollCallback.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScrollableArea;
class ScrollState;

// The ViewportScrollCallback used by non-root Frames, i.e. iframes. This is
// essentially a no-op implementation that simply scrolls the frame since
// iframes don't have any additional scrolling actions, unlike the root frame
// which moves the top controls and provides overscroll glow.
class ChildViewportScrollCallback : public ViewportScrollCallback {
public:
    static ChildViewportScrollCallback* create()
    {
        return new ChildViewportScrollCallback();
    }

    virtual ~ChildViewportScrollCallback();

    void handleEvent(ScrollState*) override;
    void setScroller(ScrollableArea*) override;

    DECLARE_VIRTUAL_TRACE();

private:
    ChildViewportScrollCallback();

    WeakMember<ScrollableArea> m_scroller;
};

} // namespace blink

#endif // ChildViewportScrollCallback_h
