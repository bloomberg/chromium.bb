/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
#include "platform/graphics/filters/FEBlend.h"

#include "SkBitmapSource.h"
#include "SkXfermodeImageFilter.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/cpu/arm/filters/FEBlendNEON.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/skia/NativeImageSkia.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/text/TextStream.h"
#include "wtf/Uint8ClampedArray.h"

typedef unsigned char (*BlendType)(unsigned char colorA, unsigned char colorB, unsigned char alphaA, unsigned char alphaB);

namespace blink {

FEBlend::FEBlend(Filter* filter, BlendModeType mode)
    : FilterEffect(filter)
    , m_mode(mode)
{
}

PassRefPtr<FEBlend> FEBlend::create(Filter* filter, BlendModeType mode)
{
    return adoptRef(new FEBlend(filter, mode));
}

BlendModeType FEBlend::blendMode() const
{
    return m_mode;
}

bool FEBlend::setBlendMode(BlendModeType mode)
{
    if (m_mode == mode)
        return false;
    m_mode = mode;
    return true;
}

static WebBlendMode toWebBlendMode(BlendModeType mode)
{
    switch (mode) {
    case FEBLEND_MODE_NORMAL:
        return WebBlendModeNormal;
    case FEBLEND_MODE_MULTIPLY:
        return WebBlendModeMultiply;
    case FEBLEND_MODE_SCREEN:
        return WebBlendModeScreen;
    case FEBLEND_MODE_DARKEN:
        return WebBlendModeDarken;
    case FEBLEND_MODE_LIGHTEN:
        return WebBlendModeLighten;
    default:
        ASSERT_NOT_REACHED();
        return WebBlendModeNormal;
    }
}

void FEBlend::applySoftware()
{
    FilterEffect* in = inputEffect(0);
    FilterEffect* in2 = inputEffect(1);

    ASSERT(m_mode > FEBLEND_MODE_UNKNOWN && m_mode <= FEBLEND_MODE_LIGHTEN);

#if HAVE(ARM_NEON_INTRINSICS)
    Uint8ClampedArray* dstPixelArray = createPremultipliedImageResult();
    if (!dstPixelArray)
        return;

    IntRect effectADrawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    RefPtr<Uint8ClampedArray> srcPixelArrayA = in->asPremultipliedImage(effectADrawingRect);

    IntRect effectBDrawingRect = requestedRegionOfInputImageData(in2->absolutePaintRect());
    RefPtr<Uint8ClampedArray> srcPixelArrayB = in2->asPremultipliedImage(effectBDrawingRect);

    unsigned pixelArrayLength = srcPixelArrayA->length();
    ASSERT(pixelArrayLength == srcPixelArrayB->length());

    if (pixelArrayLength >= 8) {
        platformApplyNEON(srcPixelArrayA->data(), srcPixelArrayB->data(), dstPixelArray->data(), pixelArrayLength);
    } else {
        // If there is just one pixel we expand it to two.
        ASSERT(pixelArrayLength > 0);
        uint32_t sourceA[2] = {0, 0};
        uint32_t sourceBAndDest[2] = {0, 0};

        sourceA[0] = reinterpret_cast<uint32_t*>(srcPixelArrayA->data())[0];
        sourceBAndDest[0] = reinterpret_cast<uint32_t*>(srcPixelArrayB->data())[0];
        platformApplyNEON(reinterpret_cast<uint8_t*>(sourceA), reinterpret_cast<uint8_t*>(sourceBAndDest), reinterpret_cast<uint8_t*>(sourceBAndDest), 8);
        reinterpret_cast<uint32_t*>(dstPixelArray->data())[0] = sourceBAndDest[0];
    }
#else
    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;
    GraphicsContext* filterContext = resultImage->context();

    ImageBuffer* imageBuffer = in->asImageBuffer();
    ImageBuffer* imageBuffer2 = in2->asImageBuffer();
    ASSERT(imageBuffer);
    ASSERT(imageBuffer2);

    WebBlendMode blendMode = toWebBlendMode(m_mode);
    filterContext->drawImageBuffer(imageBuffer2, drawingRegionOfInputImage(in2->absolutePaintRect()));
    filterContext->drawImageBuffer(imageBuffer, drawingRegionOfInputImage(in->absolutePaintRect()), 0, CompositeSourceOver, blendMode);
#endif
}

PassRefPtr<SkImageFilter> FEBlend::createImageFilter(SkiaImageFilterBuilder* builder)
{
    RefPtr<SkImageFilter> foreground(builder->build(inputEffect(0), operatingColorSpace()));
    RefPtr<SkImageFilter> background(builder->build(inputEffect(1), operatingColorSpace()));
    RefPtr<SkXfermode> mode(WebCoreCompositeToSkiaComposite(CompositeSourceOver, toWebBlendMode(m_mode)));
    SkImageFilter::CropRect cropRect = getCropRect(builder->cropOffset());
    return adoptRef(SkXfermodeImageFilter::Create(mode.get(), background.get(), foreground.get(), &cropRect));
}

static TextStream& operator<<(TextStream& ts, const BlendModeType& type)
{
    switch (type) {
    case FEBLEND_MODE_UNKNOWN:
        ts << "unknown";
        break;
    case FEBLEND_MODE_NORMAL:
        ts << "normal";
        break;
    case FEBLEND_MODE_MULTIPLY:
        ts << "multiply";
        break;
    case FEBLEND_MODE_SCREEN:
        ts << "screen";
        break;
    case FEBLEND_MODE_DARKEN:
        ts << "darken";
        break;
    case FEBLEND_MODE_LIGHTEN:
        ts << "lighten";
        break;
    }
    return ts;
}

TextStream& FEBlend::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feBlend";
    FilterEffect::externalRepresentation(ts);
    ts << " mode=\"" << m_mode << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    inputEffect(1)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace blink
