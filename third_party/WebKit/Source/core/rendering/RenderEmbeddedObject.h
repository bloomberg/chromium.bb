/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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

#ifndef RenderEmbeddedObject_h
#define RenderEmbeddedObject_h

#include "core/rendering/RenderPart.h"

namespace blink {

class TextRun;

// Renderer for embeds and objects, often, but not always, rendered via plug-ins.
// For example, <embed src="foo.html"> does not invoke a plug-in.
class RenderEmbeddedObject : public RenderPart {
public:
    RenderEmbeddedObject(Element*);
    virtual ~RenderEmbeddedObject();

    enum PluginUnavailabilityReason {
        PluginMissing,
        PluginBlockedByContentSecurityPolicy,
    };
    void setPluginUnavailabilityReason(PluginUnavailabilityReason);
    bool showsUnavailablePluginIndicator() const;

private:
    virtual void paintContents(PaintInfo&, const LayoutPoint&) override final;
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) override final;
    virtual void paint(PaintInfo&, const LayoutPoint&) override final;

    virtual void layout() override final;

    virtual const char* renderName() const override { return "RenderEmbeddedObject"; }
    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectEmbeddedObject || RenderPart::isOfType(type); }
    virtual RenderBox* embeddedContentBox() const override final;

    virtual LayerType layerTypeRequired() const override final;

    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier) override final;

    bool getReplacementTextGeometry(const LayoutPoint& accumulatedOffset, FloatRect& contentRect, Path&, FloatRect& replacementTextRect, Font&, TextRun&, float& textWidth) const;

    virtual CompositingReasons additionalCompositingReasons() const override;

    bool m_showsUnavailablePluginIndicator;
    PluginUnavailabilityReason m_pluginUnavailabilityReason;
    String m_unavailablePluginReplacementText;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderEmbeddedObject, isEmbeddedObject());

} // namespace blink

#endif // RenderEmbeddedObject_h
