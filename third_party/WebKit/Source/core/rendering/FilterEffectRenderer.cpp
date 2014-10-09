/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/FilterEffectRenderer.h"

#include "core/fetch/DocumentResource.h"
#include "core/fetch/DocumentResourceReference.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/ReferenceFilterBuilder.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "platform/FloatConversion.h"
#include "platform/LengthFunctions.h"
#include "platform/graphics/ColorSpace.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/filters/FEColorMatrix.h"
#include "platform/graphics/filters/FEComponentTransfer.h"
#include "platform/graphics/filters/FEDropShadow.h"
#include "platform/graphics/filters/FEGaussianBlur.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "wtf/MathExtras.h"
#include <algorithm>

namespace blink {

static inline void endMatrixRow(Vector<float>& parameters)
{
    parameters.append(0);
    parameters.append(0);
}

static inline void lastMatrixRow(Vector<float>& parameters)
{
    parameters.append(0);
    parameters.append(0);
    parameters.append(0);
    parameters.append(1);
    parameters.append(0);
}

FilterEffectRenderer::FilterEffectRenderer()
    : Filter(AffineTransform())
{
    m_sourceGraphic = SourceGraphic::create(this);
}

FilterEffectRenderer::~FilterEffectRenderer()
{
}

bool FilterEffectRenderer::build(RenderObject* renderer, const FilterOperations& operations)
{
    // Inverse zoom the pre-zoomed CSS shorthand filters, so that they are in the same zoom as the unzoomed reference filters.
    const RenderStyle* style = renderer->style();
    float zoom = style ? style->effectiveZoom() : 1.0f;

    RefPtr<FilterEffect> previousEffect = m_sourceGraphic;
    for (size_t i = 0; i < operations.operations().size(); ++i) {
        RefPtr<FilterEffect> effect;
        FilterOperation* filterOperation = operations.operations().at(i).get();
        switch (filterOperation->type()) {
        case FilterOperation::REFERENCE: {
            RefPtr<ReferenceFilter> referenceFilter = ReferenceFilter::create();
            referenceFilter->setAbsoluteTransform(AffineTransform().scale(zoom, zoom));
            effect = ReferenceFilterBuilder::build(referenceFilter.get(), renderer, previousEffect.get(), toReferenceFilterOperation(filterOperation));
            referenceFilter->setLastEffect(effect);
            m_referenceFilters.append(referenceFilter);
            break;
        }
        case FilterOperation::GRAYSCALE: {
            Vector<float> inputParameters;
            double oneMinusAmount = clampTo(1 - toBasicColorMatrixFilterOperation(filterOperation)->amount(), 0.0, 1.0);

            // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#grayscaleEquivalent
            // for information on parameters.

            inputParameters.append(narrowPrecisionToFloat(0.2126 + 0.7874 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.7152 + 0.2848 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.0722 - 0.0722 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.2126 - 0.2126 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.7152 - 0.7152 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.0722 + 0.9278 * oneMinusAmount));
            endMatrixRow(inputParameters);

            lastMatrixRow(inputParameters);

            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_MATRIX, inputParameters);
            break;
        }
        case FilterOperation::SEPIA: {
            Vector<float> inputParameters;
            double oneMinusAmount = clampTo(1 - toBasicColorMatrixFilterOperation(filterOperation)->amount(), 0.0, 1.0);

            // See https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#sepiaEquivalent
            // for information on parameters.

            inputParameters.append(narrowPrecisionToFloat(0.393 + 0.607 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.769 - 0.769 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.189 - 0.189 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.349 - 0.349 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.686 + 0.314 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.168 - 0.168 * oneMinusAmount));
            endMatrixRow(inputParameters);

            inputParameters.append(narrowPrecisionToFloat(0.272 - 0.272 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.534 - 0.534 * oneMinusAmount));
            inputParameters.append(narrowPrecisionToFloat(0.131 + 0.869 * oneMinusAmount));
            endMatrixRow(inputParameters);

            lastMatrixRow(inputParameters);

            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_MATRIX, inputParameters);
            break;
        }
        case FilterOperation::SATURATE: {
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(toBasicColorMatrixFilterOperation(filterOperation)->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_SATURATE, inputParameters);
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(toBasicColorMatrixFilterOperation(filterOperation)->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_HUEROTATE, inputParameters);
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation* componentTransferOperation = toBasicComponentTransferFilterOperation(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
            Vector<float> transferParameters;
            transferParameters.append(narrowPrecisionToFloat(componentTransferOperation->amount()));
            transferParameters.append(narrowPrecisionToFloat(1 - componentTransferOperation->amount()));
            transferFunction.tableValues = transferParameters;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::OPACITY: {
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
            Vector<float> transferParameters;
            transferParameters.append(0);
            transferParameters.append(narrowPrecisionToFloat(toBasicComponentTransferFilterOperation(filterOperation)->amount()));
            transferFunction.tableValues = transferParameters;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, nullFunction, nullFunction, nullFunction, transferFunction);
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            transferFunction.slope = narrowPrecisionToFloat(toBasicComponentTransferFilterOperation(filterOperation)->amount());
            transferFunction.intercept = 0;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::CONTRAST: {
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            float amount = narrowPrecisionToFloat(toBasicComponentTransferFilterOperation(filterOperation)->amount());
            transferFunction.slope = amount;
            transferFunction.intercept = -0.5 * amount + 0.5;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::BLUR: {
            float stdDeviation = floatValueForLength(toBlurFilterOperation(filterOperation)->stdDeviation(), 0);
            effect = FEGaussianBlur::create(this, stdDeviation, stdDeviation);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = toDropShadowFilterOperation(filterOperation);
            float stdDeviation = dropShadowOperation->stdDeviation();
            float x = dropShadowOperation->x();
            float y = dropShadowOperation->y();
            effect = FEDropShadow::create(this, stdDeviation, stdDeviation, x, y, dropShadowOperation->color(), 1);
            break;
        }
        default:
            break;
        }

        if (effect) {
            if (filterOperation->type() != FilterOperation::REFERENCE) {
                // Unlike SVG, filters applied here should not clip to their primitive subregions.
                effect->setClipsToBounds(false);
                effect->setOperatingColorSpace(ColorSpaceDeviceRGB);
                effect->inputEffects().append(previousEffect);
            }
            previousEffect = effect.release();
        }
    }

    // We need to keep the old effects alive until this point, so that SVG reference filters
    // can share cached resources across frames.
    m_lastEffect = previousEffect;

    // If we didn't make any effects, tell our caller we are not valid
    if (!m_lastEffect.get())
        return false;

    return true;
}

void FilterEffectRenderer::updateBackingStoreRect(const FloatRect& floatFilterRect)
{
    IntRect filterRect = enclosingIntRect(floatFilterRect);
    if (!filterRect.isEmpty() && FilterEffect::isFilterSizeValid(filterRect)) {
        setSourceImageRect(filterRect);
    }
}

void FilterEffectRenderer::clearIntermediateResults()
{
    if (m_lastEffect.get())
        m_lastEffect->clearResultsRecursive();
}

LayoutRect FilterEffectRenderer::computeSourceImageRectForDirtyRect(const LayoutRect& dirtyRect)
{
    // The result of this function is the area that needs paint invalidation, so that we fully cover the "dirtyRect".
    FloatRect rectForPaintInvalidation = dirtyRect;
    float inf = std::numeric_limits<float>::infinity();
    FloatRect clipRect = FloatRect(FloatPoint(-inf, -inf), FloatSize(inf, inf));
    rectForPaintInvalidation = lastEffect()->getSourceRect(rectForPaintInvalidation, clipRect);
    return LayoutRect(rectForPaintInvalidation);
}

bool FilterEffectRendererHelper::prepareFilterEffect(RenderLayer* renderLayer, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect)
{
    ASSERT(m_haveFilterEffect && renderLayer->filterRenderer());
    m_renderLayer = renderLayer;

    FilterEffectRenderer* filter = renderLayer->filterRenderer();

    IntRect filterSourceRect = pixelSnappedIntRect(filter->computeSourceImageRectForDirtyRect(dirtyRect));

    if (filterSourceRect.isEmpty()) {
        // The dirty rect is not in view, just bail out.
        m_haveFilterEffect = false;
        return false;
    }

    m_filterBoxRect = filterBoxRect;
    filter->lastEffect()->determineFilterPrimitiveSubregion(MapRectForward);

    filter->updateBackingStoreRect(filterSourceRect);
    return true;
}

void FilterEffectRendererHelper::beginFilterEffect(GraphicsContext* context)
{
    ASSERT(m_renderLayer);

    FilterEffectRenderer* filter = m_renderLayer->filterRenderer();
    SkiaImageFilterBuilder builder(context);
    RefPtr<ImageFilter> imageFilter = builder.build(filter->lastEffect().get(), ColorSpaceDeviceRGB);
    if (!imageFilter) {
        m_haveFilterEffect = false;
        return;
    }
    context->save();
    FloatRect boundaries = mapImageFilterRect(imageFilter.get(), m_filterBoxRect);
    context->translate(m_filterBoxRect.x(), m_filterBoxRect.y());
    boundaries.move(-m_filterBoxRect.x(), -m_filterBoxRect.y());
    context->beginLayer(1, CompositeSourceOver, &boundaries, ColorFilterNone, imageFilter.get());
    context->translate(-m_filterBoxRect.x(), -m_filterBoxRect.y());
}

void FilterEffectRendererHelper::endFilterEffect(GraphicsContext* context)
{
    ASSERT(m_haveFilterEffect && m_renderLayer->filterRenderer());

    context->endLayer();
    context->restore();
}

} // namespace blink

