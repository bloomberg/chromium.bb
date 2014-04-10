// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaValues.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/page/Page.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/PlatformScreen.h"

namespace WebCore {

static int calculateViewportWidth(LocalFrame* frame, RenderStyle* style)
{
    ASSERT(frame && frame->view() && style);
    int viewportWidth = frame->view()->layoutSize(IncludeScrollbars).width();
    return adjustForAbsoluteZoom(viewportWidth, style);
}

static int calculateViewportHeight(LocalFrame* frame, RenderStyle* style)
{
    ASSERT(frame && frame->view() && style);
    int viewportHeight = frame->view()->layoutSize(IncludeScrollbars).height();
    return adjustForAbsoluteZoom(viewportHeight, style);
}

static int calculateDeviceWidth(LocalFrame* frame)
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    int deviceWidth = static_cast<int>(screenRect(frame->view()).width());
    if (frame->settings()->reportScreenSizeInPhysicalPixelsQuirk())
        deviceWidth = lroundf(deviceWidth * frame->host()->deviceScaleFactor());
    return deviceWidth;
}

static int calculateDeviceHeight(LocalFrame* frame)
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    int deviceHeight = static_cast<int>(screenRect(frame->view()).height());
    if (frame->settings()->reportScreenSizeInPhysicalPixelsQuirk())
        deviceHeight = lroundf(deviceHeight * frame->host()->deviceScaleFactor());
    return deviceHeight;
}

static bool calculateStrictMode(LocalFrame* frame)
{
    ASSERT(frame && frame->document());
    return !frame->document()->inQuirksMode();
}

static float calculateDevicePixelRatio(LocalFrame* frame)
{
    return frame->devicePixelRatio();
}

static int calculateColorBitsPerComponent(LocalFrame* frame)
{
    ASSERT(frame && frame->page() && frame->page()->mainFrame());
    if (screenIsMonochrome(frame->page()->mainFrame()->view()))
        return 0;
    return screenDepthPerComponent(frame->view());
}

static int calculateMonochromeBitsPerComponent(LocalFrame* frame)
{
    ASSERT(frame && frame->page() && frame->page()->mainFrame());
    if (screenIsMonochrome(frame->page()->mainFrame()->view()))
        return screenDepthPerComponent(frame->view());
    return 0;
}

static int calculateDefaultFontSize(RenderStyle* style)
{
    return style->fontDescription().specifiedSize();
}

static bool calculateScanMediaType(LocalFrame* frame)
{
    ASSERT(frame && frame->view());
    // Scan only applies to 'tv' media.
    return equalIgnoringCase(frame->view()->mediaType(), "tv");
}

static bool calculateScreenMediaType(LocalFrame* frame)
{
    ASSERT(frame && frame->view());
    return equalIgnoringCase(frame->view()->mediaType(), "screen");
}

static bool calculatePrintMediaType(LocalFrame* frame)
{
    ASSERT(frame && frame->view());
    return equalIgnoringCase(frame->view()->mediaType(), "print");
}

static bool calculateThreeDEnabled(LocalFrame* frame)
{
    ASSERT(frame && frame->contentRenderer() && frame->contentRenderer()->compositor());
    bool threeDEnabled = false;
    if (RenderView* view = frame->contentRenderer())
        threeDEnabled = view->compositor()->canRender3DTransforms();
    return threeDEnabled;
}

static MediaValues::PointerDeviceType calculateLeastCapablePrimaryPointerDeviceType(LocalFrame* frame)
{
    ASSERT(frame && frame->settings());
    if (frame->settings()->deviceSupportsTouch())
        return MediaValues::TouchPointer;

    // FIXME: We should also try to determine if we know we have a mouse.
    // When we do this, we'll also need to differentiate between known not to
    // have mouse or touch screen (NoPointer) and unknown (UnknownPointer).
    // We could also take into account other preferences like accessibility
    // settings to decide which of the available pointers should be considered
    // "primary".

    return MediaValues::UnknownPointer;
}

