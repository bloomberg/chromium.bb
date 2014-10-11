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

#include "platform/Timer.h"
#include "platform/Widget.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "wtf/MathExtras.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class GraphicsContext;
class IntRect;
class PlatformGestureEvent;
class PlatformMouseEvent;
class ScrollableArea;
class ScrollbarTheme;

class PLATFORM_EXPORT Scrollbar : public Widget,
                  public ScrollbarThemeClient {

public:
    static PassRefPtr<Scrollbar> create(ScrollableArea*, ScrollbarOrientation, ScrollbarControlSize);

    virtual ~Scrollbar();

    // ScrollbarThemeClient implementation.
    virtual int x() const override { return Widget::x(); }
    virtual int y() const override { return Widget::y(); }
    virtual int width() const override { return Widget::width(); }
    virtual int height() const override { return Widget::height(); }
    virtual IntSize size() const override { return Widget::size(); }
    virtual IntPoint location() const override { return Widget::location(); }

    virtual Widget* parent() const override { return Widget::parent(); }
    virtual Widget* root() const override { return Widget::root(); }

    virtual void setFrameRect(const IntRect& frameRect) override { Widget::setFrameRect(frameRect); }
    virtual IntRect frameRect() const override { return Widget::frameRect(); }

    virtual void invalidate() override { Widget::invalidate(); }
    virtual void invalidateRect(const IntRect&) override;

    virtual ScrollbarOverlayStyle scrollbarOverlayStyle() const override;
    virtual void getTickmarks(Vector<IntRect>&) const override;
    virtual bool isScrollableAreaActive() const override;

    virtual IntPoint convertFromContainingWindow(const IntPoint& windowPoint) override { return Widget::convertFromContainingWindow(windowPoint); }

    virtual bool isCustomScrollbar() const override { return false; }
    virtual ScrollbarOrientation orientation() const override { return m_orientation; }
    virtual bool isLeftSideVerticalScrollbar() const override;

    virtual int value() const override { return lroundf(m_currentPos); }
    virtual float currentPos() const override { return m_currentPos; }
    virtual int visibleSize() const override { return m_visibleSize; }
    virtual int totalSize() const override { return m_totalSize; }
    virtual int maximum() const override { return m_totalSize - m_visibleSize; }
    virtual ScrollbarControlSize controlSize() const override { return m_controlSize; }

    virtual ScrollbarPart pressedPart() const override { return m_pressedPart; }
    virtual ScrollbarPart hoveredPart() const override { return m_hoveredPart; }

    virtual void styleChanged() override { }

    virtual bool enabled() const override { return m_enabled; }
    virtual void setEnabled(bool) override;

    // Called by the ScrollableArea when the scroll offset changes.
    void offsetDidChange();

    void disconnectFromScrollableArea() { m_scrollableArea = 0; }
    ScrollableArea* scrollableArea() const { return m_scrollableArea; }

    int pressedPos() const { return m_pressedPos; }

    virtual void setHoveredPart(ScrollbarPart);
    virtual void setPressedPart(ScrollbarPart);

    void setProportion(int visibleSize, int totalSize);
    void setPressedPos(int p) { m_pressedPos = p; }

    virtual void paint(GraphicsContext*, const IntRect& damageRect) override;

    virtual bool isOverlayScrollbar() const override;
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

    bool suppressInvalidation() const { return m_suppressInvalidation; }
    void setSuppressInvalidation(bool s) { m_suppressInvalidation = s; }

    virtual IntRect convertToContainingView(const IntRect&) const override;
    virtual IntRect convertFromContainingView(const IntRect&) const override;

    virtual IntPoint convertToContainingView(const IntPoint&) const override;
    virtual IntPoint convertFromContainingView(const IntPoint&) const override;

    void moveThumb(int pos, bool draggingDocument = false);

    virtual bool isAlphaLocked() const override { return m_isAlphaLocked; }
    virtual void setIsAlphaLocked(bool flag) override { m_isAlphaLocked = flag; }

    bool overlapsResizer() const { return m_overlapsResizer; }
    void setOverlapsResizer(bool overlapsResizer) { m_overlapsResizer = overlapsResizer; }

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
    virtual bool isScrollbar() const override { return true; }

    float scrollableAreaCurrentPos() const;
};

DEFINE_TYPE_CASTS(Scrollbar, Widget, widget, widget->isScrollbar(), widget.isScrollbar());

} // namespace blink

#endif // Scrollbar_h
