/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "core/platform/graphics/filters/FETile.h"

#include "SkFlattenableBuffers.h"
#include "SkImageFilter.h"

#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/Pattern.h"
#include "core/platform/graphics/filters/Filter.h"
#include "core/platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "core/platform/graphics/transforms/AffineTransform.h"
#include "core/platform/text/TextStream.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace WebCore {

class TileImageFilter : public SkImageFilter {
public:
    TileImageFilter(const SkRect& srcRect, const SkRect& dstRect, SkImageFilter* input)
        : SkImageFilter(input)
        , m_srcRect(srcRect)
        , m_dstRect(dstRect)
    {
    }

    virtual bool onFilterImage(Proxy* proxy, const SkBitmap& src, const SkMatrix& ctm, SkBitmap* dst, SkIPoint* offset)
    {
        SkBitmap source = src;
        SkImageFilter* input = getInput(0);
        SkIPoint localOffset = SkIPoint::Make(0, 0);
        if (input && !input->filterImage(proxy, src, ctm, &source, &localOffset))
            return false;

        if (!m_srcRect.width() || !m_srcRect.height() || !m_dstRect.width() || !m_dstRect.height())
            return false;

        SkIRect srcRect;
        m_srcRect.roundOut(&srcRect);
        SkBitmap subset;
        if (!source.extractSubset(&subset, srcRect))
            return false;

        SkAutoTUnref<SkDevice> device(proxy->createDevice(m_dstRect.width(), m_dstRect.height()));
        SkIRect bounds;
        source.getBounds(&bounds);
        SkCanvas canvas(device);
        SkPaint paint;
        paint.setXfermodeMode(SkXfermode::kSrc_Mode);

        SkAutoTUnref<SkShader> shader(SkShader::CreateBitmapShader(subset, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode));
        paint.setShader(shader);
        SkRect dstRect = m_dstRect;
        dstRect.offset(localOffset.fX, localOffset.fY);
        canvas.drawRect(dstRect, paint);
        *dst = device->accessBitmap(false);
        return true;
    }

    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(TileImageFilter)

protected:
    explicit TileImageFilter(SkFlattenableReadBuffer& buffer)
        : SkImageFilter(buffer)
    {
        buffer.readRect(&m_srcRect);
        buffer.readRect(&m_dstRect);
    }

    virtual void flatten(SkFlattenableWriteBuffer& buffer) const
    {
        this->SkImageFilter::flatten(buffer);
        buffer.writeRect(m_srcRect);
        buffer.writeRect(m_dstRect);
    }

private:
    SkRect m_srcRect;
    SkRect m_dstRect;
};

FETile::FETile(Filter* filter)
    : FilterEffect(filter)
{
}

PassRefPtr<FETile> FETile::create(Filter* filter)
{
    return adoptRef(new FETile(filter));
}

void FETile::applySoftware()
{
    FilterEffect* in = inputEffect(0);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    setIsAlphaImage(in->isAlphaImage());

    // Source input needs more attention. It has the size of the filterRegion but gives the
    // size of the cutted sourceImage back. This is part of the specification and optimization.
    FloatRect tileRect = in->maxEffectRect();
    FloatPoint inMaxEffectLocation = tileRect.location();
    FloatPoint maxEffectLocation = maxEffectRect().location();
    if (in->filterEffectType() == FilterEffectTypeSourceInput) {
        Filter* filter = this->filter();
        tileRect = filter->absoluteFilterRegion();
        tileRect.scale(filter->filterResolution().width(), filter->filterResolution().height());
    }

    OwnPtr<ImageBuffer> tileImage;
    if (!SVGRenderingContext::createImageBufferForPattern(tileRect, tileRect, tileImage, filter()->renderingMode()))
        return;

    GraphicsContext* tileImageContext = tileImage->context();
    tileImageContext->translate(-inMaxEffectLocation.x(), -inMaxEffectLocation.y());
    tileImageContext->drawImageBuffer(in->asImageBuffer(), in->absolutePaintRect().location());

    RefPtr<Pattern> pattern = Pattern::create(tileImage->copyImage(CopyBackingStore), true, true);

    AffineTransform patternTransform;
    patternTransform.translate(inMaxEffectLocation.x() - maxEffectLocation.x(), inMaxEffectLocation.y() - maxEffectLocation.y());
    pattern->setPatternSpaceTransform(patternTransform);
    GraphicsContext* filterContext = resultImage->context();
    filterContext->setFillPattern(pattern);
    filterContext->fillRect(FloatRect(FloatPoint(), absolutePaintRect().size()));
}

PassRefPtr<SkImageFilter> FETile::createImageFilter(SkiaImageFilterBuilder* builder)
{
    RefPtr<SkImageFilter> input(builder->build(inputEffect(0), operatingColorSpace()));
    FloatRect srcRect = inputEffect(0) ? inputEffect(0)->effectBoundaries() : FloatRect();
    return adoptRef(new TileImageFilter(srcRect, effectBoundaries(), input.get()));
}

TextStream& FETile::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feTile";
    FilterEffect::externalRepresentation(ts);
    ts << "]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);

    return ts;
}

} // namespace WebCore
