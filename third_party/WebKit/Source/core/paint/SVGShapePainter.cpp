// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGShapePainter.h"

#include "core/paint/ObjectPainter.h"
#include "core/paint/SVGContainerPainter.h"
#include "core/rendering/GraphicsContextAnnotator.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGPath.h"
#include "core/rendering/svg/RenderSVGResourceMarker.h"
#include "core/rendering/svg/RenderSVGShape.h"
#include "core/rendering/svg/SVGMarkerData.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"

namespace blink {

static bool setupNonScalingStrokeContext(AffineTransform& strokeTransform, GraphicsContextStateSaver& stateSaver)
{
    if (!strokeTransform.isInvertible())
        return false;

    stateSaver.save();
    stateSaver.context()->concatCTM(strokeTransform.inverse());
    return true;
}

static void useStrokeStyleToFill(GraphicsContext* context)
{
    if (Gradient* gradient = context->strokeGradient())
        context->setFillGradient(gradient);
    else if (Pattern* pattern = context->strokePattern())
        context->setFillPattern(pattern);
    else
        context->setFillColor(context->strokeColor());
}

void SVGShapePainter::paint(PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderSVGShape);
    if (paintInfo.phase != PaintPhaseForeground
        || m_renderSVGShape.style()->visibility() == HIDDEN
        || m_renderSVGShape.isShapeEmpty())
        return;

    FloatRect boundingBox = m_renderSVGShape.paintInvalidationRectInLocalCoordinates();
    if (!SVGRenderSupport::paintInfoIntersectsPaintInvalidationRect(boundingBox, m_renderSVGShape.localTransform(), paintInfo))
        return;

    PaintInfo childPaintInfo(paintInfo);

    GraphicsContextStateSaver stateSaver(*childPaintInfo.context);
    childPaintInfo.applyTransform(m_renderSVGShape.localTransform());

    SVGRenderingContext renderingContext(&m_renderSVGShape, childPaintInfo);

