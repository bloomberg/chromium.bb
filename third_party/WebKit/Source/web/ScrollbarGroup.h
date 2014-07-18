/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollbarGroup_h
#define ScrollbarGroup_h

#include "platform/scroll/ScrollableArea.h"

#include "wtf/RefPtr.h"

namespace blink {
class FrameView;
}

namespace blink {

class WebPluginScrollbarImpl;

class ScrollbarGroup FINAL : public blink::ScrollableArea {
public:
    ScrollbarGroup(blink::FrameView*, const blink::IntRect& frameRect);
    virtual ~ScrollbarGroup();

    void scrollbarCreated(WebPluginScrollbarImpl*);
    void scrollbarDestroyed(WebPluginScrollbarImpl*);
    void setLastMousePosition(const blink::IntPoint&);
    void setFrameRect(const blink::IntRect&);

    // blink::ScrollableArea methods
    virtual int scrollSize(blink::ScrollbarOrientation) const OVERRIDE;
    virtual void setScrollOffset(const blink::IntPoint&) OVERRIDE;
    virtual void invalidateScrollbarRect(blink::Scrollbar*, const blink::IntRect&) OVERRIDE;
    virtual void invalidateScrollCornerRect(const blink::IntRect&) OVERRIDE;
    virtual bool isActive() const OVERRIDE;
    virtual blink::IntRect scrollCornerRect() const OVERRIDE { return blink::IntRect(); }
    virtual bool isScrollCornerVisible() const OVERRIDE;
    virtual void getTickmarks(Vector<blink::IntRect>&) const OVERRIDE;
    virtual blink::IntPoint convertFromContainingViewToScrollbar(const blink::Scrollbar*, const blink::IntPoint& parentPoint) const OVERRIDE;
    virtual blink::Scrollbar* horizontalScrollbar() const OVERRIDE;
    virtual blink::Scrollbar* verticalScrollbar() const OVERRIDE;
    virtual blink::IntPoint scrollPosition() const OVERRIDE;
    virtual blink::IntPoint minimumScrollPosition() const OVERRIDE;
    virtual blink::IntPoint maximumScrollPosition() const OVERRIDE;
    virtual int visibleHeight() const OVERRIDE;
    virtual int visibleWidth() const OVERRIDE;
    virtual blink::IntSize contentsSize() const OVERRIDE;
    virtual blink::IntSize overhangAmount() const OVERRIDE;
    virtual blink::IntPoint lastKnownMousePosition() const OVERRIDE;
    virtual bool shouldSuspendScrollAnimations() const OVERRIDE;
    virtual void scrollbarStyleChanged() OVERRIDE;
    virtual bool scrollbarsCanBeActive() const OVERRIDE;
    virtual blink::IntRect scrollableAreaBoundingBox() const OVERRIDE;
    virtual bool userInputScrollable(blink::ScrollbarOrientation) const OVERRIDE;
    virtual bool shouldPlaceVerticalScrollbarOnLeft() const OVERRIDE;
    virtual int pageStep(blink::ScrollbarOrientation) const OVERRIDE;

private:
    blink::FrameView* m_frameView;
    blink::IntPoint m_lastMousePosition;
    blink::IntRect m_frameRect;
    WebPluginScrollbarImpl* m_horizontalScrollbar;
    WebPluginScrollbarImpl* m_verticalScrollbar;
};

} // namespace blink

#endif
