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

#include "core/dom/Document.h"
#include "core/loader/cache/DocumentResource.h"
#include "core/loader/cache/DocumentResourceReference.h"
#include "core/page/Page.h"
#include "core/platform/FloatConversion.h"
#include "core/platform/graphics/ColorSpace.h"
#include "core/platform/graphics/filters/FEColorMatrix.h"
#include "core/platform/graphics/filters/FEComponentTransfer.h"
#include "core/platform/graphics/filters/FEDropShadow.h"
#include "core/platform/graphics/filters/FEGaussianBlur.h"
#include "core/platform/graphics/filters/custom/CustomFilterGlobalContext.h"
#include "core/platform/graphics/filters/custom/CustomFilterValidatedProgram.h"
#include "core/platform/graphics/filters/custom/FECustomFilter.h"
#include "core/platform/graphics/filters/custom/ValidatedCustomFilterOperation.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/svg/ReferenceFilterBuilder.h"

#include "core/svg/SVGElement.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "wtf/MathExtras.h"
#include <algorithm>

namespace WebCore {

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

inline bool isFilterSizeValid(FloatRect rect)
{
    if (rect.width() < 0 || rect.width() > kMaxFilterSize
        || rect.height() < 0 || rect.height() > kMaxFilterSize)
        return false;
    return true;
}

static PassRefPtr<FECustomFilter> createCustomFilterEffect(Filter* filter, Document* document, ValidatedCustomFilterOperation* operation)
{
    if (!document)
        return 0;

    CustomFilterGlobalContext* globalContext = document->renderView()->customFilterGlobalContext();
    globalContext->prepareContextIfNeeded();
    if (!globalContext->context())
        return 0;

    return FECustomFilter::create(filter, globalContext->context(), operation->validatedProgram(), operation->parameters(),
        operation->meshRows(), operation->meshColumns(),  operation->meshType());
}

FilterEffectRenderer::FilterEffectRenderer()
    : Filter(AffineTransform())
    , m_graphicsBufferAttached(false)
    , m_hasFilterThatMovesPixels(false)
    , m_hasCustomShaderFilter(false)
{
    setFilterResolution(FloatSize(1, 1));
    m_sourceGraphic = SourceGraphic::create(this);
}

FilterEffectRenderer::~FilterEffectRenderer()
{
}

GraphicsContext* FilterEffectRenderer::inputContext()
{
    return sourceImage() ? sourceImage()->context() : 0;
}

bool FilterEffectRenderer::build(RenderObject* renderer, const FilterOperations& operations)
{
    m_hasCustomShaderFilter = false;
    m_hasFilterThatMovesPixels = operations.hasFilterThatMovesPixels();

    // Inverse zoom the pre-zoomed CSS shorthand filters, so that they are in the same zoom as the unzoomed reference filters.
    const RenderStyle* style = renderer->style();
    // FIXME: The effects now contain high dpi information, but the software path doesn't (yet) scale its backing.
    //        When the proper dpi dependant backing size is allocated, we should remove deviceScaleFactor(...) here.
    float invZoom = 1.0f / ((style ? style->effectiveZoom() : 1.0f) * deviceScaleFactor(renderer->frame()));

    RefPtr<FilterEffect> previousEffect = m_sourceGraphic;
    for (size_t i = 0; i < operations.operations().size(); ++i) {
        RefPtr<FilterEffect> effect;
        FilterOperation* filterOperation = operations.operations().at(i).get();
        switch (filterOperation->getOperationType()) {
        case FilterOperation::REFERENCE: {
            ReferenceFilterOperation* referenceOperation = static_cast<ReferenceFilterOperation*>(filterOperation);
            effect = ReferenceFilterBuilder::build(this, renderer, previousEffect.get(), referenceOperation);
            break;
        }
        case FilterOperation::GRAYSCALE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            Vector<float> inputParameters;
            double oneMinusAmount = clampTo(1 - colorMatrixOperation->amount(), 0.0, 1.0);

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
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            Vector<float> inputParameters;
            double oneMinusAmount = clampTo(1 - colorMatrixOperation->amount(), 0.0, 1.0);

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
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(colorMatrixOperation->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_SATURATE, inputParameters);
            break;
        }
        case FilterOperation::HUE_ROTATE: {
            BasicColorMatrixFilterOperation* colorMatrixOperation = static_cast<BasicColorMatrixFilterOperation*>(filterOperation);
            Vector<float> inputParameters;
            inputParameters.append(narrowPrecisionToFloat(colorMatrixOperation->amount()));
            effect = FEColorMatrix::create(this, FECOLORMATRIX_TYPE_HUEROTATE, inputParameters);
            break;
        }
        case FilterOperation::INVERT: {
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
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
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_TABLE;
            Vector<float> transferParameters;
            transferParameters.append(0);
            transferParameters.append(narrowPrecisionToFloat(componentTransferOperation->amount()));
            transferFunction.tableValues = transferParameters;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, nullFunction, nullFunction, nullFunction, transferFunction);
            break;
        }
        case FilterOperation::BRIGHTNESS: {
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            transferFunction.slope = narrowPrecisionToFloat(componentTransferOperation->amount());
            transferFunction.intercept = 0;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::CONTRAST: {
            BasicComponentTransferFilterOperation* componentTransferOperation = static_cast<BasicComponentTransferFilterOperation*>(filterOperation);
            ComponentTransferFunction transferFunction;
            transferFunction.type = FECOMPONENTTRANSFER_TYPE_LINEAR;
            float amount = narrowPrecisionToFloat(componentTransferOperation->amount());
            transferFunction.slope = amount;
            transferFunction.intercept = -0.5 * amount + 0.5;

            ComponentTransferFunction nullFunction;
            effect = FEComponentTransfer::create(this, transferFunction, transferFunction, transferFunction, nullFunction);
            break;
        }
        case FilterOperation::BLUR: {
            BlurFilterOperation* blurOperation = static_cast<BlurFilterOperation*>(filterOperation);
            float stdDeviation = floatValueForLength(blurOperation->stdDeviation(), 0) * invZoom;
            effect = FEGaussianBlur::create(this, stdDeviation, stdDeviation);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            DropShadowFilterOperation* dropShadowOperation = static_cast<DropShadowFilterOperation*>(filterOperation);
            float stdDeviation = dropShadowOperation->stdDeviation() * invZoom;
            float x = dropShadowOperation->x() * invZoom;
            float y = dropShadowOperation->y() * invZoom;
            effect = FEDropShadow::create(this, stdDeviation, stdDeviation, x, y, dropShadowOperation->color(), 1);
            break;
        }
        case FilterOperation::CUSTOM:
            // CUSTOM operations are always converted to VALIDATED_CUSTOM before getting here.
            // The conversion happens in RenderLayer::computeFilterOperations.
            ASSERT_NOT_REACHED();
            break;
        case FilterOperation::VALIDATED_CUSTOM: {
            ValidatedCustomFilterOperation* customFilterOperation = static_cast<ValidatedCustomFilterOperation*>(filterOperation);
            Document* document = renderer ? renderer->document() : 0;
            effect = createCustomFilterEffect(this, document, customFilterOperation);
            if (effect)
                m_hasCustomShaderFilter = true;
            break;
        }
        default:
            break;
        }

        if (effect) {
            if (filterOperation->getOperationType() != FilterOperation::REFERENCE) {
                // Unlike SVG, filters applied here should not clip to their primitive subregions.
                effect->setClipsToBounds(false);
                effect->setOperatingColorSpace(ColorSpaceDeviceRGB);
                effect->inputEffects().append(previousEffect);
            }
            previousEffect = effect.release();
        }
    }

