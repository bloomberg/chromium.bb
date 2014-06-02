// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientationController.h"

#include "core/dom/Document.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Screen.h"
#include "core/page/Page.h"
#include "platform/LayoutTestSupport.h"
#include "platform/PlatformScreen.h"

namespace WebCore {

ScreenOrientationController::~ScreenOrientationController()
{
}

ScreenOrientationController& ScreenOrientationController::from(Document& document)
{
    ScreenOrientationController* controller = static_cast<ScreenOrientationController*>(DocumentSupplement::from(document, supplementName()));
    if (!controller) {
        controller = new ScreenOrientationController(document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtrWillBeNoop(controller));
    }
    return *controller;
}

ScreenOrientationController::ScreenOrientationController(Document& document)
    : PageLifecycleObserver(document.page())
    , m_document(document)
    , m_overrideOrientation(blink::WebScreenOrientationUndefined)
{
}

const char* ScreenOrientationController::supplementName()
{
    return "ScreenOrientationController";
}

// Compute the screen orientation using the orientation angle and the screen width / height.
blink::WebScreenOrientationType ScreenOrientationController::computeOrientation(FrameView* view)
{
    // Bypass orientation detection in layout tests to get consistent results.
    // FIXME: The screen dimension should be fixed when running the layout tests to avoid such
    // issues.
    if (isRunningLayoutTest())
        return blink::WebScreenOrientationPortraitPrimary;

    FloatRect rect = screenRect(view);
    uint16_t rotation = screenOrientationAngle(view);
    bool isTallDisplay = rotation % 180 ? rect.height() < rect.width() : rect.height() > rect.width();
    switch (rotation) {
    case 0:
        return isTallDisplay ? blink::WebScreenOrientationPortraitPrimary : blink::WebScreenOrientationLandscapePrimary;
    case 90:
        return isTallDisplay ? blink::WebScreenOrientationLandscapePrimary : blink::WebScreenOrientationPortraitSecondary;
    case 180:
        return isTallDisplay ? blink::WebScreenOrientationPortraitSecondary : blink::WebScreenOrientationLandscapeSecondary;
    case 270:
        return isTallDisplay ? blink::WebScreenOrientationLandscapeSecondary : blink::WebScreenOrientationPortraitPrimary;
    default:
        ASSERT_NOT_REACHED();
        return blink::WebScreenOrientationPortraitPrimary;
    }
}

void ScreenOrientationController::pageVisibilityChanged()
{
    if (page() && page()->visibilityState() == PageVisibilityStateVisible) {
        blink::WebScreenOrientationType oldOrientation = m_overrideOrientation;
        m_overrideOrientation = blink::WebScreenOrientationUndefined;
        LocalFrame* mainFrame = m_document.frame();
        if (mainFrame && oldOrientation != orientation())
            mainFrame->sendOrientationChangeEvent();
    } else if (m_overrideOrientation == blink::WebScreenOrientationUndefined) {
        // The page is no longer visible, store the last know screen orientation
        // so that we keep returning this orientation until the page becomes
        // visible again.
        m_overrideOrientation = orientation();
    }
}

blink::WebScreenOrientationType ScreenOrientationController::orientation() const
{
    if (m_overrideOrientation != blink::WebScreenOrientationUndefined) {
        // The page is not visible, keep returning the last known screen orientation.
        ASSERT(!page() || page()->visibilityState() != PageVisibilityStateVisible);
        return m_overrideOrientation;
    }

    LocalFrame* mainFrame = m_document.frame();
    if (!mainFrame)
        return blink::WebScreenOrientationPortraitPrimary;
    blink::WebScreenOrientationType orientationType = screenOrientationType(mainFrame->view());
    if (orientationType == blink::WebScreenOrientationUndefined) {
        // The embedder could not provide us with an orientation, deduce it ourselves.
        orientationType = computeOrientation(mainFrame->view());
    }
    ASSERT(orientationType != blink::WebScreenOrientationUndefined);
    return orientationType;
}

} // namespace WebCore
