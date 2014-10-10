/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#ifndef RenderPart_h
#define RenderPart_h

#include "core/rendering/RenderReplaced.h"
#include "platform/Widget.h"

namespace blink {

// Renderer for frames via RenderFrame and RenderIFrame, and plug-ins via RenderEmbeddedObject.
class RenderPart : public RenderReplaced {
public:
    explicit RenderPart(Element*);
    virtual ~RenderPart();

    bool requiresAcceleratedCompositing() const;

    virtual bool needsPreferredWidthsRecalculation() const override final;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;

#if !ENABLE(OILPAN)
    void ref() { ++m_refCount; }
    void deref();
#endif

    Widget* widget() const;

    void updateOnWidgetChange();
    void updateWidgetPosition();
    void widgetPositionsUpdated();
    bool updateWidgetGeometry();

    virtual bool isRenderPart() const override final { return true; }

protected:
    virtual LayerType layerTypeRequired() const override;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override final;
    virtual void layout() override;
    virtual void paint(PaintInfo&, const LayoutPoint&) override;
    virtual void paintContents(PaintInfo&, const LayoutPoint&);
    virtual CursorDirective getCursor(const LayoutPoint&, Cursor&) const override final;

private:
    virtual const char* renderName() const override { return "RenderPart"; }

    virtual CompositingReasons additionalCompositingReasons() const override;

    virtual void willBeDestroyed() override final;
    virtual void destroy() override final;

    bool setWidgetGeometry(const LayoutRect&);

    bool nodeAtPointOverWidget(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);

#if !ENABLE(OILPAN)
    int m_refCount;
#endif
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderPart, isRenderPart());

} // namespace blink

#endif // RenderPart_h
