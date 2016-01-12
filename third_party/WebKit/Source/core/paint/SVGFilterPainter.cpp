// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGFilterPainter.h"

#include "core/layout/svg/LayoutSVGResourceFilter.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

#define CHECK_CTM_FOR_TRANSFORMED_IMAGEFILTER

namespace blink {

GraphicsContext* SVGFilterRecordingContext::beginContent(FilterData* filterData)
{
    ASSERT(filterData->m_state == FilterData::Initial);

    GraphicsContext* context = &paintingContext();

    // Create a new context so the contents of the filter can be drawn and cached.
    m_paintController = PaintController::create();
    m_context = adoptPtr(new GraphicsContext(*m_paintController));
    context = m_context.get();

    filterData->m_state = FilterData::RecordingContent;
    return context;
}

void SVGFilterRecordingContext::endContent(FilterData* filterData)
{
    ASSERT(filterData->m_state == FilterData::RecordingContent);

    SourceGraphic* sourceGraphic = filterData->filter->sourceGraphic();
    ASSERT(sourceGraphic);

    GraphicsContext* context = &paintingContext();

    // Use the context that contains the filtered content.
    ASSERT(m_paintController);
    ASSERT(m_context);
    context = m_context.get();
    context->beginRecording(filterData->filter->filterRegion());
    m_paintController->commitNewDisplayItems();
    m_paintController->paintArtifact().replay(*context);

    sourceGraphic->setPicture(context->endRecording());

    // Content is cached by the source graphic so temporaries can be freed.
    m_paintController = nullptr;
    m_context = nullptr;

    filterData->m_state = FilterData::ReadyToPaint;
}

static void paintFilteredContent(const LayoutObject& object, GraphicsContext& context, FilterData* filterData)
{
    ASSERT(filterData->m_state == FilterData::ReadyToPaint);
    ASSERT(filterData->filter->sourceGraphic());

    filterData->m_state = FilterData::PaintingFilter;

    SkiaImageFilterBuilder builder;
    RefPtr<SkImageFilter> imageFilter = builder.build(filterData->filter->lastEffect(), ColorSpaceDeviceRGB);
    FloatRect boundaries = filterData->filter->filterRegion();
    context.save();

    // Clip drawing of filtered image to the minimum required paint rect.
    FilterEffect* lastEffect = filterData->filter->lastEffect();
    context.clipRect(lastEffect->determineAbsolutePaintRect(lastEffect->maxEffectRect()));

#ifdef CHECK_CTM_FOR_TRANSFORMED_IMAGEFILTER
    // TODO: Remove this workaround once skew/rotation support is added in Skia
    // (https://code.google.com/p/skia/issues/detail?id=3288, crbug.com/446935).
    // If the CTM contains rotation or shearing, apply the filter to
    // the unsheared/unrotated matrix, and do the shearing/rotation
    // as a final pass.
    AffineTransform ctm = SVGLayoutSupport::deprecatedCalculateTransformToLayer(&object);
    if (ctm.b() || ctm.c()) {
        AffineTransform scaleAndTranslate;
        scaleAndTranslate.translate(ctm.e(), ctm.f());
        scaleAndTranslate.scale(ctm.xScale(), ctm.yScale());
        ASSERT(scaleAndTranslate.isInvertible());
        AffineTransform shearAndRotate = scaleAndTranslate.inverse();
        shearAndRotate.multiply(ctm);
        context.concatCTM(shearAndRotate.inverse());
        imageFilter = builder.buildTransform(shearAndRotate, imageFilter.get());
    }
#endif

    context.beginLayer(1, SkXfermode::kSrcOver_Mode, &boundaries, ColorFilterNone, imageFilter.get());
    context.endLayer();
    context.restore();

    filterData->m_state = FilterData::ReadyToPaint;
}

GraphicsContext* SVGFilterPainter::prepareEffect(const LayoutObject& object, SVGFilterRecordingContext& recordingContext)
{
    m_filter.clearInvalidationMask();

    if (FilterData* filterData = m_filter.getFilterDataForLayoutObject(&object)) {
        // If the filterData already exists we do not need to record the content
        // to be filtered. This can occur if the content was previously recorded
        // or we are in a cycle.
        if (filterData->m_state == FilterData::PaintingFilter)
            filterData->m_state = FilterData::PaintingFilterCycleDetected;

        if (filterData->m_state == FilterData::RecordingContent)
            filterData->m_state = FilterData::RecordingContentCycleDetected;

        return nullptr;
    }

    OwnPtrWillBeRawPtr<FilterData> filterData = FilterData::create();
    FloatRect referenceBox = object.objectBoundingBox();

    SVGFilterElement* filterElement = toSVGFilterElement(m_filter.element());
    FloatRect filterRegion = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement, filterElement->filterUnits()->currentValue()->enumValue(), referenceBox);
    if (filterRegion.isEmpty())
        return nullptr;

    // Create the SVGFilter object.
    bool primitiveBoundingBoxMode = filterElement->primitiveUnits()->currentValue()->enumValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    Filter::UnitScaling unitScaling = primitiveBoundingBoxMode ? Filter::BoundingBox : Filter::UserSpace;
    filterData->filter = Filter::create(referenceBox, filterRegion, 1, unitScaling);
    filterData->nodeMap = SVGFilterGraphNodeMap::create();

    IntRect sourceRegion = enclosingIntRect(intersection(filterRegion, object.strokeBoundingBox()));
    filterData->filter->sourceGraphic()->setSourceRect(sourceRegion);

    // Create all relevant filter primitives.
    SVGFilterBuilder builder(filterData->filter->sourceGraphic(), filterData->nodeMap.get());
    builder.buildGraph(filterData->filter.get(), *filterElement, referenceBox);

    FilterEffect* lastEffect = builder.lastEffect();
    if (!lastEffect)
        return nullptr;

    lastEffect->determineFilterPrimitiveSubregion(ClipToFilterRegion);
    filterData->filter->setLastEffect(lastEffect);

    FilterData* data = filterData.get();
    // TODO(pdr): Can this be moved out of painter?
    m_filter.setFilterDataForLayoutObject(const_cast<LayoutObject*>(&object), filterData.release());
    return recordingContext.beginContent(data);
}

void SVGFilterPainter::finishEffect(const LayoutObject& object, SVGFilterRecordingContext& recordingContext)
{
    FilterData* filterData = m_filter.getFilterDataForLayoutObject(&object);
    if (filterData) {
        // A painting cycle can occur when an FeImage references a source that
        // makes use of the FeImage itself. This is the first place we would hit
        // the cycle so we reset the state and continue.
        if (filterData->m_state == FilterData::PaintingFilterCycleDetected)
            filterData->m_state = FilterData::PaintingFilter;

        // Check for RecordingContent here because we may can be re-painting
        // without re-recording the contents to be filtered.
        if (filterData->m_state == FilterData::RecordingContent)
            recordingContext.endContent(filterData);

        if (filterData->m_state == FilterData::RecordingContentCycleDetected)
            filterData->m_state = FilterData::RecordingContent;
    }

    GraphicsContext& context = recordingContext.paintingContext();
    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, object, DisplayItem::SVGFilter, LayoutPoint()))
        return;

    // TODO(chrishtr): stop using an infinite rect, and instead bound the filter.
    LayoutObjectDrawingRecorder recorder(context, object, DisplayItem::SVGFilter, LayoutRect::infiniteIntRect(), LayoutPoint());
    if (filterData && filterData->m_state == FilterData::ReadyToPaint)
        paintFilteredContent(object, context, filterData);
}

}
