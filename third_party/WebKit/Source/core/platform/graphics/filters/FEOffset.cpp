/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/platform/graphics/filters/FEOffset.h"

#include "SkFlattenableBuffers.h"
#include "SkImageFilter.h"

#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/filters/Filter.h"
#include "core/platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "core/platform/text/TextStream.h"
#include "core/rendering/RenderTreeAsText.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace WebCore {

class OffsetImageFilter : public SkImageFilter {
public:
    OffsetImageFilter(SkScalar dx, SkScalar dy, SkImageFilter* input, const SkIRect* cropRect = 0) : SkImageFilter(input, cropRect), m_dx(dx), m_dy(dy)
    {
    }

    virtual bool onFilterImage(Proxy* proxy, const SkBitmap& src, const SkMatrix& ctm, SkBitmap* dst, SkIPoint* offset)
    {
        SkBitmap source = src;
        SkImageFilter* input = getInput(0);
        SkIPoint srcOffset = SkIPoint::Make(0, 0);
        if (input && !input->filterImage(proxy, src, ctm, &source, &srcOffset))
            return false;

        SkIRect bounds;
        source.getBounds(&bounds);

        if (!applyCropRect(&bounds, ctm)) {
            return false;
        }

        SkAutoTUnref<SkBaseDevice> device(proxy->createDevice(bounds.width(), bounds.height()));
        SkCanvas canvas(device);
        SkPaint paint;
        paint.setXfermodeMode(SkXfermode::kSrc_Mode);
        canvas.drawBitmap(source, m_dx - bounds.left(), m_dy - bounds.top(), &paint);
        *dst = device->accessBitmap(false);
        offset->fX += bounds.left();
        offset->fY += bounds.top();
        return true;
    }

    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(OffsetImageFilter)

protected:
    explicit OffsetImageFilter(SkFlattenableReadBuffer& buffer)
        : SkImageFilter(buffer)
    {
        m_dx = buffer.readScalar();
        m_dy = buffer.readScalar();
    }

    virtual void flatten(SkFlattenableWriteBuffer& buffer) const
    {
        this->SkImageFilter::flatten(buffer);
        buffer.writeScalar(m_dx);
        buffer.writeScalar(m_dy);
    }

private:
    SkScalar m_dx, m_dy;
};

FEOffset::FEOffset(Filter* filter, float dx, float dy)
    : FilterEffect(filter)
    , m_dx(dx)
    , m_dy(dy)
{
}

PassRefPtr<FEOffset> FEOffset::create(Filter* filter, float dx, float dy)
{
    return adoptRef(new FEOffset(filter, dx, dy));
}

float FEOffset::dx() const
{
    return m_dx;
}

void FEOffset::setDx(float dx)
{
    m_dx = dx;
}

float FEOffset::dy() const
{
    return m_dy;
}

void FEOffset::setDy(float dy)
{
    m_dy = dy;
}

void FEOffset::determineAbsolutePaintRect()
{
    FloatRect paintRect = inputEffect(0)->absolutePaintRect();
    Filter* filter = this->filter();
    paintRect.move(filter->applyHorizontalScale(m_dx), filter->applyVerticalScale(m_dy));
    if (clipsToBounds())
        paintRect.intersect(maxEffectRect());
    else
        paintRect.unite(maxEffectRect());
    setAbsolutePaintRect(enclosingIntRect(paintRect));
}

FloatRect FEOffset::mapRect(const FloatRect& rect, bool forward)
{
    FloatRect result = rect;
    if (forward)
        result.move(filter()->applyHorizontalScale(m_dx), filter()->applyHorizontalScale(m_dy));
    else
        result.move(-filter()->applyHorizontalScale(m_dx), -filter()->applyHorizontalScale(m_dy));
    return result;
}

void FEOffset::applySoftware()
{
    FilterEffect* in = inputEffect(0);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    setIsAlphaImage(in->isAlphaImage());

    FloatRect drawingRegion = drawingRegionOfInputImage(in->absolutePaintRect());
    Filter* filter = this->filter();
    drawingRegion.move(filter->applyHorizontalScale(m_dx), filter->applyVerticalScale(m_dy));
    resultImage->context()->drawImageBuffer(in->asImageBuffer(), drawingRegion);
}

PassRefPtr<SkImageFilter> FEOffset::createImageFilter(SkiaImageFilterBuilder* builder)
{
    RefPtr<SkImageFilter> input(builder->build(inputEffect(0), operatingColorSpace()));
    Filter* filter = this->filter();
    SkIRect cropRect = getCropRect(builder->cropOffset());
    return adoptRef(new OffsetImageFilter(SkFloatToScalar(filter->applyHorizontalScale(m_dx)), SkFloatToScalar(filter->applyVerticalScale(m_dy)), input.get(), &cropRect));
}

TextStream& FEOffset::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feOffset";
    FilterEffect::externalRepresentation(ts);
    ts << " dx=\"" << dx() << "\" dy=\"" << dy() << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore
