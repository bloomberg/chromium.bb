/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Scrollbar_h
#define Scrollbar_h

#include "core/platform/ScrollTypes.h"
#include "core/platform/ScrollbarThemeClient.h"
#include "core/platform/Timer.h"
#include "core/platform/Widget.h"
#include "wtf/MathExtras.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class GraphicsContext;
class IntRect;
class PlatformGestureEvent;
class PlatformMouseEvent;
class ScrollableArea;
class ScrollbarTheme;

class Scrollbar : public Widget,
                  public ScrollbarThemeClient {

public:
    static PassRefPtr<Scrollbar> create(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize);

    virtual ~Scrollbar();

    // ScrollbarThemeClient implementation.
    virtual int x() const { return Widget::x(); }
    virtual int y() const { return Widget::y(); }
    virtual int width() const { return Widget::width(); }
    virtual int height() const { return Widget::height(); }
    virtual IntSize size() const { return Widget::size(); }
    virtual IntPoint location() const { return Widget::location(); }

    virtual ScrollView* parent() const { return Widget::parent(); }
    virtual ScrollView* root() const { return Widget::root(); }

    virtual void setFrameRect(const IntRect&);
    virtual IntRect frameRect() const { return Widget::frameRect(); }

    virtual void invalidate() { Widget::invalidate(); }
    virtual void invalidateRect(const IntRect&);

    virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const;
    virtual void getTickmarks(Vector<IntRect>&) const;
    virtual bool isScrollableAreaActive() const;
    virtual bool isScrollViewScrollbar() const;

    virtual IntPoint convertFromContainingWindow(const IntPoint& windowPoint) { return Widget::convertFromContainingWindow(windowPoint); }

    virtual bool isCustomScrollbar() const { return false; }
    virtual ScrollbarOrientation orientation() const { return m_orientation; }
    virtual bool isLeftSideVerticalScrollbar() const;

    virtual int value() const { return lroundf(m_currentPos); }
    virtual float currentPos() const { return m_currentPos; }
    virtual int visibleSize() const { return m_visibleSize; }
    virtual int totalSize() const { return m_totalSize; }
    virtual int maximum() const { return m_totalSize - m_visibleSize; }
    virtual ScrollbarControlSize controlSize() const { return m_controlSize; }

    virtual ScrollbarPart pressedPart() const { return m_pressedPart; }
    virtual ScrollbarPart hoveredPart() const { return m_hoveredPart; }

    virtual void styleChanged() { }

    virtual bool enabled() const { return m_enabled; }
    virtual void setEnabled(bool);

    // Called by the ScrollableArea when the scroll offset changes.
    void offsetDidChange();

    void disconnectFromScrollableArea() { m_scrollableArea = 0; }
    ScrollableArea* scrollableArea() const { return m_scrollableArea; }

    int pressedPos() const { return m_pressedPos; }

    virtual void setHoveredPart(ScrollbarPart);
    virtual void setPressedPart(ScrollbarPart);

    void setProportion(int visibleSize, int totalSize);
    void setPressedPos(int p) { m_pressedPos = p; }

    virtual void paint(GraphicsContext*, const IntRect& damageRect);

    virtual bool isOverlayScrollbar() const;
    bool shouldParticipateInHitTesting();

    bool isWindowActive() const;

    bool gestureEvent(const PlatformGestureEvent&);

    // These methods are used for platform scrollbars to give :hover feedback.  They will not get called
    // when the mouse went down in a scrollbar, since it is assumed the scrollbar will start
    // grabbing all events in that case anyway.
    void mouseMoved(const PlatformMouseEvent&);
    void mouseEntered();
    void mouseExited();

    // Used by some platform scrollbars to know when they've been released from capture.
    void mouseUp(const PlatformMouseEvent&);
    void mouseDown(const PlatformMouseEvent&);

    ScrollbarTheme* theme() const { return m_theme; }

    virtual void setParent(ScrollView*);

    bool suppressInvalidation() const { return m_suppressInvalidation; }
    void setSuppressInvalidation(bool s) { m_suppressInvalidation = s; }

    virtual IntRect convertToContainingView(const IntRect&) const;
    virtual IntRect convertFromContainingView(const IntRect&) const;

    virtual IntPoint convertToContainingView(const IntPoint&) const;
    virtual IntPoint convertFromContainingView(const IntPoint&) const;

    void moveThumb(int pos, bool draggingDocument = false);

    virtual bool isAlphaLocked() const { return m_isAlphaLocked; }
    virtual void setIsAlphaLocked(bool flag) { m_isAlphaLocked = flag; }

protected:
    Scrollbar(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize, ScrollbarTheme* = 0);

    void updateThumb();
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

    void autoscrollTimerFired(Timer<Scrollbar>*);
    void startTimerIfNeeded(double delay);
    void stopTimerIfNeeded();
    void autoscrollPressedPart(double delay);
    ScrollDirection pressedPartScrollDirection();
    ScrollGranularity pressedPartScrollGranularity();

    ScrollableArea* m_scrollableArea;
    ScrollbarOrientation m_orientation;
    ScrollbarControlSize m_controlSize;
    ScrollbarTheme* m_theme;

    int m_visibleSize;
    int m_totalSize;
    float m_currentPos;
    float m_dragOrigin;

    ScrollbarPart m_hoveredPart;
    ScrollbarPart m_pressedPart;
    int m_pressedPos;
    float m_scrollPos;
    bool m_draggingDocument;
    int m_documentDragPos;

    bool m_enabled;

    Timer<Scrollbar> m_scrollTimer;
    bool m_overlapsResizer;

    bool m_suppressInvalidation;

    bool m_isAlphaLocked;

private:
    virtual bool isScrollbar() const { return true; }
    virtual AXObjectCache* existingAXObjectCache() const;

    float scrollableAreaCurrentPos() const;
};

inline Scrollbar* toScrollbar(Widget* widget)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!widget || widget->isScrollbar());
    return static_cast<Scrollbar*>(widget);
}

inline const Scrollbar* toScrollbar(const Widget* widget)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!widget || widget->isScrollbar());
    return static_cast<const Scrollbar*>(widget);
}

// This will catch anyone doing an unnecessary cast.
void toScrollbar(const Scrollbar*);

} // namespace WebCore

#endif // Scrollbar_h
