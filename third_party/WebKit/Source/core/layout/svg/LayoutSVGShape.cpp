/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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
#include "core/layout/svg/LayoutSVGShape.h"

#include "core/layout/HitTestRequest.h"
#include "core/layout/PointerEventsHitRules.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGPathData.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGShapePainter.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGLengthContext.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/graphics/StrokeData.h"
#include "wtf/MathExtras.h"

namespace blink {

LayoutSVGShape::LayoutSVGShape(SVGGraphicsElement* node)
    : LayoutSVGModelObject(node)
    , m_needsBoundariesUpdate(false) // Default is false, the cached rects are empty from the beginning.
    , m_needsShapeUpdate(true) // Default is true, so we grab a Path object once from SVGGraphicsElement.
    , m_needsTransformUpdate(true) // Default is true, so we grab a AffineTransform object once from SVGGraphicsElement.
{
}

LayoutSVGShape::~LayoutSVGShape()
{
}

void LayoutSVGShape::createPath()
{
    clearPath();
    m_path = adoptPtr(new Path);
    ASSERT(LayoutSVGShape::isShapeEmpty());

    updatePathFromGraphicsElement(toSVGGraphicsElement(element()), path());
}

void LayoutSVGShape::updateShapeFromElement()
{
    createPath();

    m_fillBoundingBox = calculateObjectBoundingBox();
    m_strokeBoundingBox = calculateStrokeBoundingBox();
}

bool LayoutSVGShape::shapeDependentStrokeContains(const FloatPoint& point)
{
    ASSERT(m_path);
    StrokeData strokeData;
    SVGLayoutSupport::applyStrokeStyleToStrokeData(strokeData, styleRef(), *this);

    if (hasNonScalingStroke()) {
        AffineTransform nonScalingTransform = nonScalingStrokeTransform();
        Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);

        return usePath->strokeContains(nonScalingTransform.mapPoint(point), strokeData);
    }

    return m_path->strokeContains(point, strokeData);
}

bool LayoutSVGShape::shapeDependentFillContains(const FloatPoint& point, const WindRule fillRule) const
{
    return path().contains(point, fillRule);
}

bool LayoutSVGShape::fillContains(const FloatPoint& point, bool requiresFill, const WindRule fillRule)
{
    if (!m_fillBoundingBox.contains(point))
        return false;

    if (requiresFill && !SVGPaintServer::existsForLayoutObject(*this, styleRef(), ApplyToFillMode))
        return false;

    return shapeDependentFillContains(point, fillRule);
}

bool LayoutSVGShape::strokeContains(const FloatPoint& point, bool requiresStroke)
{
    if (!strokeBoundingBox().contains(point))
        return false;

    if (requiresStroke && !SVGPaintServer::existsForLayoutObject(*this, styleRef(), ApplyToStrokeMode))
        return false;

    return shapeDependentStrokeContains(point);
}

void LayoutSVGShape::updateLocalTransform()
{
    SVGGraphicsElement* graphicsElement = toSVGGraphicsElement(element());
    if (graphicsElement->hasAnimatedLocalTransform()) {
        if (m_localTransform)
            m_localTransform->setTransform(graphicsElement->calculateAnimatedLocalTransform());
        else
            m_localTransform = adoptPtr(new AffineTransform(graphicsElement->calculateAnimatedLocalTransform()));
    } else {
        m_localTransform = 0;
    }
}

void LayoutSVGShape::layout()
{
    bool updateCachedBoundariesInParents = false;

    if (m_needsShapeUpdate || m_needsBoundariesUpdate) {
        updateShapeFromElement();
        m_needsShapeUpdate = false;
        updatePaintInvalidationBoundingBox();
        m_needsBoundariesUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    if (m_needsTransformUpdate) {
        updateLocalTransform();
        m_needsTransformUpdate = false;
        updateCachedBoundariesInParents = true;
    }

    // Invalidate all resources of this client if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        SVGResourcesCache::clientLayoutChanged(this);

    // If our bounds changed, notify the parents.
    if (updateCachedBoundariesInParents)
        LayoutSVGModelObject::setNeedsBoundariesUpdate();

    clearNeedsLayout();
}

Path* LayoutSVGShape::nonScalingStrokePath(const Path* path, const AffineTransform& strokeTransform) const
{
    DEFINE_STATIC_LOCAL(Path, tempPath, ());

    tempPath = *path;
    tempPath.transform(strokeTransform);

    return &tempPath;
}

AffineTransform LayoutSVGShape::nonScalingStrokeTransform() const
{
    return toSVGGraphicsElement(element())->getScreenCTM(SVGGraphicsElement::DisallowStyleUpdate);
}

void LayoutSVGShape::paint(const PaintInfo& paintInfo, const LayoutPoint&)
{
    SVGShapePainter(*this).paint(paintInfo);
}

// This method is called from inside paintOutline() since we call paintOutline()
// while transformed to our coord system, return local coords
void LayoutSVGShape::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint&) const
{
    LayoutRect rect = LayoutRect(paintInvalidationRectInLocalCoordinates());
    if (!rect.isEmpty())
        rects.append(rect);
}

