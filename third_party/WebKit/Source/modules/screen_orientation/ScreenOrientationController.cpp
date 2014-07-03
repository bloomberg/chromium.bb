// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientationController.h"

#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Screen.h"
#include "core/page/Page.h"
#include "modules/screen_orientation/OrientationInformation.h"
#include "platform/LayoutTestSupport.h"
#include "platform/PlatformScreen.h"
#include "public/platform/WebScreenOrientationClient.h"

namespace WebCore {

ScreenOrientationController::~ScreenOrientationController()
{
}

void ScreenOrientationController::persistentHostHasBeenDestroyed()
{
    // Unregister lifecycle observation once page is being torn down.
    observeContext(0);
}

void ScreenOrientationController::provideTo(LocalFrame& frame, blink::WebScreenOrientationClient* client)
{
    ScreenOrientationController* controller = new ScreenOrientationController(frame, client);
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, supplementName(), adoptPtrWillBeNoop(controller));
}

ScreenOrientationController& ScreenOrientationController::from(LocalFrame& frame)
{
    return *static_cast<ScreenOrientationController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
}

ScreenOrientationController::ScreenOrientationController(LocalFrame& frame, blink::WebScreenOrientationClient* client)
    : PageLifecycleObserver(frame.page())
    , m_orientation(new OrientationInformation)
    , m_client(client)
    , m_frame(frame)
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

void ScreenOrientationController::updateOrientation()
{
    blink::WebScreenOrientationType orientationType = screenOrientationType(m_frame.view());
    if (orientationType == blink::WebScreenOrientationUndefined) {
        // The embedder could not provide us with an orientation, deduce it ourselves.
        orientationType = computeOrientation(m_frame.view());
    }
    ASSERT(orientationType != blink::WebScreenOrientationUndefined);

    m_orientation->setType(orientationType);
    m_orientation->setAngle(screenOrientationAngle(m_frame.view()));
}

void ScreenOrientationController::pageVisibilityChanged()
{
    if (!page() || page()->visibilityState() != PageVisibilityStateVisible)
        return;

    // The orientation type and angle are tied in a way that if the angle has
    // changed, the type must have changed.
    unsigned short currentAngle = screenOrientationAngle(m_frame.view());

    // FIXME: sendOrientationChangeEvent() currently send an event all the
    // children of the frame, so it should only be called on the frame on
    // top of the tree. We would need the embedder to call
    // sendOrientationChangeEvent on every WebFrame part of a WebView to be
    // able to remove this.
    if (m_frame == m_frame.localFrameRoot() && m_orientation->angle() != currentAngle)
        notifyOrientationChanged();
}

void ScreenOrientationController::notifyOrientationChanged()
{
    if (!RuntimeEnabledFeatures::orientationEventEnabled() && !RuntimeEnabledFeatures::screenOrientationEnabled())
        return;

    if (!page() || page()->visibilityState() != PageVisibilityStateVisible)
        return;

    updateOrientation();

    // Keep track of the frames that need to be notified before notifying the
    // current frame as it will prevent side effects from the orientationchange
    // event handlers.
    Vector<RefPtr<LocalFrame> > childFrames;
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            childFrames.append(toLocalFrame(child));
    }

    // Notify current frame.
    LocalDOMWindow* window = m_frame.domWindow();
    if (window)
        window->dispatchEvent(Event::create(EventTypeNames::orientationchange));

    // Notify subframes.
    for (size_t i = 0; i < childFrames.size(); ++i)
        ScreenOrientationController::from(*childFrames[i]).notifyOrientationChanged();
}

OrientationInformation* ScreenOrientationController::orientation()
{
    if (!m_orientation->initialized())
        updateOrientation();

    return m_orientation.get();
}

void ScreenOrientationController::lockOrientation(blink::WebScreenOrientationLockType orientation, blink::WebLockOrientationCallback* callback)
{
    if (!m_client) {
        return;
    }

    m_client->lockOrientation(orientation, callback);
}

void ScreenOrientationController::unlockOrientation()
{
    if (!m_client) {
        return;
    }

    m_client->unlockOrientation();
}

void ScreenOrientationController::trace(Visitor* visitor)
{
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
    visitor->trace(m_orientation);
}

} // namespace WebCore
