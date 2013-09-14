/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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

#include "core/platform/graphics/filters/FEFlood.h"

#include "SkFlattenableBuffers.h"
#include "SkImageFilter.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/filters/Filter.h"
#include "core/platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "core/platform/text/TextStream.h"
#include "core/rendering/RenderTreeAsText.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace {

class FloodImageFilter : public SkImageFilter {
public:
    FloodImageFilter(const SkColor& color, const SkIRect* cropRect)
        : SkImageFilter(0, 0, cropRect)
        , m_color(color)
    {
    }
    SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(FloodImageFilter)

    FloodImageFilter(SkFlattenableReadBuffer& buffer) : SkImageFilter(buffer)
    {
        m_color = buffer.readColor();
    }

    virtual void flatten(SkFlattenableWriteBuffer& buffer) const
    {
        this->SkImageFilter::flatten(buffer);
        buffer.writeColor(m_color);
    }

    virtual bool onFilterImage(Proxy* proxy, const SkBitmap& src, const SkMatrix& ctm, SkBitmap* result, SkIPoint* offset)
    {
        if (!src.width() || !src.height())
            return false;

        SkIRect bounds;
        src.getBounds(&bounds);
        if (!applyCropRect(&bounds, ctm)) {
            return false;
        }

        SkAutoTUnref<SkBaseDevice> device(proxy->createDevice(bounds.width(), bounds.height()));
        SkCanvas canvas(device.get());
        SkPaint paint;
        paint.setColor(m_color);
        canvas.drawRect(SkRect::MakeWH(src.width(), src.height()), paint);
        *result = device->accessBitmap(false);
        offset->fX += bounds.left();
        offset->fY += bounds.top();
        return true;
    }
private:
    SkColor m_color;
};

}; // unnamed namespace

namespace WebCore {

FEFlood::FEFlood(Filter* filter, const Color& floodColor, float floodOpacity)
    : FilterEffect(filter)
    , m_floodColor(floodColor)
    , m_floodOpacity(floodOpacity)
{
    FilterEffect::setOperatingColorSpace(ColorSpaceDeviceRGB);
    FilterEffect::setResultColorSpace(ColorSpaceDeviceRGB);
}

PassRefPtr<FEFlood> FEFlood::create(Filter* filter, const Color& floodColor, float floodOpacity)
{
    return adoptRef(new FEFlood(filter, floodColor, floodOpacity));
}

Color FEFlood::floodColor() const
{
    return m_floodColor;
}

bool FEFlood::setFloodColor(const Color& color)
{
    if (m_floodColor == color)
        return false;
    m_floodColor = color;
    return true;
}

float FEFlood::floodOpacity() const
{
    return m_floodOpacity;
}

bool FEFlood::setFloodOpacity(float floodOpacity)
{
    if (m_floodOpacity == floodOpacity)
        return false;
    m_floodOpacity = floodOpacity;
    return true;
}

void FEFlood::applySoftware()
{
    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    Color color = colorWithOverrideAlpha(floodColor().rgb(), floodOpacity());
    resultImage->context()->fillRect(FloatRect(FloatPoint(), absolutePaintRect().size()), color);
}

PassRefPtr<SkImageFilter> FEFlood::createImageFilter(SkiaImageFilterBuilder* builder)
{
    Color color = colorWithOverrideAlpha(floodColor().rgb(), floodOpacity());

    SkIRect rect = getCropRect(builder->cropOffset());
    return adoptRef(new FloodImageFilter(color.rgb(), &rect));
}

TextStream& FEFlood::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feFlood";
    FilterEffect::externalRepresentation(ts);
    ts << " flood-color=\"" << floodColor().nameForRenderTreeAsText() << "\" "
       << "flood-opacity=\"" << floodOpacity() << "\"]\n";
    return ts;
}

} // namespace WebCore