bool LayoutSVGShape::nodeAtFloatPoint(const HitTestRequest& request, HitTestResult& result, const FloatPoint& pointInParent, HitTestAction hitTestAction)
{
    // We only draw in the foreground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    FloatPoint localPoint;
    if (!SVGLayoutSupport::transformToUserSpaceAndCheckClipping(this, localToParentTransform(), pointInParent, localPoint))
        return false;

    PointerEventsHitRules hitRules(PointerEventsHitRules::SVG_GEOMETRY_HITTESTING, request, style()->pointerEvents());
    if (nodeAtFloatPointInternal(request, localPoint, hitRules)) {
        updateHitTestResult(result, roundedLayoutPoint(localPoint));
        return true;
    }

    return false;
}

bool LayoutSVGShape::nodeAtFloatPointInternal(const HitTestRequest& request, const FloatPoint& localPoint, PointerEventsHitRules hitRules)
{
    bool isVisible = (style()->visibility() == VISIBLE);
    if (isVisible || !hitRules.requireVisible) {
        const SVGLayoutStyle& svgStyle = style()->svgStyle();
        WindRule fillRule = svgStyle.fillRule();
        if (request.svgClipContent())
            fillRule = svgStyle.clipRule();
        if ((hitRules.canHitBoundingBox && objectBoundingBox().contains(localPoint))
            || (hitRules.canHitStroke && (svgStyle.hasStroke() || !hitRules.requireStroke) && strokeContains(localPoint, hitRules.requireStroke))
            || (hitRules.canHitFill && (svgStyle.hasFill() || !hitRules.requireFill) && fillContains(localPoint, hitRules.requireFill, fillRule)))
            return true;
    }
    return false;
}

FloatRect LayoutSVGShape::calculateObjectBoundingBox() const
{
    return path().boundingRect();
}

FloatRect LayoutSVGShape::calculateStrokeBoundingBox() const
{
    ASSERT(m_path);
    FloatRect strokeBoundingBox = m_fillBoundingBox;

    if (style()->svgStyle().hasStroke()) {
        StrokeData strokeData;
        SVGLayoutSupport::applyStrokeStyleToStrokeData(strokeData, styleRef(), *this);
        if (hasNonScalingStroke()) {
            AffineTransform nonScalingTransform = nonScalingStrokeTransform();
            if (nonScalingTransform.isInvertible()) {
                Path* usePath = nonScalingStrokePath(m_path.get(), nonScalingTransform);
                FloatRect strokeBoundingRect = usePath->strokeBoundingRect(strokeData);
                strokeBoundingRect = nonScalingTransform.inverse().mapRect(strokeBoundingRect);
                strokeBoundingBox.unite(strokeBoundingRect);
            }
        } else {
            strokeBoundingBox.unite(path().strokeBoundingRect(strokeData));
        }
    }

    return strokeBoundingBox;
}

void LayoutSVGShape::updatePaintInvalidationBoundingBox()
{
    m_paintInvalidationBoundingBox = strokeBoundingBox();
    if (strokeWidth() < 1.0f && !m_paintInvalidationBoundingBox.isEmpty())
        m_paintInvalidationBoundingBox.inflate(1);
    SVGLayoutSupport::intersectPaintInvalidationRectWithResources(this, m_paintInvalidationBoundingBox);
}

float LayoutSVGShape::strokeWidth() const
{
    SVGLengthContext lengthContext(element());
    return lengthContext.valueForLength(style()->svgStyle().strokeWidth());
}

}
