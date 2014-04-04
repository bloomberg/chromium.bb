/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#include "platform/graphics/filters/SkiaImageFilterBuilder.h"

#include "SkBlurImageFilter.h"
#include "SkColorFilterImageFilter.h"
#include "SkColorMatrixFilter.h"
#include "SkDropShadowImageFilter.h"
#include "SkMatrixImageFilter.h"
#include "SkTableColorFilter.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "public/platform/WebPoint.h"

namespace {

PassRefPtr<SkImageFilter> createMatrixImageFilter(SkScalar matrix[20], SkImageFilter* input)
{
    RefPtr<SkColorFilter> colorFilter(adoptRef(new SkColorMatrixFilter(matrix)));
    return adoptRef(SkColorFilterImageFilter::Create(colorFilter.get(), input));
}

};

namespace WebCore {

SkiaImageFilterBuilder::SkiaImageFilterBuilder()
    : m_context(0)
{
}

SkiaImageFilterBuilder::SkiaImageFilterBuilder(GraphicsContext* context)
    : m_context(context)
{
}

SkiaImageFilterBuilder::~SkiaImageFilterBuilder()
{
}

PassRefPtr<SkImageFilter> SkiaImageFilterBuilder::build(FilterEffect* effect, ColorSpace colorSpace, bool destinationRequiresValidPreMultipliedPixels)
{
    if (!effect)
        return nullptr;

    bool requiresPMColorValidation = effect->mayProduceInvalidPreMultipliedPixels() && destinationRequiresValidPreMultipliedPixels;
    FilterHashKey key(effect, static_cast<int>(colorSpace) | (requiresPMColorValidation ? static_cast<int>(PMColorValidationFlag) : 0));
    FilterBuilderHashMap::iterator it = m_map.find(key);
    if (it != m_map.end()) {
        return it->value;
    } else {
        // Note that we may still need the color transform even if the filter is null
        RefPtr<SkImageFilter> origFilter = requiresPMColorValidation ? effect->createImageFilter(this) : effect->createImageFilterWithoutValidation(this);
        RefPtr<SkImageFilter> filter = transformColorSpace(origFilter.get(), effect->operatingColorSpace(), colorSpace);
        m_map.set(key, filter);
        return filter.release();
    }
}

PassRefPtr<SkImageFilter> SkiaImageFilterBuilder::transformColorSpace(
    SkImageFilter* input, ColorSpace srcColorSpace, ColorSpace dstColorSpace) {

    RefPtr<SkColorFilter> colorFilter = ImageBuffer::createColorSpaceFilter(srcColorSpace, dstColorSpace);
    if (!colorFilter)
        return input;

    return adoptRef(SkColorFilterImageFilter::Create(colorFilter.get(), input));
}

bool SkiaImageFilterBuilder::buildFilterOperations(const FilterOperations& operations, blink::WebFilterOperations* filters)
{
    if (!filters)
        return false;

    ColorSpace currentColorSpace = ColorSpaceDeviceRGB;

    RefPtr<SkImageFilter> noopFilter;
    SkScalar matrix[20];
    memset(matrix, 0, 20 * sizeof(SkScalar));
    matrix[0] = matrix[6] = matrix[12] = matrix[18] = 1.f;
    noopFilter = createMatrixImageFilter(matrix, 0);

    for (size_t i = 0; i < operations.size(); ++i) {
        const FilterOperation& op = *operations.at(i);
        switch (op.type()) {
        case FilterOperation::REFERENCE: {
            RefPtr<SkImageFilter> filter;
            ReferenceFilter* referenceFilter = toReferenceFilterOperation(op).filter();
            if (referenceFilter && referenceFilter->lastEffect()) {
                FilterEffect* filterEffect = referenceFilter->lastEffect();
                // Link SourceGraphic to a noop filter that serves as a placholder for
                // the previous filter in the chain. We don't know what color space the
                // interior nodes will request, so we have to populate the map with both
                // options. (Only one of these will actually have a color transform on it.)
                FilterHashKey deviceKey(referenceFilter->sourceGraphic(), ColorSpaceDeviceRGB);
                FilterHashKey linearKey(referenceFilter->sourceGraphic(), ColorSpaceLinearRGB);
                FilterHashKey deviceKeyWithValidation(referenceFilter->sourceGraphic(), static_cast<int>(ColorSpaceDeviceRGB) | static_cast<int>(PMColorValidationFlag));
                FilterHashKey linearKeyWithValidation(referenceFilter->sourceGraphic(), static_cast<int>(ColorSpaceLinearRGB) | static_cast<int>(PMColorValidationFlag));
                RefPtr<SkImageFilter> deviceFilter = transformColorSpace(noopFilter.get(), currentColorSpace, ColorSpaceDeviceRGB);
                RefPtr<SkImageFilter> linearFilter = transformColorSpace(noopFilter.get(), currentColorSpace, ColorSpaceLinearRGB);
                m_map.set(deviceKey, deviceFilter);
                m_map.set(linearKey, linearFilter);
                // The PM color validated filters might be requested, but the SourceGraphic inputs are always valid,
                // so the filters are the same in this case, there's no extra step necessary.
                m_map.set(deviceKeyWithValidation, deviceFilter);
                m_map.set(linearKeyWithValidation, linearFilter);

                currentColorSpace = filterEffect->operatingColorSpace();
                filter = SkiaImageFilterBuilder::build(filterEffect, currentColorSpace);

                // We might have no reference to the SourceGraphic's Skia filter now, so make
                // sure we don't keep it in the map anymore.
                m_map.remove(deviceKey);
                m_map.remove(linearKey);
                m_map.remove(deviceKeyWithValidation);
                m_map.remove(linearKeyWithValidation);
                filters->appendReferenceFilter(filter.get());
            }
            break;
        }
        case FilterOperation::GRAYSCALE:
        case FilterOperation::SEPIA:
        case FilterOperation::SATURATE:
        case FilterOperation::HUE_ROTATE: {
            float amount = toBasicColorMatrixFilterOperation(op).amount();
            switch (op.type()) {
            case FilterOperation::GRAYSCALE:
                filters->appendGrayscaleFilter(amount);
                break;
            case FilterOperation::SEPIA:
                filters->appendSepiaFilter(amount);
                break;
            case FilterOperation::SATURATE:
                filters->appendSaturateFilter(amount);
                break;
            case FilterOperation::HUE_ROTATE:
                filters->appendHueRotateFilter(amount);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::INVERT:
        case FilterOperation::OPACITY:
        case FilterOperation::BRIGHTNESS:
        case FilterOperation::CONTRAST: {
            float amount = toBasicComponentTransferFilterOperation(op).amount();
            switch (op.type()) {
            case FilterOperation::INVERT:
                filters->appendInvertFilter(amount);
                break;
            case FilterOperation::OPACITY:
                filters->appendOpacityFilter(amount);
                break;
            case FilterOperation::BRIGHTNESS:
                filters->appendBrightnessFilter(amount);
                break;
            case FilterOperation::CONTRAST:
                filters->appendContrastFilter(amount);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            break;
        }
        case FilterOperation::BLUR: {
            float pixelRadius = toBlurFilterOperation(op).stdDeviation().getFloatValue();
            filters->appendBlurFilter(pixelRadius);
            break;
        }
        case FilterOperation::DROP_SHADOW: {
            const DropShadowFilterOperation& drop = toDropShadowFilterOperation(op);
            filters->appendDropShadowFilter(blink::WebPoint(drop.x(), drop.y()), drop.stdDeviation(), drop.color().rgb());
            break;
        }
        case FilterOperation::VALIDATED_CUSTOM:
        case FilterOperation::CUSTOM:
            return false; // Not supported.
        case FilterOperation::NONE:
            break;
        }
    }
    if (currentColorSpace != ColorSpaceDeviceRGB) {
        // Transform to device color space at the end of processing, if required
        RefPtr<SkImageFilter> filter;
        filter = transformColorSpace(noopFilter.get(), currentColorSpace, ColorSpaceDeviceRGB);
        if (filter != noopFilter)
            filters->appendReferenceFilter(filter.get());
    }
    return true;
}

PassRefPtr<SkImageFilter> SkiaImageFilterBuilder::buildTransform(const AffineTransform& transform, SkImageFilter* input)
{
    return adoptRef(SkMatrixImageFilter::Create(affineTransformToSkMatrix(transform), SkPaint::kHigh_FilterLevel, input));
}

};
