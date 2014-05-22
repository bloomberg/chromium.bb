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

#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGRoot.h"
#include "core/rendering/svg/RenderSVGShape.h"
#include "core/rendering/svg/RenderSVGText.h"
#include "core/rendering/svg/RenderSVGViewportContainer.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"

namespace WebCore {

class FloatPoint;
class FloatRect;
class ImageBuffer;
class LayoutRect;
class RenderBoxModelObject;
class RenderGeometryMap;
class RenderLayerModelObject;
class RenderObject;
class RenderStyle;
class RenderSVGRoot;
class TransformState;

class SVGRenderSupport {
public:
    // Shares child layouting code between RenderSVGRoot/RenderSVG(Hidden)Container
    template <typename RenderObjectType>
    static void layoutChildren(RenderObjectType*, bool selfNeedsLayout);

    // Layout resources used by this node.
    static void layoutResourcesIfNeeded(const RenderObject*);

    // Helper function determining wheter overflow is hidden
    static bool isOverflowHidden(const RenderObject*);

    // Calculates the repaintRect in combination with filter, clipper and masker in local coordinates.
    static void intersectRepaintRectWithResources(const RenderObject*, FloatRect&);

    // Determines whether a container needs to be laid out because it's filtered and a child is being laid out.
    static bool filtersForceContainerLayout(RenderObject*);

    // Determines whether the passed point lies in a clipping area
    static bool pointInClippingArea(RenderObject*, const FloatPoint&);

    template <typename RenderObjectType>
    static void computeContainerBoundingBoxes(const RenderObjectType* container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& repaintBoundingBox);

    static bool paintInfoIntersectsRepaintRect(const FloatRect& localRepaintRect, const AffineTransform& localTransform, const PaintInfo&);

    // Important functions used by nearly all SVG renderers centralizing coordinate transformations / repaint rect calculations
    static LayoutRect clippedOverflowRectForRepaint(const RenderObject*, const RenderLayerModelObject* repaintContainer);
    static void computeFloatRectForRepaint(const RenderObject*, const RenderLayerModelObject* repaintContainer, FloatRect&, bool fixed);
    static void mapLocalToContainer(const RenderObject*, const RenderLayerModelObject* repaintContainer, TransformState&, bool* wasFixed = 0);
    static const RenderObject* pushMappingToContainer(const RenderObject*, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&);
    static bool checkForSVGRepaintDuringLayout(RenderObject*);

    // Shared between SVG renderers and resources.
    static void applyStrokeStyleToContext(GraphicsContext*, const RenderStyle*, const RenderObject*);
    static void applyStrokeStyleToStrokeData(StrokeData*, const RenderStyle*, const RenderObject*);

    // Determines if any ancestor's transform has changed.
    static bool transformToRootChanged(RenderObject*);

    // FIXME: These methods do not belong here.
    static const RenderSVGRoot* findTreeRootObject(const RenderObject*);

