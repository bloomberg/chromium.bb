/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/platform/graphics/filters/FilterEffect.h"

#include "core/platform/graphics/ImageBuffer.h"
#include "core/platform/graphics/filters/Filter.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "wtf/Uint8ClampedArray.h"

#if HAVE(ARM_NEON_INTRINSICS)
#include <arm_neon.h>
#endif

namespace WebCore {

FilterEffect::FilterEffect(Filter* filter)
    : m_alphaImage(false)
    , m_filter(filter)
    , m_hasX(false)
    , m_hasY(false)
    , m_hasWidth(false)
    , m_hasHeight(false)
    , m_clipsToBounds(true)
    , m_operatingColorSpace(ColorSpaceLinearRGB)
    , m_resultColorSpace(ColorSpaceDeviceRGB)
{
    ASSERT(m_filter);
}

FilterEffect::~FilterEffect()
{
}

inline bool isFilterSizeValid(IntRect rect)
{
    if (rect.width() < 0 || rect.width() > kMaxFilterSize
        || rect.height() < 0 || rect.height() > kMaxFilterSize)
        return false;
    return true;
}

void FilterEffect::determineAbsolutePaintRect()
{
    m_absolutePaintRect = IntRect();
    unsigned size = m_inputEffects.size();
    for (unsigned i = 0; i < size; ++i)
        m_absolutePaintRect.unite(m_inputEffects.at(i)->absolutePaintRect());

    // Filters in SVG clip to primitive subregion, while CSS doesn't.
    if (m_clipsToBounds)
        m_absolutePaintRect.intersect(enclosingIntRect(m_maxEffectRect));
    else
        m_absolutePaintRect.unite(enclosingIntRect(m_maxEffectRect));

}

FloatRect FilterEffect::mapRectRecursive(const FloatRect& rect)
{
    FloatRect result;
    if (m_inputEffects.size() > 0) {
        result = m_inputEffects.at(0)->mapRectRecursive(rect);
        for (unsigned i = 1; i < m_inputEffects.size(); ++i)
            result.unite(m_inputEffects.at(i)->mapRectRecursive(rect));
    } else
        result = rect;
    return mapRect(result);
}

FloatRect FilterEffect::getSourceRect(const FloatRect& destRect, const FloatRect& destClipRect)
{
    FloatRect sourceRect = mapRect(destRect, false);
    FloatRect sourceClipRect = mapRect(destClipRect, false);

    FloatRect boundaries = effectBoundaries();
    if (hasX())
        sourceClipRect.setX(boundaries.x());
    if (hasY())
        sourceClipRect.setY(boundaries.y());
    if (hasWidth())
        sourceClipRect.setWidth(boundaries.width());
    if (hasHeight())
        sourceClipRect.setHeight(boundaries.height());

    FloatRect result;
    if (m_inputEffects.size() > 0) {
        result = m_inputEffects.at(0)->getSourceRect(sourceRect, sourceClipRect);
        for (unsigned i = 1; i < m_inputEffects.size(); ++i)
            result.unite(m_inputEffects.at(i)->getSourceRect(sourceRect, sourceClipRect));
    } else {
        result = sourceRect;
        result.intersect(sourceClipRect);
    }
    return result;
}

IntRect FilterEffect::requestedRegionOfInputImageData(const IntRect& effectRect) const
{
    ASSERT(hasResult());
    IntPoint location = m_absolutePaintRect.location();
    location.moveBy(-effectRect.location());
    return IntRect(location, m_absolutePaintRect.size());
}

IntRect FilterEffect::drawingRegionOfInputImage(const IntRect& srcRect) const
{
    return IntRect(IntPoint(srcRect.x() - m_absolutePaintRect.x(),
                            srcRect.y() - m_absolutePaintRect.y()), srcRect.size());
}

FilterEffect* FilterEffect::inputEffect(unsigned number) const
{
    ASSERT_WITH_SECURITY_IMPLICATION(number < m_inputEffects.size());
    return m_inputEffects.at(number).get();
}

void FilterEffect::apply()
{
    if (hasResult())
        return;
    unsigned size = m_inputEffects.size();
    for (unsigned i = 0; i < size; ++i) {
        FilterEffect* in = m_inputEffects.at(i).get();
        in->apply();
        if (!in->hasResult())
            return;

        // Convert input results to the current effect's color space.
        transformResultColorSpace(in, i);
    }

    determineAbsolutePaintRect();
    setResultColorSpace(m_operatingColorSpace);

    if (!isFilterSizeValid(m_absolutePaintRect))
        return;

    if (requiresValidPreMultipliedPixels()) {
        for (unsigned i = 0; i < size; ++i)
            inputEffect(i)->correctFilterResultIfNeeded();
    }

    if (applySkia())
        return;

    applySoftware();
}

void FilterEffect::forceValidPreMultipliedPixels()
{
    // Must operate on pre-multiplied results; other formats cannot have invalid pixels.
    if (!m_premultipliedImageResult)
        return;

    Uint8ClampedArray* imageArray = m_premultipliedImageResult.get();
    unsigned char* pixelData = imageArray->data();
    int pixelArrayLength = imageArray->length();

    // We must have four bytes per pixel, and complete pixels
    ASSERT(!(pixelArrayLength % 4));

#if HAVE(ARM_NEON_INTRINSICS)
    if (pixelArrayLength >= 64) {
        unsigned char* lastPixel = pixelData + (pixelArrayLength & ~0x3f);
        do {
            // Increments pixelData by 64.
            uint8x16x4_t sixteenPixels = vld4q_u8(pixelData);
            sixteenPixels.val[0] = vminq_u8(sixteenPixels.val[0], sixteenPixels.val[3]);
            sixteenPixels.val[1] = vminq_u8(sixteenPixels.val[1], sixteenPixels.val[3]);
            sixteenPixels.val[2] = vminq_u8(sixteenPixels.val[2], sixteenPixels.val[3]);
            vst4q_u8(pixelData, sixteenPixels);
            pixelData += 64;
        } while (pixelData < lastPixel);

        pixelArrayLength &= 0x3f;
        if (!pixelArrayLength)
            return;
    }
#endif

    int numPixels = pixelArrayLength / 4;

    // Iterate over each pixel, checking alpha and adjusting color components if necessary
    while (--numPixels >= 0) {
        // Alpha is the 4th byte in a pixel
        unsigned char a = *(pixelData + 3);
        // Clamp each component to alpha, and increment the pixel location
        for (int i = 0; i < 3; ++i) {
            if (*pixelData > a)
                *pixelData = a;
            ++pixelData;
        }
        // Increment for alpha
        ++pixelData;
    }
}

void FilterEffect::clearResult()
{
    if (m_imageBufferResult)
        m_imageBufferResult.clear();
    if (m_unmultipliedImageResult)
        m_unmultipliedImageResult.clear();
    if (m_premultipliedImageResult)
        m_premultipliedImageResult.clear();
}

void FilterEffect::clearResultsRecursive()
{
    // Clear all results, regardless that the current effect has
    // a result. Can be used if an effect is in an erroneous state.
    if (hasResult())
        clearResult();

    unsigned size = m_inputEffects.size();
    for (unsigned i = 0; i < size; ++i)
        m_inputEffects.at(i).get()->clearResultsRecursive();
}

ImageBuffer* FilterEffect::asImageBuffer()
{
    if (!hasResult())
        return 0;
    if (m_imageBufferResult)
        return m_imageBufferResult.get();
    m_imageBufferResult = ImageBuffer::create(m_absolutePaintRect.size(), 1, m_filter->renderingMode());
    IntRect destinationRect(IntPoint(), m_absolutePaintRect.size());
    if (m_premultipliedImageResult)
        m_imageBufferResult->putByteArray(Premultiplied, m_premultipliedImageResult.get(), destinationRect.size(), destinationRect, IntPoint());
    else
        m_imageBufferResult->putByteArray(Unmultiplied, m_unmultipliedImageResult.get(), destinationRect.size(), destinationRect, IntPoint());
    return m_imageBufferResult.get();
}

PassRefPtr<Uint8ClampedArray> FilterEffect::asUnmultipliedImage(const IntRect& rect)
{
    ASSERT(isFilterSizeValid(rect));
    RefPtr<Uint8ClampedArray> imageData = Uint8ClampedArray::createUninitialized(rect.width() * rect.height() * 4);
    copyUnmultipliedImage(imageData.get(), rect);
    return imageData.release();
}

PassRefPtr<Uint8ClampedArray> FilterEffect::asPremultipliedImage(const IntRect& rect)
{
    ASSERT(isFilterSizeValid(rect));
    RefPtr<Uint8ClampedArray> imageData = Uint8ClampedArray::createUninitialized(rect.width() * rect.height() * 4);
    copyPremultipliedImage(imageData.get(), rect);
    return imageData.release();
}

inline void FilterEffect::copyImageBytes(Uint8ClampedArray* source, Uint8ClampedArray* destination, const IntRect& rect)
{
    // Initialize the destination to transparent black, if not entirely covered by the source.
    if (rect.x() < 0 || rect.y() < 0 || rect.maxX() > m_absolutePaintRect.width() || rect.maxY() > m_absolutePaintRect.height())
        memset(destination->data(), 0, destination->length());

    // Early return if the rect does not intersect with the source.
    if (rect.maxX() <= 0 || rect.maxY() <= 0 || rect.x() >= m_absolutePaintRect.width() || rect.y() >= m_absolutePaintRect.height())
        return;

    int xOrigin = rect.x();
    int xDest = 0;
    if (xOrigin < 0) {
        xDest = -xOrigin;
        xOrigin = 0;
    }
    int xEnd = rect.maxX();
    if (xEnd > m_absolutePaintRect.width())
        xEnd = m_absolutePaintRect.width();

    int yOrigin = rect.y();
    int yDest = 0;
    if (yOrigin < 0) {
        yDest = -yOrigin;
        yOrigin = 0;
    }
    int yEnd = rect.maxY();
    if (yEnd > m_absolutePaintRect.height())
        yEnd = m_absolutePaintRect.height();

    int size = (xEnd - xOrigin) * 4;
    int destinationScanline = rect.width() * 4;
    int sourceScanline = m_absolutePaintRect.width() * 4;
    unsigned char *destinationPixel = destination->data() + ((yDest * rect.width()) + xDest) * 4;
    unsigned char *sourcePixel = source->data() + ((yOrigin * m_absolutePaintRect.width()) + xOrigin) * 4;

    while (yOrigin < yEnd) {
        memcpy(destinationPixel, sourcePixel, size);
        destinationPixel += destinationScanline;
        sourcePixel += sourceScanline;
        ++yOrigin;
    }
}

void FilterEffect::copyUnmultipliedImage(Uint8ClampedArray* destination, const IntRect& rect)
{
    ASSERT(hasResult());

    if (!m_unmultipliedImageResult) {
        // We prefer a conversion from the image buffer.
        if (m_imageBufferResult)
            m_unmultipliedImageResult = m_imageBufferResult->getUnmultipliedImageData(IntRect(IntPoint(), m_absolutePaintRect.size()));
        else {
            ASSERT(isFilterSizeValid(m_absolutePaintRect));
            m_unmultipliedImageResult = Uint8ClampedArray::createUninitialized(m_absolutePaintRect.width() * m_absolutePaintRect.height() * 4);
            unsigned char* sourceComponent = m_premultipliedImageResult->data();
            unsigned char* destinationComponent = m_unmultipliedImageResult->data();
            unsigned char* end = sourceComponent + (m_absolutePaintRect.width() * m_absolutePaintRect.height() * 4);
            while (sourceComponent < end) {
                int alpha = sourceComponent[3];
                if (alpha) {
                    destinationComponent[0] = static_cast<int>(sourceComponent[0]) * 255 / alpha;
                    destinationComponent[1] = static_cast<int>(sourceComponent[1]) * 255 / alpha;
                    destinationComponent[2] = static_cast<int>(sourceComponent[2]) * 255 / alpha;
                } else {
                    destinationComponent[0] = 0;
                    destinationComponent[1] = 0;
                    destinationComponent[2] = 0;
                }
                destinationComponent[3] = alpha;
                sourceComponent += 4;
                destinationComponent += 4;
            }
        }
    }
    copyImageBytes(m_unmultipliedImageResult.get(), destination, rect);
}

void FilterEffect::copyPremultipliedImage(Uint8ClampedArray* destination, const IntRect& rect)
{
    ASSERT(hasResult());

    if (!m_premultipliedImageResult) {
        // We prefer a conversion from the image buffer.
        if (m_imageBufferResult)
            m_premultipliedImageResult = m_imageBufferResult->getPremultipliedImageData(IntRect(IntPoint(), m_absolutePaintRect.size()));
        else {
            ASSERT(isFilterSizeValid(m_absolutePaintRect));
            m_premultipliedImageResult = Uint8ClampedArray::createUninitialized(m_absolutePaintRect.width() * m_absolutePaintRect.height() * 4);
            unsigned char* sourceComponent = m_unmultipliedImageResult->data();
            unsigned char* destinationComponent = m_premultipliedImageResult->data();
            unsigned char* end = sourceComponent + (m_absolutePaintRect.width() * m_absolutePaintRect.height() * 4);
            while (sourceComponent < end) {
                int alpha = sourceComponent[3];
                destinationComponent[0] = static_cast<int>(sourceComponent[0]) * alpha / 255;
                destinationComponent[1] = static_cast<int>(sourceComponent[1]) * alpha / 255;
                destinationComponent[2] = static_cast<int>(sourceComponent[2]) * alpha / 255;
                destinationComponent[3] = alpha;
                sourceComponent += 4;
                destinationComponent += 4;
            }
        }
    }
    copyImageBytes(m_premultipliedImageResult.get(), destination, rect);
}

ImageBuffer* FilterEffect::createImageBufferResult()
{
    // Only one result type is allowed.
    ASSERT(!hasResult());
    if (m_absolutePaintRect.isEmpty())
        return 0;
    m_imageBufferResult = ImageBuffer::create(m_absolutePaintRect.size(), 1, m_filter->renderingMode());
    if (!m_imageBufferResult)
        return 0;
    ASSERT(m_imageBufferResult->context());
    return m_imageBufferResult.get();
}

Uint8ClampedArray* FilterEffect::createUnmultipliedImageResult()
{
    // Only one result type is allowed.
    ASSERT(!hasResult());
    ASSERT(isFilterSizeValid(m_absolutePaintRect));

    if (m_absolutePaintRect.isEmpty())
        return 0;
    m_unmultipliedImageResult = Uint8ClampedArray::createUninitialized(m_absolutePaintRect.width() * m_absolutePaintRect.height() * 4);
    return m_unmultipliedImageResult.get();
}

Uint8ClampedArray* FilterEffect::createPremultipliedImageResult()
{
    // Only one result type is allowed.
    ASSERT(!hasResult());
    ASSERT(isFilterSizeValid(m_absolutePaintRect));

    if (m_absolutePaintRect.isEmpty())
        return 0;
    m_premultipliedImageResult = Uint8ClampedArray::createUninitialized(m_absolutePaintRect.width() * m_absolutePaintRect.height() * 4);
    return m_premultipliedImageResult.get();
}

void FilterEffect::transformResultColorSpace(ColorSpace dstColorSpace)
{
    if (!hasResult() || dstColorSpace == m_resultColorSpace)
        return;

    // FIXME: We can avoid this potentially unnecessary ImageBuffer conversion by adding
    // color space transform support for the {pre,un}multiplied arrays.
    asImageBuffer()->transformColorSpace(m_resultColorSpace, dstColorSpace);

    m_resultColorSpace = dstColorSpace;

    if (m_unmultipliedImageResult)
        m_unmultipliedImageResult.clear();
    if (m_premultipliedImageResult)
        m_premultipliedImageResult.clear();
}

TextStream& FilterEffect::externalRepresentation(TextStream& ts, int) const
{
    // FIXME: We should dump the subRegions of the filter primitives here later. This isn't
    // possible at the moment, because we need more detailed informations from the target object.
    return ts;
}

FloatRect FilterEffect::determineFilterPrimitiveSubregion()
{
    ASSERT(filter());

    // FETile, FETurbulence, FEFlood don't have input effects, take the filter region as unite rect.
    FloatRect subregion;
    if (unsigned numberOfInputEffects = inputEffects().size()) {
        subregion = inputEffect(0)->determineFilterPrimitiveSubregion();
        for (unsigned i = 1; i < numberOfInputEffects; ++i)
            subregion.unite(inputEffect(i)->determineFilterPrimitiveSubregion());
    } else
        subregion = filter()->filterRegion();

    // After calling determineFilterPrimitiveSubregion on the target effect, reset the subregion again for <feTile>.
    if (filterEffectType() == FilterEffectTypeTile)
        subregion = filter()->filterRegion();

    subregion = mapRect(subregion);

    FloatRect boundaries = effectBoundaries();
    if (hasX())
        subregion.setX(boundaries.x());
    if (hasY())
        subregion.setY(boundaries.y());
    if (hasWidth())
        subregion.setWidth(boundaries.width());
    if (hasHeight())
        subregion.setHeight(boundaries.height());

    setFilterPrimitiveSubregion(subregion);

    FloatRect absoluteSubregion = filter()->absoluteTransform().mapRect(subregion);
    FloatSize filterResolution = filter()->filterResolution();
    absoluteSubregion.scale(filterResolution.width(), filterResolution.height());

    setMaxEffectRect(absoluteSubregion);
    return subregion;
}

PassRefPtr<SkImageFilter> FilterEffect::createImageFilter(SkiaImageFilterBuilder* builder)
{
    return 0;
}

SkIRect FilterEffect::getCropRect(const FloatSize& cropOffset) const
{
    SkIRect rect;
    FloatRect boundaries = effectBoundaries();
    FloatSize resolution = filter()->filterResolution();
    boundaries.scale(resolution.width(), resolution.height());
    rect.fLeft = hasX() ? static_cast<int>(boundaries.x()) + cropOffset.width() : 0;
    rect.fTop = hasY() ? static_cast<int>(boundaries.y()) + cropOffset.height() : 0;
    rect.fRight = hasWidth() ? rect.fLeft + static_cast<int>(boundaries.width()) : SK_MaxS32;
    rect.fBottom = hasHeight() ? rect.fTop + static_cast<int>(boundaries.height()) : SK_MaxS32;
    return rect;
}

} // namespace WebCore
