/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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
 */

#ifndef LayoutLayerModelObject_h
#define LayoutLayerModelObject_h

#include "core/layout/LayoutObject.h"

namespace blink {

class Layer;
class LayerScrollableArea;

enum LayerType {
    NoLayer,
    NormalLayer,
    // A forced or overflow clip layer is required for bookkeeping purposes,
    // but does not force a layer to be self painting.
    OverflowClipLayer,
    ForcedLayer
};

class LayoutLayerModelObject : public LayoutObject {
public:
    explicit LayoutLayerModelObject(ContainerNode*);
    virtual ~LayoutLayerModelObject();

    // This is the only way layers should ever be destroyed.
    void destroyLayer();

    bool hasSelfPaintingLayer() const;
    Layer* layer() const { return m_layer.get(); }
    LayerScrollableArea* scrollableArea() const;

    virtual void styleWillChange(StyleDifference, const LayoutStyle& newStyle) override;
    virtual void styleDidChange(StyleDifference, const LayoutStyle* oldStyle) override;
    virtual void updateFromStyle() { }

    virtual LayerType layerTypeRequired() const = 0;

    // Returns true if the background is painted opaque in the given rect.
    // The query rect is given in local coordinate system.
    virtual bool backgroundIsKnownToBeOpaqueInRect(const LayoutRect&) const { return false; }

    // This is null for anonymous renderers.
    ContainerNode* node() const { return toContainerNode(LayoutObject::node()); }

    virtual void invalidateTreeIfNeeded(const PaintInvalidationState&) override;

    // Indicate that the contents of this renderer need to be repainted. Only has an effect if compositing is being used,
    void setBackingNeedsPaintInvalidationInRect(const LayoutRect&, PaintInvalidationReason) const; // r is in the coordinate space of this render object

protected:
    void createLayer(LayerType);

    virtual void willBeDestroyed() override;

    virtual void addLayerHitTestRects(LayerHitTestRects&, const Layer*, const LayoutPoint&, const LayoutRect&) const override;

    void addChildFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset) const;

private:
    virtual bool isLayoutLayerModelObject() const override final { return true; }

    OwnPtr<Layer> m_layer;

    // Used to store state between styleWillChange and styleDidChange
    static bool s_wasFloating;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutLayerModelObject, isLayoutLayerModelObject());

} // namespace blink

#endif // LayoutLayerModelObject_h