    // We need to keep the old effects alive until this point, so that filters like FECustomFilter
    // can share cached resources across frames.
    m_lastEffect = previousEffect;

    // If we didn't make any effects, tell our caller we are not valid
    if (!m_lastEffect.get())
        return false;

    return true;
}

bool FilterEffectRenderer::updateBackingStoreRect(const FloatRect& filterRect)
{
    if (!filterRect.isZero() && isFilterSizeValid(filterRect)) {
        FloatRect currentSourceRect = sourceImageRect();
        if (filterRect != currentSourceRect) {
            setSourceImageRect(filterRect);
            return true;
        }
    }
    return false;
}

void FilterEffectRenderer::allocateBackingStoreIfNeeded()
{
    // At this point the effect chain has been built, and the
    // source image sizes set. We just need to attach the graphic
    // buffer if we have not yet done so.
    if (!m_graphicsBufferAttached) {
        IntSize logicalSize(m_sourceDrawingRegion.width(), m_sourceDrawingRegion.height());
        if (!sourceImage() || sourceImage()->logicalSize() != logicalSize)
            setSourceImage(ImageBuffer::create(logicalSize, 1, renderingMode()));
        m_graphicsBufferAttached = true;
    }
}

void FilterEffectRenderer::clearIntermediateResults()
{
    if (m_lastEffect.get())
        m_lastEffect->clearResultsRecursive();
}

void FilterEffectRenderer::apply()
{
    RefPtr<FilterEffect> effect = lastEffect();
    effect->apply();
    effect->transformResultColorSpace(ColorSpaceDeviceRGB);
}