    // Helper method for determining if a RenderObject marked as text (isText()== true)
    // can/will be rendered as part of a <text>.
    static bool isRenderableTextNode(const RenderObject*);

private:
    static void updateObjectBoundingBox(FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, RenderObject* other, FloatRect otherBoundingBox);
    static void invalidateResourcesOfChildren(RenderObject* start);
    static bool layoutSizeOfNearestViewportChanged(const RenderObject* start);
};

template <typename RenderObjectType>
void SVGRenderSupport::computeContainerBoundingBoxes(const RenderObjectType* container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& repaintBoundingBox)
{
    objectBoundingBox = FloatRect();
    objectBoundingBoxValid = false;
    strokeBoundingBox = FloatRect();

    // When computing the strokeBoundingBox, we use the repaintRects of the container's children so that the container's stroke includes
    // the resources applied to the children (such as clips and filters). This allows filters applied to containers to correctly bound
    // the children, and also improves inlining of SVG content, as the stroke bound is used in that situation also.
    for (RenderObject* current = container->firstChild(); current; current = current->nextSibling()) {
        if (current->isSVGHiddenContainer())
            continue;

        const AffineTransform& transform = current->localToParentTransform();
        updateObjectBoundingBox(objectBoundingBox, objectBoundingBoxValid, current,
            transform.mapRect(current->objectBoundingBox()));
        strokeBoundingBox.unite(transform.mapRect(current->repaintRectInLocalCoordinates()));
    }

    repaintBoundingBox = strokeBoundingBox;
}

template <typename RenderObjectType>
void SVGRenderSupport::layoutChildren(RenderObjectType* start, bool selfNeedsLayout)
{
    bool layoutSizeChanged = layoutSizeOfNearestViewportChanged(start);
    bool transformChanged = transformToRootChanged(start);
    HashSet<RenderObject*> notlayoutedObjects;

    for (RenderObject* child = start->firstChild(); child; child = child->nextSibling()) {
        bool needsLayout = selfNeedsLayout;
        bool childEverHadLayout = child->everHadLayout();

        if (transformChanged) {
            // If the transform changed we need to update the text metrics (note: this also happens for layoutSizeChanged=true).
            if (child->isSVGText())
                toRenderSVGText(child)->setNeedsTextMetricsUpdate();
            needsLayout = true;
        }

        if (layoutSizeChanged) {
            // When selfNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths
            if (SVGElement* element = child->node()->isSVGElement() ? toSVGElement(child->node()) : 0) {
                if (element->hasRelativeLengths()) {
                    // When the layout size changed and when using relative values tell the RenderSVGShape to update its shape object
                    if (child->isSVGShape()) {
                        toRenderSVGShape(child)->setNeedsShapeUpdate();
                    } else if (child->isSVGText()) {
                        toRenderSVGText(child)->setNeedsTextMetricsUpdate();
                        toRenderSVGText(child)->setNeedsPositioningValuesUpdate();
                    }

                    needsLayout = true;
                }
            }
        }

        SubtreeLayoutScope layoutScope(*child);
        // Resource containers are nasty: they can invalidate clients outside the current SubtreeLayoutScope.
        // Since they only care about viewport size changes (to resolve their relative lengths), we trigger
        // their invalidation directly from SVGSVGElement::svgAttributeChange() or at a higher
        // SubtreeLayoutScope (in RenderView::layout()).
        if (needsLayout && !child->isSVGResourceContainer())
            layoutScope.setNeedsLayout(child);

        layoutResourcesIfNeeded(child);

        if (child->needsLayout()) {
            child->layout();
            // Renderers are responsible for repainting themselves when changing, except
            // for the initial paint to avoid potential double-painting caused by non-sensical "old" bounds.
            // We could handle this in the individual objects, but for now it's easier to have
            // parent containers call repaint().  (RenderBlock::layout* has similar logic.)
            if (!childEverHadLayout && !RuntimeEnabledFeatures::repaintAfterLayoutEnabled())
                child->repaint();
        } else if (layoutSizeChanged) {
            notlayoutedObjects.add(child);
        }
    }

    if (!layoutSizeChanged) {
        ASSERT(notlayoutedObjects.isEmpty());
        return;
    }

    // If the layout size changed, invalidate all resources of all children that didn't go through the layout() code path.
    HashSet<RenderObject*>::iterator end = notlayoutedObjects.end();
    for (HashSet<RenderObject*>::iterator it = notlayoutedObjects.begin(); it != end; ++it)
        invalidateResourcesOfChildren(*it);
}

// Update a bounding box taking into account the validity of the other bounding box.
inline void SVGRenderSupport::updateObjectBoundingBox(FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, RenderObject* other, FloatRect otherBoundingBox)
{
    bool otherValid = other->isSVGContainer() ? toRenderSVGContainer(other)->isObjectBoundingBoxValid() : true;
    if (!otherValid)
        return;

    if (!objectBoundingBoxValid) {
        objectBoundingBox = otherBoundingBox;
        objectBoundingBoxValid = true;
        return;
    }

    objectBoundingBox.uniteEvenIfEmpty(otherBoundingBox);
}

inline void SVGRenderSupport::invalidateResourcesOfChildren(RenderObject* start)
{
    ASSERT(!start->needsLayout());
    if (SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(start))
        resources->removeClientFromCache(start, false);

    for (RenderObject* child = start->slowFirstChild(); child; child = child->nextSibling())
        invalidateResourcesOfChildren(child);
}

inline bool SVGRenderSupport::layoutSizeOfNearestViewportChanged(const RenderObject* start)
{
    while (start && !start->isSVGRoot() && !start->isSVGViewportContainer())
        start = start->parent();

    ASSERT(start);
    ASSERT(start->isSVGRoot() || start->isSVGViewportContainer());
    if (start->isSVGViewportContainer())
        return toRenderSVGViewportContainer(start)->isLayoutSizeChanged();

    return toRenderSVGRoot(start)->isLayoutSizeChanged();
}

} // namespace WebCore

#endif // SVGRenderSupport_h
