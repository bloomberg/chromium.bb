// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaValuesCached.h"

#include "core/css/CSSHelper.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

PassRefPtr<MediaValues> MediaValuesCached::create(MediaValuesCachedData& data)
{
    return adoptRef(new MediaValuesCached(data));
}

PassRefPtr<MediaValues> MediaValuesCached::create(Document& document)
{
    Document* executingDocument = getExecutingDocument(document);
    return MediaValuesCached::create(executingDocument->frame(), executingDocument->renderer()->style());
}

PassRefPtr<MediaValues> MediaValuesCached::create(LocalFrame* frame, RenderStyle* style)
{
    return adoptRef(new MediaValuesCached(frame, style));
}

MediaValuesCached::MediaValuesCached(LocalFrame* frame, RenderStyle* style)
{
    ASSERT(frame && style);
    m_data.viewportWidth = calculateViewportWidth(frame, style);
    m_data.viewportHeight = calculateViewportHeight(frame, style);
    m_data.deviceWidth = calculateDeviceWidth(frame);
    m_data.deviceHeight = calculateDeviceHeight(frame);
    m_data.devicePixelRatio = calculateDevicePixelRatio(frame);
    m_data.colorBitsPerComponent = calculateColorBitsPerComponent(frame);
    m_data.monochromeBitsPerComponent = calculateMonochromeBitsPerComponent(frame);
    m_data.pointer = calculateLeastCapablePrimaryPointerDeviceType(frame);
    m_data.defaultFontSize = calculateDefaultFontSize(style);
    m_data.computedFontSize = calculateComputedFontSize(style);
    m_data.hasXHeight = calculateHasXHeight(style);
    m_data.xHeight = calculateXHeight(style);
    m_data.zeroWidth = calculateZeroWidth(style);
    m_data.threeDEnabled = calculateThreeDEnabled(frame);
    m_data.scanMediaType = calculateScanMediaType(frame);
    m_data.screenMediaType = calculateScreenMediaType(frame);
    m_data.printMediaType = calculatePrintMediaType(frame);
    m_data.strictMode = calculateStrictMode(frame);
}

MediaValuesCached::MediaValuesCached(const MediaValuesCachedData& data)
    : m_data(data)
{
}

PassRefPtr<MediaValues> MediaValuesCached::copy() const
{
    return adoptRef(new MediaValuesCached(m_data));
}

bool MediaValuesCached::computeLength(double value, unsigned short type, int& result) const
{
    // We're running in a background thread, so RenderStyle is not available.
    // Nevertheless, we can evaluate lengths using cached values.
    //
    // The logic in this function is duplicated from CSSPrimitiveValue::computeLengthDouble
    // because MediaValues::computeLength needs nearly identical logic, but we haven't found a way to make
    // CSSPrimitiveValue::computeLengthDouble more generic (to solve both cases) without hurting performance.

    // FIXME - Unite the logic here with CSSPrimitiveValue is a performant way.
    int factor = 0;
    switch (type) {
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_REMS:
        factor = m_data.defaultFontSize;
        break;
    case CSSPrimitiveValue::CSS_PX:
        factor = 1;
        break;
    case CSSPrimitiveValue::CSS_EXS:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // We really need to compute EX using fontMetrics for the original specifiedSize and not use
        // our actual constructed rendering font.
        if (m_data.hasXHeight)
            factor = m_data.xHeight;
        else
            factor = m_data.defaultFontSize / 2.0;
        break;
    case CSSPrimitiveValue::CSS_CHS:
        factor = m_data.zeroWidth;
        break;
    case CSSPrimitiveValue::CSS_VW:
        factor = m_data.viewportWidth / 100;
        break;
    case CSSPrimitiveValue::CSS_VH:
        factor = m_data.viewportHeight / 100;
        break;
    case CSSPrimitiveValue::CSS_VMIN:
        factor = std::min(m_data.viewportWidth, m_data.viewportHeight) / 100;
        break;
    case CSSPrimitiveValue::CSS_VMAX:
        factor = std::max(m_data.viewportWidth, m_data.viewportHeight) / 100;
        break;
    case CSSPrimitiveValue::CSS_CM:
        factor = cssPixelsPerCentimeter;
        break;
    case CSSPrimitiveValue::CSS_MM:
        factor = cssPixelsPerMillimeter;
        break;
    case CSSPrimitiveValue::CSS_IN:
        factor = cssPixelsPerInch;
        break;
    case CSSPrimitiveValue::CSS_PT:
        factor = cssPixelsPerPoint;
        break;
    case CSSPrimitiveValue::CSS_PC:
        factor = cssPixelsPerPica;
        break;
    default:
        return false;
    }

    ASSERT(factor > 0);
    result = roundForImpreciseConversion<int>(value*factor);
    return true;
}

bool MediaValuesCached::isSafeToSendToAnotherThread() const
{
    return hasOneRef();
}

int MediaValuesCached::viewportWidth() const
{
    return m_data.viewportWidth;
}

int MediaValuesCached::viewportHeight() const
{
    return m_data.viewportHeight;
}

int MediaValuesCached::deviceWidth() const
{
    return m_data.deviceWidth;
}

int MediaValuesCached::deviceHeight() const
{
    return m_data.deviceHeight;
}

float MediaValuesCached::devicePixelRatio() const
{
    return m_data.devicePixelRatio;
}

int MediaValuesCached::colorBitsPerComponent() const
{
    return m_data.colorBitsPerComponent;
}

int MediaValuesCached::monochromeBitsPerComponent() const
{
    return m_data.monochromeBitsPerComponent;
}

MediaValues::PointerDeviceType MediaValuesCached::pointer() const
{
    return m_data.pointer;
}

bool MediaValuesCached::threeDEnabled() const
{
    return m_data.threeDEnabled;
}

bool MediaValuesCached::scanMediaType() const
{
    return m_data.scanMediaType;
}

bool MediaValuesCached::screenMediaType() const
{
    return m_data.screenMediaType;
}

bool MediaValuesCached::printMediaType() const
{
    return m_data.printMediaType;
}

bool MediaValuesCached::strictMode() const
{
    return m_data.strictMode;
}

Document* MediaValuesCached::document() const
{
    return 0;
}

bool MediaValuesCached::hasValues() const
{
    return true;
}

} // namespace
