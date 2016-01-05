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

#ifndef LayoutSVGBlock_h
#define LayoutSVGBlock_h

#include "core/layout/LayoutBlockFlow.h"

namespace blink {

class SVGElement;

class LayoutSVGBlock : public LayoutBlockFlow {
public:
    explicit LayoutSVGBlock(SVGElement*);

    LayoutRect clippedOverflowRectForPaintInvalidation(const LayoutBoxModelObject* paintInvalidationContainer, const PaintInvalidationState* = nullptr) const final;

    void mapLocalToAncestor(const LayoutBoxModelObject* ancestor, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = nullptr, const PaintInvalidationState* = nullptr) const final;
    const LayoutObject* pushMappingToContainer(const LayoutBoxModelObject* ancestorToStopAt, LayoutGeometryMap&) const final;

    AffineTransform localTransform() const final { return m_localTransform; }

    PaintLayerType layerTypeRequired() const final { return NoPaintLayer; }

    void invalidateTreeIfNeeded(PaintInvalidationState&) override;

protected:
    void willBeDestroyed() override;
    void mapToVisibleRectInAncestorSpace(const LayoutBoxModelObject* ancestor, LayoutRect&, const PaintInvalidationState*) const final;

    AffineTransform m_localTransform;

    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVG || LayoutBlockFlow::isOfType(type); }
private:
    void updateFromStyle() final;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const final;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) final;

    bool nodeAtPoint(HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override;
};

}
#endif // LayoutSVGBlock_h