LayoutRect FilterEffectRenderer::computeSourceImageRectForDirtyRect(const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect)
{
    if (hasCustomShaderFilter()) {
        // When we have at least a custom shader in the chain, we need to compute the whole source image, because the shader can
        // reference any pixel and we cannot control that.
        return filterBoxRect;
    }
    // The result of this function is the area in the "filterBoxRect" that needs to be repainted, so that we fully cover the "dirtyRect".
    FloatRect rectForRepaint = dirtyRect;
    rectForRepaint.move(-filterBoxRect.location().x(), -filterBoxRect.location().y());
    float inf = std::numeric_limits<float>::infinity();
    FloatRect clipRect = FloatRect(FloatPoint(-inf, -inf), FloatSize(inf, inf));
    rectForRepaint = lastEffect()->getSourceRect(rectForRepaint, clipRect);
    rectForRepaint.move(filterBoxRect.location().x(), filterBoxRect.location().y());
    rectForRepaint.intersect(filterBoxRect);
    return LayoutRect(rectForRepaint);
}

bool FilterEffectRendererHelper::prepareFilterEffect(RenderLayer* renderLayer, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect)
{
    ASSERT(m_haveFilterEffect && renderLayer->filterRenderer());
    m_renderLayer = renderLayer;
    m_repaintRect = dirtyRect;

    FilterEffectRenderer* filter = renderLayer->filterRenderer();
    LayoutRect filterSourceRect = filter->computeSourceImageRectForDirtyRect(filterBoxRect, dirtyRect);

    if (filterSourceRect.isEmpty()) {
        // The dirty rect is not in view, just bail out.
        m_haveFilterEffect = false;
        return false;
    }

    // Get the zoom factor to scale the filterSourceRect input
    const RenderLayerModelObject* renderer = renderLayer->renderer();
    const RenderStyle* style = renderer ? renderer->style() : 0;
    float zoom = style ? style->effectiveZoom() : 1.0f;

    AffineTransform absoluteTransform;
    absoluteTransform.translate(filterBoxRect.x(), filterBoxRect.y());
    filter->setAbsoluteTransform(absoluteTransform);
    filter->setAbsoluteFilterRegion(AffineTransform().scale(zoom).mapRect(filterSourceRect));
    filter->setFilterRegion(absoluteTransform.inverse().mapRect(filterSourceRect));
    filter->lastEffect()->determineFilterPrimitiveSubregion();

    bool hasUpdatedBackingStore = filter->updateBackingStoreRect(filterSourceRect);
    if (filter->hasFilterThatMovesPixels()) {
        if (hasUpdatedBackingStore)
            m_repaintRect = filterSourceRect;
        else {
            m_repaintRect.unite(layerRepaintRect);
            m_repaintRect.intersect(filterSourceRect);
        }
    }
    return true;
}

GraphicsContext* FilterEffectRendererHelper::beginFilterEffect(GraphicsContext* oldContext)
{
    ASSERT(m_renderLayer);

    FilterEffectRenderer* filter = m_renderLayer->filterRenderer();
    filter->allocateBackingStoreIfNeeded();
    // Paint into the context that represents the SourceGraphic of the filter.
    GraphicsContext* sourceGraphicsContext = filter->inputContext();
    if (!sourceGraphicsContext || !isFilterSizeValid(filter->absoluteFilterRegion())) {
        // Disable the filters and continue.
        m_haveFilterEffect = false;
        return oldContext;
    }

    m_savedGraphicsContext = oldContext;

    // Translate the context so that the contents of the layer is captuterd in the offscreen memory buffer.
    sourceGraphicsContext->save();
    // FIXME: can we just use sourceImageRect for everything, and get rid of
    // m_repaintRect?
    FloatPoint offset = filter->sourceImageRect().location();
    sourceGraphicsContext->translate(-offset.x(), -offset.y());
    sourceGraphicsContext->clearRect(m_repaintRect);
    sourceGraphicsContext->clip(m_repaintRect);

    return sourceGraphicsContext;
}

GraphicsContext* FilterEffectRendererHelper::applyFilterEffect()
{
    ASSERT(m_haveFilterEffect && m_renderLayer->filterRenderer());
    FilterEffectRenderer* filter = m_renderLayer->filterRenderer();
    filter->inputContext()->restore();

    filter->apply();

    // Get the filtered output and draw it in place.
    m_savedGraphicsContext->drawImageBuffer(filter->output(), filter->outputRect(), CompositeSourceOver);

    filter->clearIntermediateResults();

    return m_savedGraphicsContext;
}

} // namespace WebCore

