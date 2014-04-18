// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaValuesDynamic.h"

#include "core/css/CSSHelper.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/dom/Document.h"

namespace WebCore {

PassRefPtr<MediaValues> MediaValuesDynamic::create(PassRefPtr<LocalFrame> frame)
{
    return adoptRef(new MediaValuesDynamic(frame));
}

MediaValuesDynamic::MediaValuesDynamic(PassRefPtr<LocalFrame> frame)
    : m_frame(frame)
{
    ASSERT(m_frame.get());
}

PassRefPtr<MediaValues> MediaValuesDynamic::copy() const
{
    return adoptRef(new MediaValuesDynamic(m_frame));
}

bool MediaValuesDynamic::computeLength(double value, unsigned short type, int& result) const
{
    LocalFrame* frame = m_frame.get();
    return MediaValues::computeLength(value,
        type,
        calculateDefaultFontSize(frame),
        calculateViewportWidth(frame),
        calculateViewportHeight(frame),
        result);
}

bool MediaValuesDynamic::isSafeToSendToAnotherThread() const
{
    return false;
}

int MediaValuesDynamic::viewportWidth() const
{
    return calculateViewportWidth(m_frame.get());
}

int MediaValuesDynamic::viewportHeight() const
{
    return calculateViewportHeight(m_frame.get());
}

int MediaValuesDynamic::deviceWidth() const
{
    return calculateDeviceWidth(m_frame.get());
}

int MediaValuesDynamic::deviceHeight() const
{
    return calculateDeviceHeight(m_frame.get());
}

float MediaValuesDynamic::devicePixelRatio() const
{
    return calculateDevicePixelRatio(m_frame.get());
}

int MediaValuesDynamic::colorBitsPerComponent() const
{
    return calculateColorBitsPerComponent(m_frame.get());
}

int MediaValuesDynamic::monochromeBitsPerComponent() const
{
    return calculateMonochromeBitsPerComponent(m_frame.get());
}

MediaValues::PointerDeviceType MediaValuesDynamic::pointer() const
{
    return calculateLeastCapablePrimaryPointerDeviceType(m_frame.get());
}

bool MediaValuesDynamic::threeDEnabled() const
{
    return calculateThreeDEnabled(m_frame.get());
}

bool MediaValuesDynamic::scanMediaType() const
{
    return calculateScanMediaType(m_frame.get());
}

bool MediaValuesDynamic::screenMediaType() const
{
    return calculateScreenMediaType(m_frame.get());
}

bool MediaValuesDynamic::printMediaType() const
{
    return calculatePrintMediaType(m_frame.get());
}

bool MediaValuesDynamic::strictMode() const
{
    return calculateStrictMode(m_frame.get());
}

Document* MediaValuesDynamic::document() const
{
    return m_frame->document();
}

bool MediaValuesDynamic::hasValues() const
{
    return(m_frame.get());
}

} // namespace
