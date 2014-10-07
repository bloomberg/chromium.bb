/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPluginScrollbarImpl_h
#define WebPluginScrollbarImpl_h

#include "public/web/WebPluginScrollbar.h"

#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class IntPoint;
class IntRect;
class Scrollbar;
class ScrollbarGroup;

class WebPluginScrollbarImpl final : public WebPluginScrollbar {
public:
    WebPluginScrollbarImpl(Orientation, ScrollbarGroup*, WebPluginScrollbarClient*);
    virtual ~WebPluginScrollbarImpl();

    void setScrollOffset(int);
    void invalidateScrollbarRect(const IntRect&);
    // FIXME: Combine this with the other getTickmarks method
    void getTickmarks(Vector<IntRect>&) const;
    IntPoint convertFromContainingViewToScrollbar(const IntPoint& parentPoint) const;
    void scrollbarStyleChanged();

    int scrollOffset() { return m_scrollOffset; }
    Scrollbar* scrollbar() { return m_scrollbar.get(); }

    // WebScrollbar methods
    virtual bool isOverlay() const override;
    virtual int value() const override;
    virtual WebPoint location() const override;
    virtual WebSize size() const override;
    virtual bool enabled() const override;
    virtual int maximum() const override;
    virtual int totalSize() const override;
    virtual bool isScrollViewScrollbar() const override;
    virtual bool isScrollableAreaActive() const override;
    virtual void getTickmarks(WebVector<WebRect>& tickmarks) const override;
    virtual WebScrollbar::ScrollbarControlSize controlSize() const override;
    virtual WebScrollbar::ScrollbarPart pressedPart() const override;
    virtual WebScrollbar::ScrollbarPart hoveredPart() const override;
    virtual WebScrollbar::ScrollbarOverlayStyle scrollbarOverlayStyle() const override;
    virtual WebScrollbar::Orientation orientation() const override;
    virtual bool isLeftSideVerticalScrollbar() const override;
    virtual bool isCustomScrollbar() const override;

    // WebPluginScrollbar methods
    virtual void setLocation(const WebRect&) override;
    virtual void setValue(int position) override;
    virtual void setDocumentSize(int) override;
    virtual void scroll(ScrollDirection, ScrollGranularity, float multiplier) override;
    virtual void paint(WebCanvas*, const WebRect&) override;
    virtual bool handleInputEvent(const WebInputEvent&) override;
    virtual bool isAlphaLocked() const override;
    virtual void setIsAlphaLocked(bool) override;

private:
    bool onMouseDown(const WebInputEvent&);
    bool onMouseUp(const WebInputEvent&);
    bool onMouseMove(const WebInputEvent&);
    bool onMouseLeave(const WebInputEvent&);
    bool onMouseWheel(const WebInputEvent&);
    bool onKeyDown(const WebInputEvent&);

    ScrollbarGroup* m_group;
    WebPluginScrollbarClient* m_client;

    int m_scrollOffset;
    RefPtr<Scrollbar> m_scrollbar;
};

} // namespace blink

#endif
