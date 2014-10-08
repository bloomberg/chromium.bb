/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef RenderSVGBlock_h
#define RenderSVGBlock_h

#include "core/rendering/RenderBlockFlow.h"

namespace blink {

class SVGElement;

class RenderSVGBlock : public RenderBlockFlow {
public:
    explicit RenderSVGBlock(SVGElement*);

    virtual LayoutRect visualOverflowRect() const override final;

    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, const PaintInvalidationState* = 0) const override final;
    virtual void computeFloatRectForPaintInvalidation(const RenderLayerModelObject* paintInvalidationContainer, FloatRect&, const PaintInvalidationState*) const override final;

    virtual void mapLocalToContainer(const RenderLayerModelObject* paintInvalidationContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0, const PaintInvalidationState* = 0) const override final;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override final;

    virtual AffineTransform localTransform() const override final { return m_localTransform; }

    virtual LayerType layerTypeRequired() const override final { return NoLayer; }

    virtual void invalidateTreeIfNeeded(const PaintInvalidationState&) override;

protected:
    virtual void willBeDestroyed() override;

    AffineTransform m_localTransform;

private:
    virtual void updateFromStyle() override final;

    virtual bool isSVG() const override final { return true; }

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override final;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override final;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
};

}
#endif
