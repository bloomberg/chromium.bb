// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaValuesDynamic.h"

#include "core/css/CSSHelper.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/dom/Document.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleInheritedData.h"

namespace WebCore {

PassRefPtr<MediaValues> MediaValuesDynamic::create(PassRefPtr<LocalFrame> frame, PassRefPtr<RenderStyle> style)
{
    return adoptRef(new MediaValuesDynamic(frame, style));
}

PassRefPtr<MediaValues> MediaValuesDynamic::create(Document& document)
{
    Document* executingDocument = getExecutingDocument(document);
    return MediaValuesDynamic::create(executingDocument->frame(), executingDocument->renderer()->style());
}

MediaValuesDynamic::MediaValuesDynamic(PassRefPtr<LocalFrame> frame, PassRefPtr<RenderStyle> style)
    : m_style(style)
    , m_frame(frame)
{
}

PassRefPtr<MediaValues> MediaValuesDynamic::copy() const
{
    return adoptRef(new MediaValuesDynamic(m_frame, m_style));
}

bool MediaValuesDynamic::computeLength(double value, unsigned short type, int& result) const
{
    ASSERT(m_style.get());
    RefPtr<CSSPrimitiveValue> primitiveValue = CSSPrimitiveValue::create(value, (CSSPrimitiveValue::UnitTypes)type);
    result = primitiveValue->computeLength<int>(CSSToLengthConversionData(m_style.get(), m_style.get(), 0, 1.0 /* zoom */, true /* computingFontSize */));
    return true;
}

bool MediaValuesDynamic::isSafeToSendToAnotherThread() const
{
    return false;
}

int MediaValuesDynamic::viewportWidth() const
{
    ASSERT(m_style.get());
    ASSERT(m_frame.get());
    return calculateViewportWidth(m_frame.get(), m_style.get());
}

int MediaValuesDynamic::viewportHeight() const
{
    ASSERT(m_style.get());
    ASSERT(m_frame.get());
    return calculateViewportHeight(m_frame.get(), m_style.get());
}

int MediaValuesDynamic::deviceWidth() const
{
    ASSERT(m_frame.get());
    return calculateDeviceWidth(m_frame.get());
}

int MediaValuesDynamic::deviceHeight() const
{
    ASSERT(m_frame.get());
    return calculateDeviceHeight(m_frame.get());
}

float MediaValuesDynamic::devicePixelRatio() const
{
    ASSERT(m_frame.get());
    return calculateDevicePixelRatio(m_frame.get());
}

int MediaValuesDynamic::colorBitsPerComponent() const
{
    ASSERT(m_frame.get());
    return calculateColorBitsPerComponent(m_frame.get());
}

int MediaValuesDynamic::monochromeBitsPerComponent() const
{
    ASSERT(m_frame.get());
    return calculateMonochromeBitsPerComponent(m_frame.get());
}

MediaValues::PointerDeviceType MediaValuesDynamic::pointer() const
{
    ASSERT(m_frame.get());
    return calculateLeastCapablePrimaryPointerDeviceType(m_frame.get());
}

bool MediaValuesDynamic::threeDEnabled() const
{
    ASSERT(m_frame.get());
    return calculateThreeDEnabled(m_frame.get());
}

bool MediaValuesDynamic::scanMediaType() const
{
    ASSERT(m_frame.get());
    return calculateScanMediaType(m_frame.get());
}

bool MediaValuesDynamic::screenMediaType() const
{
    ASSERT(m_frame.get());
    return calculateScreenMediaType(m_frame.get());
}

bool MediaValuesDynamic::printMediaType() const
{
    ASSERT(m_frame.get());
    return calculatePrintMediaType(m_frame.get());
}

bool MediaValuesDynamic::strictMode() const
{
    ASSERT(m_frame.get());
    return calculateStrictMode(m_frame.get());
}

Document* MediaValuesDynamic::document() const
{
    ASSERT(m_frame.get());
    return m_frame->document();
}

bool MediaValuesDynamic::hasValues() const
{
    return(m_style.get() && m_frame.get());
}

} // namespace
