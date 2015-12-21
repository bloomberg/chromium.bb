// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesCached.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutObject.h"

namespace blink {

PassRefPtrWillBeRawPtr<MediaValues> MediaValuesCached::create()
{
    return adoptRefWillBeNoop(new MediaValuesCached());
}

PassRefPtrWillBeRawPtr<MediaValues> MediaValuesCached::create(MediaValuesCachedData& data)
{
    return adoptRefWillBeNoop(new MediaValuesCached(data));
}

PassRefPtrWillBeRawPtr<MediaValues> MediaValuesCached::create(Document& document)
{
    return MediaValuesCached::create(frameFrom(document));
}

PassRefPtrWillBeRawPtr<MediaValues> MediaValuesCached::create(LocalFrame* frame)
{
    // FIXME - Added an assert here so we can better understand when a frame is present without its view().
    ASSERT(!frame || frame->view());
    if (!frame || !frame->view())
        return adoptRefWillBeNoop(new MediaValuesCached());
    ASSERT(frame->document() && frame->document()->layoutView());
    return adoptRefWillBeNoop(new MediaValuesCached(frame));
}

MediaValuesCached::MediaValuesCached()
{
}

MediaValuesCached::MediaValuesCached(LocalFrame* frame)
{
    ASSERT(isMainThread());
    ASSERT(frame);
    // In case that frame is missing (e.g. for images that their document does not have a frame)
    // We simply leave the MediaValues object with the default MediaValuesCachedData values.
    m_data.viewportWidth = calculateViewportWidth(frame);
    m_data.viewportHeight = calculateViewportHeight(frame);
    m_data.deviceWidth = calculateDeviceWidth(frame);
    m_data.deviceHeight = calculateDeviceHeight(frame);
    m_data.devicePixelRatio = calculateDevicePixelRatio(frame);
    m_data.colorBitsPerComponent = calculateColorBitsPerComponent(frame);
    m_data.monochromeBitsPerComponent = calculateMonochromeBitsPerComponent(frame);
    m_data.primaryPointerType = calculatePrimaryPointerType(frame);
    m_data.availablePointerTypes = calculateAvailablePointerTypes(frame);
    m_data.primaryHoverType = calculatePrimaryHoverType(frame);
    m_data.availableHoverTypes = calculateAvailableHoverTypes(frame);
    m_data.defaultFontSize = calculateDefaultFontSize(frame);
    m_data.threeDEnabled = calculateThreeDEnabled(frame);
    m_data.strictMode = calculateStrictMode(frame);
    m_data.displayMode = calculateDisplayMode(frame);
    const String mediaType = calculateMediaType(frame);
    if (!mediaType.isEmpty())
        m_data.mediaType = mediaType.isolatedCopy();
}

MediaValuesCached::MediaValuesCached(const MediaValuesCachedData& data)
    : m_data(data)
{
}

PassRefPtrWillBeRawPtr<MediaValues> MediaValuesCached::copy() const
{
    return adoptRefWillBeNoop(new MediaValuesCached(m_data));
}

bool MediaValuesCached::computeLength(double value, CSSPrimitiveValue::UnitType type, int& result) const
{
    return MediaValues::computeLength(value, type, m_data.defaultFontSize, m_data.viewportWidth, m_data.viewportHeight, result);
}

bool MediaValuesCached::computeLength(double value, CSSPrimitiveValue::UnitType type, double& result) const
{
    return MediaValues::computeLength(value, type, m_data.defaultFontSize, m_data.viewportWidth, m_data.viewportHeight, result);
}

bool MediaValuesCached::isSafeToSendToAnotherThread() const
{
#if ENABLE(OILPAN)
    // Oilpan objects are safe to send to another thread as long as the thread
    // does not outlive the thread used for creation. MediaValues are
    // allocated on the main thread and may be passed to the parser thread,
    // so this should be safe.
    return true;
#else
    return hasOneRef();
#endif
}

double MediaValuesCached::viewportWidth() const
{
    return m_data.viewportWidth;
}

double MediaValuesCached::viewportHeight() const
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

PointerType MediaValuesCached::primaryPointerType() const
{
    return m_data.primaryPointerType;
}

int MediaValuesCached::availablePointerTypes() const
{
    return m_data.availablePointerTypes;
}

HoverType MediaValuesCached::primaryHoverType() const
{
    return m_data.primaryHoverType;
}

int MediaValuesCached::availableHoverTypes() const
{
    return m_data.availableHoverTypes;
}

bool MediaValuesCached::threeDEnabled() const
{
    return m_data.threeDEnabled;
}

bool MediaValuesCached::strictMode() const
{
    return m_data.strictMode;
}

const String MediaValuesCached::mediaType() const
{
    return m_data.mediaType;
}

WebDisplayMode MediaValuesCached::displayMode() const
{
    return m_data.displayMode;
}

Document* MediaValuesCached::document() const
{
    return nullptr;
}

bool MediaValuesCached::hasValues() const
{
    return true;
}

void MediaValuesCached::setViewportWidth(double width)
{
    m_data.viewportWidth = width;
}

void MediaValuesCached::setViewportHeight(double height)
{
    m_data.viewportHeight = height;
}

} // namespace
