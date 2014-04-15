// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaValues.h"

#include "core/css/MediaValuesCached.h"
#include "core/css/MediaValuesDynamic.h"
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

Document* MediaValues::getExecutingDocument(Document& document)
{
    Document* executingDocument = document.importsController() ? document.importsController()->master() : &document;
    ASSERT(executingDocument);
    ASSERT(executingDocument->renderer());
    return executingDocument;
}

int MediaValues::calculateViewportWidth(LocalFrame* frame, RenderStyle* style) const
{
    ASSERT(frame && frame->view() && style);
    int viewportWidth = frame->view()->layoutSize(IncludeScrollbars).width();
    return adjustForAbsoluteZoom(viewportWidth, style);
}

int MediaValues::calculateViewportHeight(LocalFrame* frame, RenderStyle* style) const
{
    ASSERT(frame && frame->view() && style);
    int viewportHeight = frame->view()->layoutSize(IncludeScrollbars).height();
    return adjustForAbsoluteZoom(viewportHeight, style);
}

int MediaValues::calculateDeviceWidth(LocalFrame* frame) const
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    int deviceWidth = static_cast<int>(screenRect(frame->view()).width());
    if (frame->settings()->reportScreenSizeInPhysicalPixelsQuirk())
        deviceWidth = lroundf(deviceWidth * frame->host()->deviceScaleFactor());
    return deviceWidth;
}

int MediaValues::calculateDeviceHeight(LocalFrame* frame) const
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    int deviceHeight = static_cast<int>(screenRect(frame->view()).height());
    if (frame->settings()->reportScreenSizeInPhysicalPixelsQuirk())
        deviceHeight = lroundf(deviceHeight * frame->host()->deviceScaleFactor());
    return deviceHeight;
}

bool MediaValues::calculateStrictMode(LocalFrame* frame) const
{
    ASSERT(frame && frame->document());
    return !frame->document()->inQuirksMode();
}

float MediaValues::calculateDevicePixelRatio(LocalFrame* frame) const
{
    return frame->devicePixelRatio();
}

int MediaValues::calculateColorBitsPerComponent(LocalFrame* frame) const
{
    ASSERT(frame && frame->page() && frame->page()->mainFrame());
    if (screenIsMonochrome(frame->page()->mainFrame()->view()))
        return 0;
    return screenDepthPerComponent(frame->view());
}

int MediaValues::calculateMonochromeBitsPerComponent(LocalFrame* frame) const
{
    ASSERT(frame && frame->page() && frame->page()->mainFrame());
    if (screenIsMonochrome(frame->page()->mainFrame()->view()))
        return screenDepthPerComponent(frame->view());
    return 0;
}

int MediaValues::calculateDefaultFontSize(RenderStyle* style) const
{
    return style->fontDescription().specifiedSize();
}

int MediaValues::calculateComputedFontSize(RenderStyle* style) const
{
    return style->fontDescription().computedSize();
}

bool MediaValues::calculateHasXHeight(RenderStyle* style) const
{
    return style->fontMetrics().hasXHeight();
}

double MediaValues::calculateXHeight(RenderStyle* style) const
{
    return style->fontMetrics().xHeight();
}

double MediaValues::calculateZeroWidth(RenderStyle* style) const
{
    return style->fontMetrics().zeroWidth();
}

bool MediaValues::calculateScanMediaType(LocalFrame* frame) const
{
    ASSERT(frame && frame->view());
    // Scan only applies to 'tv' media.
    return equalIgnoringCase(frame->view()->mediaType(), "tv");
}

bool MediaValues::calculateScreenMediaType(LocalFrame* frame) const
{
    ASSERT(frame && frame->view());
    return equalIgnoringCase(frame->view()->mediaType(), "screen");
}

bool MediaValues::calculatePrintMediaType(LocalFrame* frame) const
{
    ASSERT(frame && frame->view());
    return equalIgnoringCase(frame->view()->mediaType(), "print");
}

bool MediaValues::calculateThreeDEnabled(LocalFrame* frame) const
{
    ASSERT(frame && frame->contentRenderer() && frame->contentRenderer()->compositor());
    bool threeDEnabled = false;
    if (RenderView* view = frame->contentRenderer())
        threeDEnabled = view->compositor()->canRender3DTransforms();
    return threeDEnabled;
}

MediaValues::PointerDeviceType MediaValues::calculateLeastCapablePrimaryPointerDeviceType(LocalFrame* frame) const
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

} // namespace
