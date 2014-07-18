// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientationController.h"

#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/page/Page.h"
#include "modules/screen_orientation/ScreenOrientation.h"
#include "platform/LayoutTestSupport.h"
#include "platform/PlatformScreen.h"
#include "public/platform/WebScreenOrientationClient.h"

namespace blink {

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
    ASSERT(RuntimeEnabledFeatures::screenOrientationEnabled());

    ScreenOrientationController* controller = new ScreenOrientationController(frame, client);
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, supplementName(), adoptPtrWillBeNoop(controller));
}

ScreenOrientationController* ScreenOrientationController::from(LocalFrame& frame)
{
    return static_cast<ScreenOrientationController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
}

ScreenOrientationController::ScreenOrientationController(LocalFrame& frame, blink::WebScreenOrientationClient* client)
    : PageLifecycleObserver(frame.page())
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
    if (LayoutTestSupport::isRunningLayoutTest())
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
    ASSERT(m_orientation);

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
    if (!m_orientation || !page() || page()->visibilityState() != PageVisibilityStateVisible)
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
    ASSERT(RuntimeEnabledFeatures::screenOrientationEnabled());

    if (!m_orientation || !page() || page()->visibilityState() != PageVisibilityStateVisible)
        return;

    updateOrientation();

    // Keep track of the frames that need to be notified before notifying the
    // current frame as it will prevent side effects from the change event
    // handlers.
    Vector<RefPtr<LocalFrame> > childFrames;
    for (Frame* child = m_frame.tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            childFrames.append(toLocalFrame(child));
    }

    // Notify current orientation object.
    m_orientation->dispatchEvent(Event::create(EventTypeNames::change));

    // ... and child frames, if they have a ScreenOrientationController.
    for (size_t i = 0; i < childFrames.size(); ++i) {
        ScreenOrientationController* controller = ScreenOrientationController::from(*childFrames[i]);
        if (controller)
            controller->notifyOrientationChanged();
    }
}

void ScreenOrientationController::setOrientation(ScreenOrientation* orientation)
{
    m_orientation = orientation;
    if (m_orientation)
        updateOrientation();
}

void ScreenOrientationController::lock(blink::WebScreenOrientationLockType orientation, blink::WebLockOrientationCallback* callback)
{
    if (!m_client) {
        return;
    }

    m_client->lockOrientation(orientation, callback);
}

void ScreenOrientationController::unlock()
{
    if (!m_client) {
        return;
    }

    m_client->unlockOrientation();
}

const LocalFrame& ScreenOrientationController::frame() const
{
    return m_frame;
}

void ScreenOrientationController::trace(Visitor* visitor)
{
    visitor->trace(m_orientation);
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
}

} // namespace blink
