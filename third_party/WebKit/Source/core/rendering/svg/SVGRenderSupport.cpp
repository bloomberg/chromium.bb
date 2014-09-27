/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include "config.h"
#include "core/rendering/svg/SVGRenderSupport.h"

#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/SubtreeLayoutScope.h"
#include "core/rendering/svg/RenderSVGInlineText.h"
#include "core/rendering/svg/RenderSVGResourceClipper.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"
#include "core/rendering/svg/RenderSVGRoot.h"
#include "core/rendering/svg/RenderSVGShape.h"
#include "core/rendering/svg/RenderSVGText.h"
#include "core/rendering/svg/RenderSVGViewportContainer.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGElement.h"
#include "platform/geometry/TransformState.h"
#include "platform/graphics/Path.h"

namespace blink {

LayoutRect SVGRenderSupport::clippedOverflowRectForPaintInvalidation(const RenderObject* object, const RenderLayerModelObject* paintInvalidationContainer, const PaintInvalidationState* paintInvalidationState)
{
    // Return early for any cases where we don't actually paint
    if (object->style()->visibility() != VISIBLE && !object->enclosingLayer()->hasVisibleContent())
        return LayoutRect();

    // Pass our local paint rect to computeRectForPaintInvalidation() which will
    // map to parent coords and recurse up the parent chain.
    FloatRect paintInvalidationRect = object->paintInvalidationRectInLocalCoordinates();
    paintInvalidationRect.inflate(object->style()->outlineWidth());

    if (paintInvalidationState && paintInvalidationState->canMapToContainer(paintInvalidationContainer)) {
        // Compute accumulated SVG transform and apply to local paint rect.
        AffineTransform transform = paintInvalidationState->svgTransform() * object->localToParentTransform();
        paintInvalidationRect = transform.mapRect(paintInvalidationRect);
        // FIXME: These are quirks carried forward from RenderSVGRoot::computeFloatRectForPaintInvalidation.
        LayoutRect rect;
        if (!paintInvalidationRect.isEmpty())
            rect = enclosingIntRect(paintInvalidationRect);
        // Offset by SVG root paint offset and apply clipping as needed.
        rect.move(paintInvalidationState->paintOffset());
        if (paintInvalidationState->isClipped())
            rect.intersect(paintInvalidationState->clipRect());
        return rect;
    }

    object->computeFloatRectForPaintInvalidation(paintInvalidationContainer, paintInvalidationRect, paintInvalidationState);
    return enclosingLayoutRect(paintInvalidationRect);
}

void SVGRenderSupport::computeFloatRectForPaintInvalidation(const RenderObject* object, const RenderLayerModelObject* paintInvalidationContainer, FloatRect& paintInvalidationRect, const PaintInvalidationState* paintInvalidationState)
{
    // Translate to coords in our parent renderer, and then call computeFloatRectForPaintInvalidation() on our parent.
    paintInvalidationRect = object->localToParentTransform().mapRect(paintInvalidationRect);
    object->parent()->computeFloatRectForPaintInvalidation(paintInvalidationContainer, paintInvalidationRect, paintInvalidationState);
}

void SVGRenderSupport::mapLocalToContainer(const RenderObject* object, const RenderLayerModelObject* paintInvalidationContainer, TransformState& transformState, bool* wasFixed, const PaintInvalidationState* paintInvalidationState)
{
    transformState.applyTransform(object->localToParentTransform());

    if (paintInvalidationState && paintInvalidationState->canMapToContainer(paintInvalidationContainer)) {
        // |svgTransform| contains localToBorderBoxTransform mentioned below.
        transformState.applyTransform(paintInvalidationState->svgTransform());
        transformState.move(paintInvalidationState->paintOffset());
        return;
    }

    RenderObject* parent = object->parent();

    // At the SVG/HTML boundary (aka RenderSVGRoot), we apply the localToBorderBoxTransform
    // to map an element from SVG viewport coordinates to CSS box coordinates.
    // RenderSVGRoot's mapLocalToContainer method expects CSS box coordinates.
    if (parent->isSVGRoot())
        transformState.applyTransform(toRenderSVGRoot(parent)->localToBorderBoxTransform());

    MapCoordinatesFlags mode = UseTransforms;
    parent->mapLocalToContainer(paintInvalidationContainer, transformState, mode, wasFixed, paintInvalidationState);
}

const RenderObject* SVGRenderSupport::pushMappingToContainer(const RenderObject* object, const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap)
{
    ASSERT_UNUSED(ancestorToStopAt, ancestorToStopAt != object);

    RenderObject* parent = object->parent();

    // At the SVG/HTML boundary (aka RenderSVGRoot), we apply the localToBorderBoxTransform
    // to map an element from SVG viewport coordinates to CSS box coordinates.
    // RenderSVGRoot's mapLocalToContainer method expects CSS box coordinates.
    if (parent->isSVGRoot()) {
        TransformationMatrix matrix(object->localToParentTransform());
        matrix.multiply(toRenderSVGRoot(parent)->localToBorderBoxTransform());
        geometryMap.push(object, matrix);
    } else
        geometryMap.push(object, object->localToParentTransform());

    return parent;
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

void SVGRenderSupport::computeContainerBoundingBoxes(const RenderObject* container, FloatRect& objectBoundingBox, bool& objectBoundingBoxValid, FloatRect& strokeBoundingBox, FloatRect& paintInvalidationBoundingBox)
{
    objectBoundingBox = FloatRect();
    objectBoundingBoxValid = false;
    strokeBoundingBox = FloatRect();

    // When computing the strokeBoundingBox, we use the paintInvalidationRects of the container's children so that the container's stroke includes
    // the resources applied to the children (such as clips and filters). This allows filters applied to containers to correctly bound
    // the children, and also improves inlining of SVG content, as the stroke bound is used in that situation also.
    for (RenderObject* current = container->slowFirstChild(); current; current = current->nextSibling()) {
        if (current->isSVGHiddenContainer())
            continue;

        // Don't include elements in the union that do not render.
        if (current->isSVGShape() && toRenderSVGShape(current)->isShapeEmpty())
            continue;

        const AffineTransform& transform = current->localToParentTransform();
        updateObjectBoundingBox(objectBoundingBox, objectBoundingBoxValid, current,
            transform.mapRect(current->objectBoundingBox()));
        strokeBoundingBox.unite(transform.mapRect(current->paintInvalidationRectInLocalCoordinates()));
    }

    paintInvalidationBoundingBox = strokeBoundingBox;
}

bool SVGRenderSupport::paintInfoIntersectsPaintInvalidationRect(const FloatRect& localPaintInvalidationRect, const AffineTransform& localTransform, const PaintInfo& paintInfo)
{
    return localTransform.mapRect(localPaintInvalidationRect).intersects(paintInfo.rect);
}

const RenderSVGRoot* SVGRenderSupport::findTreeRootObject(const RenderObject* start)
{
    while (start && !start->isSVGRoot())
        start = start->parent();

    ASSERT(start);
    ASSERT(start->isSVGRoot());
    return toRenderSVGRoot(start);
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

bool SVGRenderSupport::transformToRootChanged(RenderObject* ancestor)
{
    while (ancestor && !ancestor->isSVGRoot()) {
        if (ancestor->isSVGTransformableContainer())
            return toRenderSVGContainer(ancestor)->didTransformToRootUpdate();
        if (ancestor->isSVGViewportContainer())
            return toRenderSVGViewportContainer(ancestor)->didTransformToRootUpdate();
        ancestor = ancestor->parent();
    }

    return false;
}

void SVGRenderSupport::layoutChildren(RenderObject* start, bool selfNeedsLayout)
{
    // When hasRelativeLengths() is false, no descendants have relative lengths
    // (hence no one is interested in viewport size changes).
    bool layoutSizeChanged = toSVGElement(start->node())->hasRelativeLengths()
        && layoutSizeOfNearestViewportChanged(start);
    bool transformChanged = transformToRootChanged(start);

    for (RenderObject* child = start->slowFirstChild(); child; child = child->nextSibling()) {
        bool forceLayout = selfNeedsLayout;

        if (transformChanged) {
            // If the transform changed we need to update the text metrics (note: this also happens for layoutSizeChanged=true).
            if (child->isSVGText())
                toRenderSVGText(child)->setNeedsTextMetricsUpdate();
            forceLayout = true;
        }

        if (layoutSizeChanged) {
            // When selfNeedsLayout is false and the layout size changed, we have to check whether this child uses relative lengths
            if (SVGElement* element = child->node()->isSVGElement() ? toSVGElement(child->node()) : 0) {
                if (element->hasRelativeLengths()) {
                    // FIXME: this should be done on invalidation, not during layout.
                    // When the layout size changed and when using relative values tell the RenderSVGShape to update its shape object
                    if (child->isSVGShape()) {
                        toRenderSVGShape(child)->setNeedsShapeUpdate();
                    } else if (child->isSVGText()) {
                        toRenderSVGText(child)->setNeedsTextMetricsUpdate();
                        toRenderSVGText(child)->setNeedsPositioningValuesUpdate();
                    }

                    forceLayout = true;
                }
            }
        }

        SubtreeLayoutScope layoutScope(*child);
        // Resource containers are nasty: they can invalidate clients outside the current SubtreeLayoutScope.
        // Since they only care about viewport size changes (to resolve their relative lengths), we trigger
        // their invalidation directly from SVGSVGElement::svgAttributeChange() or at a higher
        // SubtreeLayoutScope (in RenderView::layout()).
        if (forceLayout && !child->isSVGResourceContainer())
            layoutScope.setNeedsLayout(child);

        // Lay out any referenced resources before the child.
        layoutResourcesIfNeeded(child);
        child->layoutIfNeeded();
    }
}

void SVGRenderSupport::layoutResourcesIfNeeded(const RenderObject* object)
{
    ASSERT(object);

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object);
    if (resources)
        resources->layoutIfNeeded();
}

bool SVGRenderSupport::isOverflowHidden(const RenderObject* object)
{
    // RenderSVGRoot should never query for overflow state - it should always clip itself to the initial viewport size.
    ASSERT(!object->isDocumentElement());

    return object->style()->overflowX() == OHIDDEN || object->style()->overflowX() == OSCROLL;
}

void SVGRenderSupport::intersectPaintInvalidationRectWithResources(const RenderObject* renderer, FloatRect& paintInvalidationRect)
{
    ASSERT(renderer);

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(renderer);
    if (!resources)
        return;

    if (RenderSVGResourceFilter* filter = resources->filter())
        paintInvalidationRect = filter->resourceBoundingBox(renderer);

    if (RenderSVGResourceClipper* clipper = resources->clipper())
        paintInvalidationRect.intersect(clipper->resourceBoundingBox(renderer));

    if (RenderSVGResourceMasker* masker = resources->masker())
        paintInvalidationRect.intersect(masker->resourceBoundingBox(renderer));
}

bool SVGRenderSupport::filtersForceContainerLayout(RenderObject* object)
{
    // If any of this container's children need to be laid out, and a filter is applied
    // to the container, we need to issue paint invalidations the entire container.
    if (!object->normalChildNeedsLayout())
        return false;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object);
    if (!resources || !resources->filter())
        return false;

    return true;
}

bool SVGRenderSupport::pointInClippingArea(RenderObject* object, const FloatPoint& point)
{
    ASSERT(object);

    // We just take clippers into account to determine if a point is on the node. The Specification may
    // change later and we also need to check maskers.
    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(object);
    if (!resources)
        return true;

    if (RenderSVGResourceClipper* clipper = resources->clipper())
        return clipper->hitTestClipContent(object->objectBoundingBox(), point);

    return true;
}

bool SVGRenderSupport::transformToUserSpaceAndCheckClipping(RenderObject* object, const AffineTransform& localTransform, const FloatPoint& pointInParent, FloatPoint& localPoint)
{
    if (!localTransform.isInvertible())
        return false;
    localPoint = localTransform.inverse().mapPoint(pointInParent);
    return pointInClippingArea(object, localPoint);
}

void SVGRenderSupport::applyStrokeStyleToContext(GraphicsContext* context, const RenderStyle* style, const RenderObject* object)
{
    ASSERT(context);
    ASSERT(style);
    ASSERT(object);
    ASSERT(object->node());
    ASSERT(object->node()->isSVGElement());

    const SVGRenderStyle& svgStyle = style->svgStyle();

    SVGLengthContext lengthContext(toSVGElement(object->node()));
    context->setStrokeThickness(svgStyle.strokeWidth()->value(lengthContext));
    context->setLineCap(svgStyle.capStyle());
    context->setLineJoin(svgStyle.joinStyle());
    context->setMiterLimit(svgStyle.strokeMiterLimit());

    RefPtr<SVGLengthList> dashes = svgStyle.strokeDashArray();
    if (dashes->isEmpty())
        return;

    DashArray dashArray;
    SVGLengthList::ConstIterator it = dashes->begin();
    SVGLengthList::ConstIterator itEnd = dashes->end();
    for (; it != itEnd; ++it)
        dashArray.append(it->value(lengthContext));

    context->setLineDash(dashArray, svgStyle.strokeDashOffset()->value(lengthContext));
}

void SVGRenderSupport::applyStrokeStyleToStrokeData(StrokeData* strokeData, const RenderStyle* style, const RenderObject* object)
{
    ASSERT(strokeData);
    ASSERT(style);
    ASSERT(object);
    ASSERT(object->node());
    ASSERT(object->node()->isSVGElement());

    const SVGRenderStyle& svgStyle = style->svgStyle();

    SVGLengthContext lengthContext(toSVGElement(object->node()));
    strokeData->setThickness(svgStyle.strokeWidth()->value(lengthContext));
    strokeData->setLineCap(svgStyle.capStyle());
    strokeData->setLineJoin(svgStyle.joinStyle());
    strokeData->setMiterLimit(svgStyle.strokeMiterLimit());

    RefPtr<SVGLengthList> dashes = svgStyle.strokeDashArray();
    if (dashes->isEmpty())
        return;

    DashArray dashArray;
    size_t length = dashes->length();
    for (size_t i = 0; i < length; ++i)
        dashArray.append(dashes->at(i)->value(lengthContext));

    strokeData->setLineDash(dashArray, svgStyle.strokeDashOffset()->value(lengthContext));
}

void SVGRenderSupport::fillOrStrokePath(GraphicsContext* context, unsigned short resourceMode, const Path& path)
{
    ASSERT(resourceMode != ApplyToDefaultMode);

    if (resourceMode & ApplyToFillMode)
        context->fillPath(path);
    if (resourceMode & ApplyToStrokeMode)
        context->strokePath(path);
}

bool SVGRenderSupport::isRenderableTextNode(const RenderObject* object)
{
    ASSERT(object->isText());
    // <br> is marked as text, but is not handled by the SVG rendering code-path.
    return object->isSVGInlineText() && !toRenderSVGInlineText(object)->hasEmptyText();
}

}