    if (renderingContext.isRenderingPrepared()) {
        const SVGRenderStyle& svgStyle = m_renderSVGShape.style()->svgStyle();
        if (svgStyle.shapeRendering() == SR_CRISPEDGES)
            childPaintInfo.context->setShouldAntialias(false);

        for (int i = 0; i < 3; i++) {
            switch (svgStyle.paintOrderType(i)) {
            case PT_FILL: {
                GraphicsContextStateSaver stateSaver(*childPaintInfo.context, false);
                if (!SVGRenderSupport::updateGraphicsContext(stateSaver, m_renderSVGShape.style(), m_renderSVGShape, ApplyToFillMode))
                    break;
                fillShape(childPaintInfo.context);
                break;
            }
            case PT_STROKE:
                if (svgStyle.hasVisibleStroke()) {
                    GraphicsContextStateSaver stateSaver(*childPaintInfo.context, false);
                    AffineTransform nonScalingTransform;
                    const AffineTransform* additionalPaintServerTransform = 0;

                    if (m_renderSVGShape.hasNonScalingStroke()) {
                        nonScalingTransform = m_renderSVGShape.nonScalingStrokeTransform();
                        if (!setupNonScalingStrokeContext(nonScalingTransform, stateSaver))
                            return;

                        // Non-scaling stroke needs to reset the transform back to the host transform.
                        additionalPaintServerTransform = &nonScalingTransform;
                    }

                    if (!SVGRenderSupport::updateGraphicsContext(stateSaver, m_renderSVGShape.style(), m_renderSVGShape, ApplyToStrokeMode, additionalPaintServerTransform))
                        break;
                    strokeShape(childPaintInfo.context);
                }
                break;
            case PT_MARKERS:
                paintMarkers(childPaintInfo);
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    if (m_renderSVGShape.style()->outlineWidth())
        ObjectPainter(m_renderSVGShape).paintOutline(childPaintInfo, IntRect(boundingBox));
}

void SVGShapePainter::fillShape(GraphicsContext* context)
{
    switch (m_renderSVGShape.geometryCodePath()) {
    case RectGeometryFastPath:
        context->fillRect(m_renderSVGShape.objectBoundingBox());
        break;
    case EllipseGeometryFastPath:
        context->fillEllipse(m_renderSVGShape.objectBoundingBox());
        break;
    default:
        context->fillPath(m_renderSVGShape.path());
    }
}

void SVGShapePainter::strokeShape(GraphicsContext* context)
{
    if (!m_renderSVGShape.style()->svgStyle().hasVisibleStroke())
        return;

    switch (m_renderSVGShape.geometryCodePath()) {
    case RectGeometryFastPath:
        context->strokeRect(m_renderSVGShape.objectBoundingBox(), m_renderSVGShape.strokeWidth());
        break;
    case EllipseGeometryFastPath:
        context->strokeEllipse(m_renderSVGShape.objectBoundingBox());
        break;
    default:
        ASSERT(m_renderSVGShape.hasPath());
        Path* usePath = &m_renderSVGShape.path();
        if (m_renderSVGShape.hasNonScalingStroke())
            usePath = m_renderSVGShape.nonScalingStrokePath(usePath, m_renderSVGShape.nonScalingStrokeTransform());
        context->strokePath(*usePath);
        strokeZeroLengthLineCaps(context);
    }
}

void SVGShapePainter::paintMarkers(PaintInfo& paintInfo)
{
    const Vector<MarkerPosition>* markerPositions = m_renderSVGShape.markerPositions();
    if (!markerPositions || markerPositions->isEmpty())
        return;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(&m_renderSVGShape);
    if (!resources)
        return;

    RenderSVGResourceMarker* markerStart = resources->markerStart();
    RenderSVGResourceMarker* markerMid = resources->markerMid();
    RenderSVGResourceMarker* markerEnd = resources->markerEnd();
    if (!markerStart && !markerMid && !markerEnd)
        return;

    float strokeWidth = m_renderSVGShape.strokeWidth();
    unsigned size = markerPositions->size();
    for (unsigned i = 0; i < size; ++i) {
        if (RenderSVGResourceMarker* marker = SVGMarkerData::markerForType((*markerPositions)[i].type, markerStart, markerMid, markerEnd))
            paintMarker(paintInfo, *marker, (*markerPositions)[i], strokeWidth);
    }
}

void SVGShapePainter::paintMarker(PaintInfo& paintInfo, RenderSVGResourceMarker& marker, const MarkerPosition& position, float strokeWidth)
{
    // An empty viewBox disables rendering.
    SVGMarkerElement* markerElement = toSVGMarkerElement(marker.element());
    ASSERT(markerElement);
    if (markerElement->hasAttribute(SVGNames::viewBoxAttr) && markerElement->viewBox()->currentValue()->isValid() && markerElement->viewBox()->currentValue()->value().isEmpty())
        return;

    PaintInfo info(paintInfo);
    GraphicsContextStateSaver stateSaver(*info.context, false);
    info.applyTransform(marker.markerTransformation(position.origin, position.angle, strokeWidth), &stateSaver);

    if (SVGRenderSupport::isOverflowHidden(&marker)) {
        stateSaver.saveIfNeeded();
        info.context->clip(marker.viewport());
    }

    SVGContainerPainter(marker).paint(info);
}

void SVGShapePainter::strokeZeroLengthLineCaps(GraphicsContext* context)
{
    const Vector<FloatPoint>* zeroLengthLineCaps = m_renderSVGShape.zeroLengthLineCaps();
    if (!zeroLengthLineCaps || zeroLengthLineCaps->isEmpty())
        return;

    Path* usePath;
    AffineTransform nonScalingTransform;

    if (m_renderSVGShape.hasNonScalingStroke())
        nonScalingTransform = m_renderSVGShape.nonScalingStrokeTransform();

    GraphicsContextStateSaver stateSaver(*context, true);
    useStrokeStyleToFill(context);
    for (size_t i = 0; i < zeroLengthLineCaps->size(); ++i) {
        usePath = zeroLengthLinecapPath((*zeroLengthLineCaps)[i]);
        if (m_renderSVGShape.hasNonScalingStroke())
            usePath = m_renderSVGShape.nonScalingStrokePath(usePath, nonScalingTransform);
        context->fillPath(*usePath);
    }
}

Path* SVGShapePainter::zeroLengthLinecapPath(const FloatPoint& linecapPosition) const
{
    DEFINE_STATIC_LOCAL(Path, tempPath, ());

    tempPath.clear();
    if (m_renderSVGShape.style()->svgStyle().capStyle() == SquareCap)
        tempPath.addRect(RenderSVGPath::zeroLengthSubpathRect(linecapPosition, m_renderSVGShape.strokeWidth()));
    else
        tempPath.addEllipse(RenderSVGPath::zeroLengthSubpathRect(linecapPosition, m_renderSVGShape.strokeWidth()));

    return &tempPath;
}

} // namespace blink
