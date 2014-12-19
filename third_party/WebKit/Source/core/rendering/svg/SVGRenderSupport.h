/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGRenderSupport_h
#define SVGRenderSupport_h

#include "core/rendering/svg/RenderSVGResourcePaintServer.h"

namespace blink {

class AffineTransform;
class FloatPoint;
class FloatRect;
class GraphicsContext;
class GraphicsContextStateSaver;
class PaintInvalidationState;
class LayoutRect;
struct PaintInfo;
class RenderGeometryMap;
class RenderLayerModelObject;
class RenderObject;
class RenderStyle;
class RenderSVGRoot;
class StrokeData;
class TransformState;

class SVGRenderSupport {
public:
    // Shares child layouting code between RenderSVGRoot/RenderSVG(Hidden)Container
    static void layoutChildren(RenderObject*, bool selfNeedsLayout);

    // Layout resources used by this node.
    static void layoutResourcesIfNeeded(const RenderObject*);

    // Helper function determining whether overflow is hidden.
    static bool isOverflowHidden(const RenderObject*);

    // Calculates the paintInvalidationRect in combination with filter, clipper and masker in local coordinates.
    static void intersectPaintInvalidationRectWithResources(const RenderObject*, FloatRect&);

    // Determines whether a container needs to be laid out because it's filtered and a child is being laid out.
    static bool filtersForceContainerLayout(RenderObject*);

    // Determines whether the passed point lies in a clipping area
    static bool pointInClippingArea(RenderObject*, const FloatPoint&);

    // Transform |pointInParent| to |object|'s user-space and check if it is
    // within the clipping area. Returns false if the transform is singular or
    // the point is outside the clipping area.
    static bool transformToUserSpaceAndCheckClipping(RenderObject*, const AffineTransform& localTransform, const FloatPoint& pointInParent, FloatPoint& localPoint);

    static void computeContainerBoundingBoxes(const RenderObject* container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& paintInvalidationBoundingBox);

    // Important functions used by nearly all SVG renderers centralizing coordinate transformations / paint invalidation rect calculations
    static LayoutRect clippedOverflowRectForPaintInvalidation(const RenderObject*, const RenderLayerModelObject* paintInvalidationContainer, const PaintInvalidationState*);
    static const RenderSVGRoot& mapRectToSVGRootForPaintInvalidation(const RenderObject*, const FloatRect& localPaintInvalidationRect, LayoutRect&);
    static void mapLocalToContainer(const RenderObject*, const RenderLayerModelObject* paintInvalidationContainer, TransformState&, bool* wasFixed = 0, const PaintInvalidationState* = 0);
    static const RenderObject* pushMappingToContainer(const RenderObject*, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&);

    // Shared between SVG renderers and resources.
    static void applyStrokeStyleToContext(GraphicsContext*, const RenderStyle*, const RenderObject*);
    static void applyStrokeStyleToStrokeData(StrokeData*, const RenderStyle*, const RenderObject*);

    // Update the GC state (on |paintInfo.context|) for painting |renderer|
    // using |style|. |resourceMode| is used to decide between fill/stroke.
    // Previous state will be saved (if needed) using |stateSaver|.
    static bool updateGraphicsContext(const PaintInfo&, GraphicsContextStateSaver&, RenderStyle*, RenderObject&, RenderSVGResourceMode, const AffineTransform* additionalPaintServerTransform = 0);

    // Determines if any ancestor's transform has changed.
    static bool transformToRootChanged(RenderObject*);

    // FIXME: These methods do not belong here.
    static const RenderSVGRoot* findTreeRootObject(const RenderObject*);

    // Helper method for determining if a RenderObject marked as text (isText()== true)
    // can/will be rendered as part of a <text>.
    static bool isRenderableTextNode(const RenderObject*);

    // Determines whether a svg node should isolate or not based on RenderStyle.
    static bool willIsolateBlendingDescendantsForStyle(const RenderStyle*);
    static bool willIsolateBlendingDescendantsForObject(const RenderObject*);
    template<typename RenderObjectType>
    static bool computeHasNonIsolatedBlendingDescendants(const RenderObjectType*);
    static bool isIsolationRequired(const RenderObject*);

private:
    static void updateObjectBoundingBox(FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, RenderObject* other, FloatRect otherBoundingBox);
    static bool layoutSizeOfNearestViewportChanged(const RenderObject* start);
};

template <typename RenderObjectType>
bool SVGRenderSupport::computeHasNonIsolatedBlendingDescendants(const RenderObjectType* object)
{
    for (RenderObject* child = object->firstChild(); child; child = child->nextSibling()) {
        if (child->isBlendingAllowed() && child->style()->hasBlendMode())
            return true;
        if (child->hasNonIsolatedBlendingDescendants() && !willIsolateBlendingDescendantsForObject(child))
            return true;
    }
    return false;
}

} // namespace blink

#endif // SVGRenderSupport_h
