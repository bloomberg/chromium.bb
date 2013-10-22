/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RenderWidget_h
#define RenderWidget_h

#include "core/rendering/RenderReplaced.h"
#include "platform/Widget.h"

namespace WebCore {

class RenderWidget : public RenderReplaced {
public:
    virtual ~RenderWidget();

    Widget* widget() const { return m_widget.get(); }
    virtual void setWidget(PassRefPtr<Widget>);

    void updateWidgetPosition();
    void widgetPositionsUpdated();
    IntRect windowClipRect() const;

    void setIsOverlapped(bool);

    void ref() { ++m_refCount; }
    void deref();

    class UpdateSuspendScope {
    public:
        UpdateSuspendScope();
        ~UpdateSuspendScope();
    };

protected:
    RenderWidget(Element*);

    FrameView* frameView() const { return m_frameView; }

    void clearWidget();

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE FINAL;
    virtual void layout();
    virtual void paint(PaintInfo&, const LayoutPoint&);
    virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const;
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) OVERRIDE;

    virtual void paintContents(PaintInfo&, const LayoutPoint&);

private:
    virtual bool isWidget() const OVERRIDE FINAL { return true; }

    virtual void willBeDestroyed() OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;

    bool setWidgetGeometry(const LayoutRect&);
    bool updateWidgetGeometry();

    RefPtr<Widget> m_widget;
    FrameView* m_frameView;
    IntRect m_clipRect; // The rectangle needs to remain correct after scrolling, so it is stored in content view coordinates, and not clipped to window.
    int m_refCount;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderWidget, isWidget());

} // namespace WebCore

#endif // RenderWidget_h