PassRefPtr<MediaValues> MediaValues::create(MediaValuesMode mode,
    int viewportWidth,
    int viewportHeight,
    int deviceWidth,
    int deviceHeight,
    float devicePixelRatio,
    int colorBitsPerComponent,
    int monochromeBitsPerComponent,
    PointerDeviceType pointer,
    int defaultFontSize,
    bool threeDEnabled,
    bool scanMediaType,
    bool screenMediaType,
    bool printMediaType,
    bool strictMode)
{
    ASSERT(mode == CachingMode);
    RefPtr<MediaValues> mediaValues = adoptRef(new MediaValues(0, nullptr, mode));
    mediaValues->m_viewportWidth = viewportWidth;
    mediaValues->m_viewportHeight = viewportHeight;
    mediaValues->m_deviceWidth = deviceWidth;
    mediaValues->m_deviceHeight = deviceHeight;
    mediaValues->m_devicePixelRatio = devicePixelRatio;
    mediaValues->m_colorBitsPerComponent = colorBitsPerComponent;
    mediaValues->m_monochromeBitsPerComponent = monochromeBitsPerComponent;
    mediaValues->m_pointer = pointer;
    mediaValues->m_defaultFontSize = defaultFontSize;
    mediaValues->m_threeDEnabled = threeDEnabled;
    mediaValues->m_scanMediaType = scanMediaType;
    mediaValues->m_screenMediaType = screenMediaType;
    mediaValues->m_printMediaType = printMediaType;
    mediaValues->m_strictMode = strictMode;

    return mediaValues;
}

PassRefPtr<MediaValues> MediaValues::create(LocalFrame* frame, RenderStyle* style, MediaValuesMode mode)
{
    ASSERT(frame && style);
    RefPtr<MediaValues> mediaValues;
    mediaValues = adoptRef(new MediaValues(frame, style, mode));
    if (mode == CachingMode) {
        mediaValues->m_viewportWidth = calculateViewportWidth(frame, style);
        mediaValues->m_viewportHeight = calculateViewportHeight(frame, style),
        mediaValues->m_deviceWidth = calculateDeviceWidth(frame),
        mediaValues->m_deviceHeight = calculateDeviceHeight(frame),
        mediaValues->m_devicePixelRatio = calculateDevicePixelRatio(frame),
        mediaValues->m_colorBitsPerComponent = calculateColorBitsPerComponent(frame),
        mediaValues->m_monochromeBitsPerComponent = calculateMonochromeBitsPerComponent(frame),
        mediaValues->m_pointer = calculateLeastCapablePrimaryPointerDeviceType(frame),
        mediaValues->m_defaultFontSize = calculateDefaultFontSize(style),
        mediaValues->m_threeDEnabled = calculateThreeDEnabled(frame),
        mediaValues->m_scanMediaType = calculateScanMediaType(frame),
        mediaValues->m_screenMediaType = calculateScreenMediaType(frame),
        mediaValues->m_printMediaType = calculatePrintMediaType(frame),
        mediaValues->m_strictMode = calculateStrictMode(frame);

        mediaValues->m_style.clear();
        mediaValues->m_frame = 0;
    }

    return mediaValues;
}

PassRefPtr<MediaValues> MediaValues::create(Document* document, MediaValuesMode mode)
{
    ASSERT(document);
    Document* executingDocument = document->importsController() ? document->importsController()->master() : document;
    ASSERT(executingDocument->frame());
    ASSERT(executingDocument->renderer());
    ASSERT(executingDocument->renderer()->style());
    LocalFrame* frame = executingDocument->frame();
    RenderStyle* style = executingDocument->renderer()->style();

    return MediaValues::create(frame, style, mode);
}

