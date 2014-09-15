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

namespace blink {

class AffineTransform;
class FloatPoint;
class FloatRect;
class GraphicsContext;
class PaintInvalidationState;
class LayoutRect;
struct PaintInfo;
class Path;
class RenderGeometryMap;
class RenderLayerModelObject;
class RenderObject;
class RenderStyle;
class RenderSVGRoot;
class RenderSVGShape;
class StrokeData;
class TransformState;

class SVGRenderSupport {
public:
    // Shares child layouting code between RenderSVGRoot/RenderSVG(Hidden)Container
    static void layoutChildren(RenderObject*, bool selfNeedsLayout);

    // Layout resources used by this node.
    static void layoutResourcesIfNeeded(const RenderObject*);

    // Helper function determining wheter overflow is hidden
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

    static bool paintInfoIntersectsPaintInvalidationRect(const FloatRect& localPaintInvalidationRect, const AffineTransform& localTransform, const PaintInfo&);

    // Important functions used by nearly all SVG renderers centralizing coordinate transformations / paint invalidation rect calculations
    static LayoutRect clippedOverflowRectForPaintInvalidation(const RenderObject*, const RenderLayerModelObject* paintInvalidationContainer, const PaintInvalidationState*);
    static void computeFloatRectForPaintInvalidation(const RenderObject*, const RenderLayerModelObject* paintInvalidationContainer, FloatRect&, const PaintInvalidationState*);
    static void mapLocalToContainer(const RenderObject*, const RenderLayerModelObject* paintInvalidationContainer, TransformState&, bool* wasFixed = 0, const PaintInvalidationState* = 0);
    static const RenderObject* pushMappingToContainer(const RenderObject*, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&);

    // Shared between SVG renderers and resources.
    static void applyStrokeStyleToContext(GraphicsContext*, const RenderStyle*, const RenderObject*);
    static void applyStrokeStyleToStrokeData(StrokeData*, const RenderStyle*, const RenderObject*);

    // Fill and/or stroke the provided |path|.
    static void fillOrStrokePath(GraphicsContext*, unsigned short resourceMode, const Path&);

    // Determines if any ancestor's transform has changed.
    static bool transformToRootChanged(RenderObject*);

    // FIXME: These methods do not belong here.
    static const RenderSVGRoot* findTreeRootObject(const RenderObject*);

    // Helper method for determining if a RenderObject marked as text (isText()== true)
    // can/will be rendered as part of a <text>.
    static bool isRenderableTextNode(const RenderObject*);

private:
    static void updateObjectBoundingBox(FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, RenderObject* other, FloatRect otherBoundingBox);
    static bool layoutSizeOfNearestViewportChanged(const RenderObject* start);
};

} // namespace blink

#endif // SVGRenderSupport_h
