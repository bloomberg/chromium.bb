// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValues.h"

#include "core/css/CSSHelper.h"
#include "core/css/MediaValuesCached.h"
#include "core/css/MediaValuesDynamic.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "public/platform/WebScreenInfo.h"

namespace blink {

PassRefPtrWillBeRawPtr<MediaValues> MediaValues::createDynamicIfFrameExists(LocalFrame* frame)
{
    if (frame)
        return MediaValuesDynamic::create(frame);
    return MediaValuesCached::create();
}

double MediaValues::calculateViewportWidth(LocalFrame* frame) const
{
    ASSERT(frame && frame->view() && frame->document());
    int viewportWidth = frame->view()->layoutSize(IncludeScrollbars).width();
    return adjustDoubleForAbsoluteZoom(viewportWidth, *frame->document()->layoutView());
}

double MediaValues::calculateViewportHeight(LocalFrame* frame) const
{
    ASSERT(frame && frame->view() && frame->document());
    int viewportHeight = frame->view()->layoutSize(IncludeScrollbars).height();
    return adjustDoubleForAbsoluteZoom(viewportHeight, *frame->document()->layoutView());
}

int MediaValues::calculateDeviceWidth(LocalFrame* frame) const
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    int deviceWidth = frame->host()->chromeClient().screenInfo().rect.width;
    if (frame->settings()->reportScreenSizeInPhysicalPixelsQuirk())
        deviceWidth = lroundf(deviceWidth * frame->host()->deviceScaleFactor());
    return deviceWidth;
}

int MediaValues::calculateDeviceHeight(LocalFrame* frame) const
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    int deviceHeight = frame->host()->chromeClient().screenInfo().rect.height;
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
    if (!frame->page()->mainFrame()->isLocalFrame()
        || frame->host()->chromeClient().screenInfo().isMonochrome)
        return 0;
    return frame->host()->chromeClient().screenInfo().depthPerComponent;
}

int MediaValues::calculateMonochromeBitsPerComponent(LocalFrame* frame) const
{
    ASSERT(frame && frame->page() && frame->page()->mainFrame());
    if (!frame->page()->mainFrame()->isLocalFrame()
        || !frame->host()->chromeClient().screenInfo().isMonochrome)
        return 0;
    return frame->host()->chromeClient().screenInfo().depthPerComponent;
}

int MediaValues::calculateDefaultFontSize(LocalFrame* frame) const
{
    return frame->host()->settings().defaultFontSize();
}

const String MediaValues::calculateMediaType(LocalFrame* frame) const
{
    ASSERT(frame);
    if (!frame->view())
        return emptyAtom;
    return frame->view()->mediaType();
}

WebDisplayMode MediaValues::calculateDisplayMode(LocalFrame* frame) const
{
    ASSERT(frame);
    WebDisplayMode mode = frame->host()->settings().displayModeOverride();

    if (mode != WebDisplayModeUndefined)
        return mode;

    if (!frame->view())
        return WebDisplayModeBrowser;

    return frame->view()->displayMode();
}

bool MediaValues::calculateThreeDEnabled(LocalFrame* frame) const
{
    ASSERT(frame && frame->contentLayoutObject() && frame->contentLayoutObject()->compositor());
    bool threeDEnabled = false;
    if (LayoutView* view = frame->contentLayoutObject())
        threeDEnabled = view->compositor()->hasAcceleratedCompositing();
    return threeDEnabled;
}

PointerType MediaValues::calculatePrimaryPointerType(LocalFrame* frame) const
{
    ASSERT(frame && frame->settings());
    return frame->settings()->primaryPointerType();
}

int MediaValues::calculateAvailablePointerTypes(LocalFrame* frame) const
{
    ASSERT(frame && frame->settings());
    return frame->settings()->availablePointerTypes();
}

HoverType MediaValues::calculatePrimaryHoverType(LocalFrame* frame) const
{
    ASSERT(frame && frame->settings());
    return frame->settings()->primaryHoverType();
}

int MediaValues::calculateAvailableHoverTypes(LocalFrame* frame) const
{
    ASSERT(frame && frame->settings());
    return frame->settings()->availableHoverTypes();
}

bool MediaValues::computeLengthImpl(double value, CSSPrimitiveValue::UnitType type, unsigned defaultFontSize, double viewportWidth, double viewportHeight, double& result)
{
    // The logic in this function is duplicated from CSSToLengthConversionData::zoomedComputedPixels()
    // because MediaValues::computeLength() needs nearly identical logic, but we haven't found a way to make
    // CSSToLengthConversionData::zoomedComputedPixels() more generic (to solve both cases) without hurting performance.

    // FIXME - Unite the logic here with CSSToLengthConversionData in a performant way.
    double factor = 0;
    switch (type) {
    case CSSPrimitiveValue::UnitType::Ems:
    case CSSPrimitiveValue::UnitType::Rems:
        factor = defaultFontSize;
        break;
    case CSSPrimitiveValue::UnitType::Pixels:
    case CSSPrimitiveValue::UnitType::UserUnits:
        factor = 1;
        break;
    case CSSPrimitiveValue::UnitType::Exs:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
        // FIXME: We don't seem to be able to cache fontMetrics related values.
        // Trying to access them is triggering some sort of microtask. Serving the spec's default instead.
        factor = defaultFontSize / 2.0;
        break;
    case CSSPrimitiveValue::UnitType::Chs:
        // FIXME: We don't seem to be able to cache fontMetrics related values.
        // Trying to access them is triggering some sort of microtask. Serving the (future) spec default instead.
        factor = defaultFontSize / 2.0;
        break;
    case CSSPrimitiveValue::UnitType::ViewportWidth:
        factor = viewportWidth / 100.0;
        break;
    case CSSPrimitiveValue::UnitType::ViewportHeight:
        factor = viewportHeight / 100.0;
        break;
    case CSSPrimitiveValue::UnitType::ViewportMin:
        factor = std::min(viewportWidth, viewportHeight) / 100.0;
        break;
    case CSSPrimitiveValue::UnitType::ViewportMax:
        factor = std::max(viewportWidth, viewportHeight) / 100.0;
        break;
    case CSSPrimitiveValue::UnitType::Centimeters:
        factor = cssPixelsPerCentimeter;
        break;
    case CSSPrimitiveValue::UnitType::Millimeters:
        factor = cssPixelsPerMillimeter;
        break;
    case CSSPrimitiveValue::UnitType::Inches:
        factor = cssPixelsPerInch;
        break;
    case CSSPrimitiveValue::UnitType::Points:
        factor = cssPixelsPerPoint;
        break;
    case CSSPrimitiveValue::UnitType::Picas:
        factor = cssPixelsPerPica;
        break;
    default:
        return false;
    }

    ASSERT(factor > 0);
    result = value * factor;
    return true;
}

LocalFrame* MediaValues::frameFrom(Document& document)
{
    Document* executingDocument = document.importsController() ? document.importsController()->master() : &document;
    ASSERT(executingDocument);
    return executingDocument->frame();
}

} // namespace blink