PassRefPtr<MediaValues> MediaValues::copy() const
{
    ASSERT(m_mode == CachingMode && !m_style.get() && !m_frame);
    RefPtr<MediaValues> mediaValues = adoptRef(new MediaValues(0, nullptr, m_mode));
    mediaValues->m_viewportWidth = m_viewportWidth;
    mediaValues->m_viewportHeight = m_viewportHeight;
    mediaValues->m_deviceWidth = m_deviceWidth;
    mediaValues->m_deviceHeight = m_deviceHeight;
    mediaValues->m_devicePixelRatio = m_devicePixelRatio;
    mediaValues->m_colorBitsPerComponent = m_colorBitsPerComponent;
    mediaValues->m_monochromeBitsPerComponent = m_monochromeBitsPerComponent;
    mediaValues->m_pointer = m_pointer;
    mediaValues->m_defaultFontSize = m_defaultFontSize;
    mediaValues->m_threeDEnabled = m_threeDEnabled;
    mediaValues->m_scanMediaType = m_scanMediaType;
    mediaValues->m_screenMediaType = m_screenMediaType;
    mediaValues->m_printMediaType = m_printMediaType;
    mediaValues->m_strictMode = m_strictMode;

    return mediaValues;
}

bool MediaValues::isSafeToSendToAnotherThread() const
{
    return (!m_frame && !m_style && m_mode == CachingMode && hasOneRef());
}

int MediaValues::viewportWidth() const
{
    if (m_mode == DynamicMode)
        return calculateViewportWidth(m_frame, m_style.get());
    return m_viewportWidth;
}

int MediaValues::viewportHeight() const
{
    if (m_mode == DynamicMode)
        return calculateViewportHeight(m_frame, m_style.get());
    return m_viewportHeight;
}

int MediaValues::deviceWidth() const
{
    if (m_mode == DynamicMode)
        return calculateDeviceWidth(m_frame);
    return m_deviceWidth;
}

int MediaValues::deviceHeight() const
{
    if (m_mode == DynamicMode)
        return calculateDeviceHeight(m_frame);
    return m_deviceHeight;
}

float MediaValues::devicePixelRatio() const
{
    if (m_mode == DynamicMode)
        return calculateDevicePixelRatio(m_frame);
    return m_devicePixelRatio;
}

int MediaValues::colorBitsPerComponent() const
{
    if (m_mode == DynamicMode)
        return calculateColorBitsPerComponent(m_frame);
    return m_colorBitsPerComponent;
}

int MediaValues::monochromeBitsPerComponent() const
{
    if (m_mode == DynamicMode)
        return calculateMonochromeBitsPerComponent(m_frame);
    return m_monochromeBitsPerComponent;
}

MediaValues::PointerDeviceType MediaValues::pointer() const
{
    if (m_mode == DynamicMode)
        return calculateLeastCapablePrimaryPointerDeviceType(m_frame);
    return m_pointer;
}

int MediaValues::defaultFontSize() const
{
    if (m_mode == DynamicMode)
        return calculateDefaultFontSize(m_style.get());
    return m_defaultFontSize;
}

bool MediaValues::threeDEnabled() const
{
    if (m_mode == DynamicMode)
        return calculateThreeDEnabled(m_frame);
    return m_threeDEnabled;
}

bool MediaValues::scanMediaType() const
{
    if (m_mode == DynamicMode)
        return calculateScanMediaType(m_frame);
    return m_scanMediaType;
}

bool MediaValues::screenMediaType() const
{
    if (m_mode == DynamicMode)
        return calculateScreenMediaType(m_frame);
    return m_screenMediaType;
}

bool MediaValues::printMediaType() const
{
    if (m_mode == DynamicMode)
        return calculatePrintMediaType(m_frame);
    return m_printMediaType;
}

bool MediaValues::strictMode() const
{
    if (m_mode == DynamicMode)
        return calculateStrictMode(m_frame);
    return m_strictMode;
}

Document* MediaValues::document() const
{
    if (!m_frame)
        return 0;
    return m_frame->document();
}

} // namespace
