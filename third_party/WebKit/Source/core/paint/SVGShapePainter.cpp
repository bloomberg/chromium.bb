// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGShapePainter.h"

#include "core/layout/PaintInfo.h"
#include "core/layout/svg/LayoutSVGPath.h"
#include "core/layout/svg/LayoutSVGResourceMarker.h"
#include "core/layout/svg/LayoutSVGShape.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGMarkerData.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/GraphicsContextAnnotator.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/SVGContainerPainter.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "third_party/skia/include/core/SkPicture.h"

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

void SVGShapePainter::paint(const PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_renderSVGShape);
    if (paintInfo.phase != PaintPhaseForeground
        || m_renderSVGShape.style()->visibility() == HIDDEN
        || m_renderSVGShape.isShapeEmpty())
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    FloatRect boundingBox = m_renderSVGShape.paintInvalidationRectInLocalCoordinates();

    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_renderSVGShape.displayItemClient(), m_renderSVGShape.localTransform());
    {
        SVGPaintContext paintContext(m_renderSVGShape, paintInfoBeforeFiltering);
        if (paintContext.applyClipMaskAndFilterIfNecessary()) {
            LayoutObjectDrawingRecorder recorder(paintContext.paintInfo().context, m_renderSVGShape, paintContext.paintInfo().phase, boundingBox);
            if (!recorder.canUseCachedDrawing()) {
                const SVGLayoutStyle& svgStyle = m_renderSVGShape.style()->svgStyle();

                bool shouldAntiAlias = svgStyle.shapeRendering() != SR_CRISPEDGES;
                // We're munging GC paint attributes without saving first (and so does
                // updateGraphicsContext below). This is in line with making GC less stateful,
                // but we should keep an eye out for unintended side effects.
                //
                // FIXME: the mutation avoidance check should be in (all) the GC setters.
                if (shouldAntiAlias != paintContext.paintInfo().context->shouldAntialias())
                    paintContext.paintInfo().context->setShouldAntialias(shouldAntiAlias);

                for (int i = 0; i < 3; i++) {
                    switch (svgStyle.paintOrderType(i)) {
                    case PT_FILL: {
                        GraphicsContextStateSaver stateSaver(*paintContext.paintInfo().context, false);
                        if (!SVGLayoutSupport::updateGraphicsContext(paintContext.paintInfo(), stateSaver, m_renderSVGShape.styleRef(), m_renderSVGShape, ApplyToFillMode))
                            break;
                        fillShape(paintContext.paintInfo().context);
                        break;
                    }
                    case PT_STROKE:
                        if (svgStyle.hasVisibleStroke()) {
                            GraphicsContextStateSaver stateSaver(*paintContext.paintInfo().context, false);
                            AffineTransform nonScalingTransform;
                            const AffineTransform* additionalPaintServerTransform = 0;

                            if (m_renderSVGShape.hasNonScalingStroke()) {
                                nonScalingTransform = m_renderSVGShape.nonScalingStrokeTransform();
                                if (!setupNonScalingStrokeContext(nonScalingTransform, stateSaver))
                                    return;

                                // Non-scaling stroke needs to reset the transform back to the host transform.
                                additionalPaintServerTransform = &nonScalingTransform;
                            }

                            if (!SVGLayoutSupport::updateGraphicsContext(paintContext.paintInfo(), stateSaver, m_renderSVGShape.styleRef(), m_renderSVGShape, ApplyToStrokeMode, additionalPaintServerTransform))
                                break;
                            strokeShape(paintContext.paintInfo().context);
                        }
                        break;
                    case PT_MARKERS:
                        paintMarkers(paintContext.paintInfo());
                        break;
                    default:
                        ASSERT_NOT_REACHED();
                        break;
                    }
                }
            }
        }
    }

    if (m_renderSVGShape.style()->outlineWidth()) {
        PaintInfo outlinePaintInfo(paintInfoBeforeFiltering);
        outlinePaintInfo.phase = PaintPhaseSelfOutline;
        ObjectPainter(m_renderSVGShape).paintOutline(outlinePaintInfo, LayoutRect(boundingBox));
    }
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

void SVGShapePainter::paintMarkers(const PaintInfo& paintInfo)
{
    const Vector<MarkerPosition>* markerPositions = m_renderSVGShape.markerPositions();
    if (!markerPositions || markerPositions->isEmpty())
        return;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(&m_renderSVGShape);
    if (!resources)
        return;

    LayoutSVGResourceMarker* markerStart = resources->markerStart();
    LayoutSVGResourceMarker* markerMid = resources->markerMid();
    LayoutSVGResourceMarker* markerEnd = resources->markerEnd();
    if (!markerStart && !markerMid && !markerEnd)
        return;

    float strokeWidth = m_renderSVGShape.strokeWidth();
    unsigned size = markerPositions->size();
    for (unsigned i = 0; i < size; ++i) {
        if (LayoutSVGResourceMarker* marker = SVGMarkerData::markerForType((*markerPositions)[i].type, markerStart, markerMid, markerEnd))
            paintMarker(paintInfo, *marker, (*markerPositions)[i], strokeWidth);
    }
}

void SVGShapePainter::paintMarker(const PaintInfo& paintInfo, LayoutSVGResourceMarker& marker, const MarkerPosition& position, float strokeWidth)
{
    // An empty viewBox disables rendering.
    SVGMarkerElement* markerElement = toSVGMarkerElement(marker.element());
    ASSERT(markerElement);
    if (markerElement->hasAttribute(SVGNames::viewBoxAttr) && markerElement->viewBox()->currentValue()->isValid() && markerElement->viewBox()->currentValue()->value().isEmpty())
        return;

    OwnPtr<DisplayItemList> displayItemList;
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        displayItemList = DisplayItemList::create();
    GraphicsContext recordingContext(nullptr, displayItemList.get());
    recordingContext.beginRecording(m_renderSVGShape.paintInvalidationRectInLocalCoordinates());

    PaintInfo markerPaintInfo(paintInfo);
    markerPaintInfo.context = &recordingContext;
    {
        TransformRecorder transformRecorder(*markerPaintInfo.context, marker.displayItemClient(), marker.markerTransformation(position.origin, position.angle, strokeWidth));
        OwnPtr<FloatClipRecorder> clipRecorder;
        if (SVGLayoutSupport::isOverflowHidden(&marker))
            clipRecorder = adoptPtr(new FloatClipRecorder(recordingContext, marker.displayItemClient(), markerPaintInfo.phase, marker.viewport()));

        SVGContainerPainter(marker).paint(markerPaintInfo);
    }

    if (displayItemList)
        displayItemList->replay(&recordingContext);
    RefPtr<const SkPicture> recording = recordingContext.endRecording();
    paintInfo.context->drawPicture(recording.get());
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
        tempPath.addRect(LayoutSVGPath::zeroLengthSubpathRect(linecapPosition, m_renderSVGShape.strokeWidth()));
    else
        tempPath.addEllipse(LayoutSVGPath::zeroLengthSubpathRect(linecapPosition, m_renderSVGShape.strokeWidth()));

    return &tempPath;
}

} // namespace blink
