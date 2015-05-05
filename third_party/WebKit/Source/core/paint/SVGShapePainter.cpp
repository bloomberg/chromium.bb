// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGShapePainter.h"

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
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGContainerPainter.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/DisplayItemListContextRecorder.h"
#include "third_party/skia/include/core/SkPaint.h"
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

static SkPath::FillType fillRuleFromStyle(const PaintInfo& paintInfo, const SVGComputedStyle& svgStyle)
{
    return WebCoreWindRuleToSkFillType(paintInfo.isRenderingClipPathAsMaskImage() ? svgStyle.clipRule() : svgStyle.fillRule());
}

void SVGShapePainter::paint(const PaintInfo& paintInfo)
{
    ANNOTATE_GRAPHICS_CONTEXT(paintInfo, &m_layoutSVGShape);
    if (paintInfo.phase != PaintPhaseForeground
        || m_layoutSVGShape.style()->visibility() == HIDDEN
        || m_layoutSVGShape.isShapeEmpty())
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    FloatRect boundingBox = m_layoutSVGShape.paintInvalidationRectInLocalCoordinates();

    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_layoutSVGShape, m_layoutSVGShape.localTransform());
    {
        SVGPaintContext paintContext(m_layoutSVGShape, paintInfoBeforeFiltering);
        if (paintContext.applyClipMaskAndFilterIfNecessary()) {
            LayoutObjectDrawingRecorder recorder(*paintContext.paintInfo().context, m_layoutSVGShape, paintContext.paintInfo().phase, boundingBox);
            if (!recorder.canUseCachedDrawing()) {
                const SVGComputedStyle& svgStyle = m_layoutSVGShape.style()->svgStyle();

                bool shouldAntiAlias = svgStyle.shapeRendering() != SR_CRISPEDGES;

                for (int i = 0; i < 3; i++) {
                    switch (svgStyle.paintOrderType(i)) {
                    case PT_FILL: {
                        SkPaint fillPaint;
                        if (!SVGPaintContext::paintForLayoutObject(paintContext.paintInfo(), m_layoutSVGShape.styleRef(), m_layoutSVGShape, ApplyToFillMode, fillPaint))
                            break;
                        fillPaint.setAntiAlias(shouldAntiAlias);
                        fillShape(paintContext.paintInfo().context, fillPaint, fillRuleFromStyle(paintContext.paintInfo(), svgStyle));
                        break;
                    }
                    case PT_STROKE:
                        if (svgStyle.hasVisibleStroke()) {
                            GraphicsContextStateSaver stateSaver(*paintContext.paintInfo().context, false);
                            AffineTransform nonScalingTransform;
                            const AffineTransform* additionalPaintServerTransform = 0;

                            if (m_layoutSVGShape.hasNonScalingStroke()) {
                                nonScalingTransform = m_layoutSVGShape.nonScalingStrokeTransform();
                                if (!setupNonScalingStrokeContext(nonScalingTransform, stateSaver))
                                    return;

                                // Non-scaling stroke needs to reset the transform back to the host transform.
                                additionalPaintServerTransform = &nonScalingTransform;
                            }

                            SkPaint strokePaint;
                            if (!SVGPaintContext::paintForLayoutObject(paintContext.paintInfo(), m_layoutSVGShape.styleRef(), m_layoutSVGShape, ApplyToStrokeMode, strokePaint, additionalPaintServerTransform))
                                break;
                            strokePaint.setAntiAlias(shouldAntiAlias);

                            StrokeData strokeData;
                            SVGLayoutSupport::applyStrokeStyleToStrokeData(strokeData, m_layoutSVGShape.styleRef(), m_layoutSVGShape);
                            strokeData.setupPaint(&strokePaint);

                            strokeShape(paintContext.paintInfo().context, strokePaint);
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

    if (m_layoutSVGShape.style()->outlineWidth()) {
        PaintInfo outlinePaintInfo(paintInfoBeforeFiltering);
        outlinePaintInfo.phase = PaintPhaseSelfOutline;
        LayoutRect layoutObjectBounds(boundingBox);
        ObjectPainter(m_layoutSVGShape).paintOutline(outlinePaintInfo, layoutObjectBounds, layoutObjectBounds);
    }
}

class PathWithTemporaryWindingRule {
public:
    PathWithTemporaryWindingRule(Path& path, SkPath::FillType fillType)
        : m_path(const_cast<SkPath&>(path.skPath()))
    {
        m_savedFillType = m_path.getFillType();
        m_path.setFillType(fillType);
    }
    ~PathWithTemporaryWindingRule()
    {
        m_path.setFillType(m_savedFillType);
    }

    const SkPath& skPath() const { return m_path; }

private:
    SkPath& m_path;
    SkPath::FillType m_savedFillType;
};

void SVGShapePainter::fillShape(GraphicsContext* context, const SkPaint& paint, SkPath::FillType fillType)
{
    switch (m_layoutSVGShape.geometryCodePath()) {
    case RectGeometryFastPath:
        context->drawRect(m_layoutSVGShape.objectBoundingBox(), paint);
        break;
    case EllipseGeometryFastPath:
        context->drawOval(m_layoutSVGShape.objectBoundingBox(), paint);
        break;
    default: {
        PathWithTemporaryWindingRule pathWithWinding(m_layoutSVGShape.path(), fillType);
        context->drawPath(pathWithWinding.skPath(), paint);
    }
    }
}

void SVGShapePainter::strokeShape(GraphicsContext* context, const SkPaint& paint)
{
    if (!m_layoutSVGShape.style()->svgStyle().hasVisibleStroke())
        return;

    switch (m_layoutSVGShape.geometryCodePath()) {
    case RectGeometryFastPath:
        context->drawRect(m_layoutSVGShape.objectBoundingBox(), paint);
        break;
    case EllipseGeometryFastPath:
        context->drawOval(m_layoutSVGShape.objectBoundingBox(), paint);
        break;
    default:
        ASSERT(m_layoutSVGShape.hasPath());
        Path* usePath = &m_layoutSVGShape.path();
        if (m_layoutSVGShape.hasNonScalingStroke())
            usePath = m_layoutSVGShape.nonScalingStrokePath(usePath, m_layoutSVGShape.nonScalingStrokeTransform());
        context->drawPath(usePath->skPath(), paint);
        strokeZeroLengthLineCaps(context, paint);
    }
}

void SVGShapePainter::paintMarkers(const PaintInfo& paintInfo)
{
    const Vector<MarkerPosition>* markerPositions = m_layoutSVGShape.markerPositions();
    if (!markerPositions || markerPositions->isEmpty())
        return;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(&m_layoutSVGShape);
    if (!resources)
        return;

    LayoutSVGResourceMarker* markerStart = resources->markerStart();
    LayoutSVGResourceMarker* markerMid = resources->markerMid();
    LayoutSVGResourceMarker* markerEnd = resources->markerEnd();
    if (!markerStart && !markerMid && !markerEnd)
        return;

    float strokeWidth = m_layoutSVGShape.strokeWidth();
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

    {
        DisplayItemListContextRecorder contextRecorder(*paintInfo.context);
        PaintInfo markerPaintInfo(paintInfo);
        markerPaintInfo.context = &contextRecorder.context();

        TransformRecorder transformRecorder(*markerPaintInfo.context, marker, marker.markerTransformation(position.origin, position.angle, strokeWidth));
        OwnPtr<FloatClipRecorder> clipRecorder;
        if (SVGLayoutSupport::isOverflowHidden(&marker))
            clipRecorder = adoptPtr(new FloatClipRecorder(*markerPaintInfo.context, marker, markerPaintInfo.phase, marker.viewport()));

        SVGContainerPainter(marker).paint(markerPaintInfo);
    }
}

void SVGShapePainter::strokeZeroLengthLineCaps(GraphicsContext* context, const SkPaint& strokePaint)
{
    const Vector<FloatPoint>* zeroLengthLineCaps = m_layoutSVGShape.zeroLengthLineCaps();
    if (!zeroLengthLineCaps || zeroLengthLineCaps->isEmpty())
        return;

    // We need a paint for filling.
    SkPaint fillPaint = strokePaint;
    fillPaint.setStyle(SkPaint::kFill_Style);

    AffineTransform nonScalingTransform;
    if (m_layoutSVGShape.hasNonScalingStroke())
        nonScalingTransform = m_layoutSVGShape.nonScalingStrokeTransform();

    Path tempPath;
    for (size_t i = 0; i < zeroLengthLineCaps->size(); ++i) {
        FloatRect subpathRect = LayoutSVGPath::zeroLengthSubpathRect((*zeroLengthLineCaps)[i], m_layoutSVGShape.strokeWidth());
        tempPath.clear();
        if (m_layoutSVGShape.style()->svgStyle().capStyle() == SquareCap)
            tempPath.addRect(subpathRect);
        else
            tempPath.addEllipse(subpathRect);
        // This open-codes LayoutSVGShape::nonScalingStrokePath, because the
        // requirements here differ (we have a temporary path that we can
        // mutate.)
        if (m_layoutSVGShape.hasNonScalingStroke())
            tempPath.transform(nonScalingTransform);
        context->drawPath(tempPath.skPath(), fillPaint);
    }
}

} // namespace blink
