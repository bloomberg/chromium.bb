// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGFilterPainter.h"

#include "core/layout/PaintInfo.h"
#include "core/layout/svg/LayoutSVGResourceFilter.h"

#include "core/paint/CompositingRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SourceAlpha.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

static GraphicsContext* beginRecordingContent(GraphicsContext* context, FilterData* filterData)
{
    ASSERT(filterData->m_state == FilterData::Initial);

    // For slimming paint we need to create a new context so the contents of the
    // filter can be drawn and cached.
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        filterData->m_displayItemList = DisplayItemList::create();
        filterData->m_context = adoptPtr(new GraphicsContext(nullptr, filterData->m_displayItemList.get()));
        context = filterData->m_context.get();
    }

    context->beginRecording(filterData->boundaries);
    filterData->m_state = FilterData::RecordingContent;
    return context;
}

static void endRecordingContent(GraphicsContext* context, FilterData* filterData)
{
    ASSERT(filterData->m_state == FilterData::RecordingContent);

    // FIXME: maybe filterData should just hold onto SourceGraphic after creation?
    SourceGraphic* sourceGraphic = static_cast<SourceGraphic*>(filterData->builder->getEffectById(SourceGraphic::effectName()));
    ASSERT(sourceGraphic);

    // For slimming paint we need to use the context that contains the filtered
    // content.
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(filterData->m_displayItemList);
        ASSERT(filterData->m_context);
        filterData->m_displayItemList->replay(filterData->m_context.get());
        context = filterData->m_context.get();
    }

    sourceGraphic->setPicture(context->endRecording());

    // Content is cached by the source graphic so temporaries can be freed.
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        filterData->m_displayItemList = nullptr;
        filterData->m_context = nullptr;
    }

    filterData->m_state = FilterData::ReadyToPaint;
}

static void paintFilteredContent(GraphicsContext* context, FilterData* filterData, SVGFilterElement* filterElement)
{
    ASSERT(filterData->m_state == FilterData::ReadyToPaint);
    filterData->m_state = FilterData::PaintingFilter;

    SkiaImageFilterBuilder builder(context);
    SourceGraphic* sourceGraphic = static_cast<SourceGraphic*>(filterData->builder->getEffectById(SourceGraphic::effectName()));
    ASSERT(sourceGraphic);
    builder.setSourceGraphic(sourceGraphic);
    RefPtr<SkImageFilter> imageFilter = builder.build(filterData->builder->lastEffect(), ColorSpaceDeviceRGB);
    FloatRect boundaries = filterData->boundaries;
    context->save();

    // Clip drawing of filtered image to the minimum required paint rect.
    FilterEffect* lastEffect = filterData->builder->lastEffect();
    context->clipRect(lastEffect->determineAbsolutePaintRect(lastEffect->maxEffectRect()));
    if (filterElement->hasAttribute(SVGNames::filterResAttr)) {
        // Get boundaries in device coords.
        // FIXME: See crbug.com/382491. Is the use of getCTM OK here, given it does not include device
        // zoom or High DPI adjustments?
        FloatSize size = context->getCTM().mapSize(boundaries.size());
        // Compute the scale amount required so that the resulting offscreen is exactly filterResX by filterResY pixels.
        float filterResScaleX = filterElement->filterResX()->currentValue()->value() / size.width();
        float filterResScaleY = filterElement->filterResY()->currentValue()->value() / size.height();
        // Scale the CTM so the primitive is drawn to filterRes.
        context->scale(filterResScaleX, filterResScaleY);
        // Create a resize filter with the inverse scale.
        AffineTransform resizeMatrix;
        resizeMatrix.scale(1 / filterResScaleX, 1 / filterResScaleY);
        imageFilter = builder.buildTransform(resizeMatrix, imageFilter.get());
    }

    // See crbug.com/382491.
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        // If the CTM contains rotation or shearing, apply the filter to
        // the unsheared/unrotated matrix, and do the shearing/rotation
        // as a final pass.
        AffineTransform ctm = context->getCTM();
        if (ctm.b() || ctm.c()) {
            AffineTransform scaleAndTranslate;
            scaleAndTranslate.translate(ctm.e(), ctm.f());
            scaleAndTranslate.scale(ctm.xScale(), ctm.yScale());
            ASSERT(scaleAndTranslate.isInvertible());
            AffineTransform shearAndRotate = scaleAndTranslate.inverse();
            shearAndRotate.multiply(ctm);
            context->setCTM(scaleAndTranslate);
            imageFilter = builder.buildTransform(shearAndRotate, imageFilter.get());
        }
    }
    context->beginLayer(1, SkXfermode::kSrcOver_Mode, &boundaries, ColorFilterNone, imageFilter.get());
    context->endLayer();
    context->restore();

    filterData->m_state = FilterData::ReadyToPaint;
}

GraphicsContext* SVGFilterPainter::prepareEffect(LayoutObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);

    m_filter.clearInvalidationMask();

    if (FilterData* filterData = m_filter.getFilterDataForLayoutObject(object)) {
        // If the filterData already exists we do not need to record the content
        // to be filtered. This can occur if the content was previously recorded
        // or we are in a cycle.
        if (filterData->m_state == FilterData::PaintingFilter)
            filterData->m_state = FilterData::PaintingFilterCycleDetected;

        return nullptr;
    }

    OwnPtrWillBeRawPtr<FilterData> filterData = FilterData::create();
    FloatRect targetBoundingBox = object->objectBoundingBox();

    SVGFilterElement* filterElement = toSVGFilterElement(m_filter.element());
    filterData->boundaries = SVGLengthContext::resolveRectangle<SVGFilterElement>(filterElement, filterElement->filterUnits()->currentValue()->enumValue(), targetBoundingBox);
    if (filterData->boundaries.isEmpty())
        return nullptr;

    // Create the SVGFilter object.
    FloatRect drawingRegion = object->strokeBoundingBox();
    drawingRegion.intersect(filterData->boundaries);
    bool primitiveBoundingBoxMode = filterElement->primitiveUnits()->currentValue()->enumValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
    filterData->filter = SVGFilter::create(enclosingIntRect(drawingRegion), targetBoundingBox, filterData->boundaries, primitiveBoundingBoxMode);

    // Create all relevant filter primitives.
    filterData->builder = m_filter.buildPrimitives(filterData->filter.get());
    if (!filterData->builder)
        return nullptr;

    FilterEffect* lastEffect = filterData->builder->lastEffect();
    if (!lastEffect)
        return nullptr;

    lastEffect->determineFilterPrimitiveSubregion(ClipToFilterRegion);

    FilterData* data = filterData.get();
    m_filter.setFilterDataForLayoutObject(object, filterData.release());
    return beginRecordingContent(context, data);
}

void SVGFilterPainter::finishEffect(LayoutObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);

    FilterData* filterData = m_filter.getFilterDataForLayoutObject(object);
    if (!filterData)
        return;

    // A painting cycle can occur when an FeImage references a source that makes
    // use of the FeImage itself. This is the first place we would hit the
    // cycle so we reset the state and continue.
    if (filterData->m_state == FilterData::PaintingFilterCycleDetected) {
        filterData->m_state = FilterData::PaintingFilter;
        return;
    }

    // Check for RecordingContent here because we may can be re-painting without
    // re-recording the contents to be filtered.
    if (filterData->m_state == FilterData::RecordingContent)
        endRecordingContent(context, filterData);

    if (filterData->m_state == FilterData::ReadyToPaint)
        paintFilteredContent(context, filterData, toSVGFilterElement(m_filter.element()));
}

}
