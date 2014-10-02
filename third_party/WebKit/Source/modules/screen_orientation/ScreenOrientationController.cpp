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
#include "modules/screen_orientation/ScreenOrientationDispatcher.h"
#include "platform/LayoutTestSupport.h"
#include "platform/PlatformScreen.h"
#include "public/platform/WebScreenOrientationClient.h"

namespace blink {

ScreenOrientationController::~ScreenOrientationController()
{
}

void ScreenOrientationController::provideTo(LocalFrame& frame, WebScreenOrientationClient* client)
{
    ASSERT(RuntimeEnabledFeatures::screenOrientationEnabled());

    ScreenOrientationController* controller = new ScreenOrientationController(frame, client);
    WillBeHeapSupplement<LocalFrame>::provideTo(frame, supplementName(), adoptPtrWillBeNoop(controller));
}

ScreenOrientationController* ScreenOrientationController::from(LocalFrame& frame)
{
    return static_cast<ScreenOrientationController*>(WillBeHeapSupplement<LocalFrame>::from(frame, supplementName()));
}

ScreenOrientationController::ScreenOrientationController(LocalFrame& frame, WebScreenOrientationClient* client)
    : FrameDestructionObserver(&frame)
    , PlatformEventController(frame.page())
    , m_client(client)
    , m_dispatchEventTimer(this, &ScreenOrientationController::dispatchEventTimerFired)
{
}

const char* ScreenOrientationController::supplementName()
{
    return "ScreenOrientationController";
}

// Compute the screen orientation using the orientation angle and the screen width / height.
WebScreenOrientationType ScreenOrientationController::computeOrientation(FrameView* view)
{
    // Bypass orientation detection in layout tests to get consistent results.
    // FIXME: The screen dimension should be fixed when running the layout tests to avoid such
    // issues.
    if (LayoutTestSupport::isRunningLayoutTest())
        return WebScreenOrientationPortraitPrimary;

    FloatRect rect = screenRect(view);
    uint16_t rotation = screenOrientationAngle(view);
    bool isTallDisplay = rotation % 180 ? rect.height() < rect.width() : rect.height() > rect.width();
    switch (rotation) {
    case 0:
        return isTallDisplay ? WebScreenOrientationPortraitPrimary : WebScreenOrientationLandscapePrimary;
    case 90:
        return isTallDisplay ? WebScreenOrientationLandscapePrimary : WebScreenOrientationPortraitSecondary;
    case 180:
        return isTallDisplay ? WebScreenOrientationPortraitSecondary : WebScreenOrientationLandscapeSecondary;
    case 270:
        return isTallDisplay ? WebScreenOrientationLandscapeSecondary : WebScreenOrientationPortraitPrimary;
    default:
        ASSERT_NOT_REACHED();
        return WebScreenOrientationPortraitPrimary;
    }
}

void ScreenOrientationController::updateOrientation()
{
    ASSERT(m_orientation);
    ASSERT(frame());

    FrameView* view = frame()->view();
    WebScreenOrientationType orientationType = screenOrientationType(view);
    if (orientationType == WebScreenOrientationUndefined) {
        // The embedder could not provide us with an orientation, deduce it ourselves.
        orientationType = computeOrientation(view);
    }
    ASSERT(orientationType != WebScreenOrientationUndefined);

    m_orientation->setType(orientationType);
    m_orientation->setAngle(screenOrientationAngle(view));
}

bool ScreenOrientationController::isActiveAndVisible() const
{
    return m_orientation && frame() && page() && page()->visibilityState() == PageVisibilityStateVisible;
}

void ScreenOrientationController::pageVisibilityChanged()
{
    notifyDispatcher();

    if (!isActiveAndVisible())
        return;

    // The orientation type and angle are tied in a way that if the angle has
    // changed, the type must have changed.
    unsigned short currentAngle = screenOrientationAngle(frame()->view());

    // FIXME: sendOrientationChangeEvent() currently send an event all the
    // children of the frame, so it should only be called on the frame on
    // top of the tree. We would need the embedder to call
    // sendOrientationChangeEvent on every WebFrame part of a WebView to be
    // able to remove this.
    if (frame() == frame()->localFrameRoot() && m_orientation->angle() != currentAngle)
        notifyOrientationChanged();
}

void ScreenOrientationController::notifyOrientationChanged()
{
    ASSERT(RuntimeEnabledFeatures::screenOrientationEnabled());

    if (!isActiveAndVisible())
        return;

    updateOrientation();

    // Keep track of the frames that need to be notified before notifying the
    // current frame as it will prevent side effects from the change event
    // handlers.
    WillBeHeapVector<RefPtrWillBeMember<LocalFrame> > childFrames;
    for (Frame* child = frame()->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            childFrames.append(toLocalFrame(child));
    }

    // Notify current orientation object.
    if (!m_dispatchEventTimer.isActive())
        m_dispatchEventTimer.startOneShot(0, FROM_HERE);

    // ... and child frames, if they have a ScreenOrientationController.
    for (size_t i = 0; i < childFrames.size(); ++i) {
        if (ScreenOrientationController* controller = ScreenOrientationController::from(*childFrames[i]))
            controller->notifyOrientationChanged();
    }
}

void ScreenOrientationController::setOrientation(ScreenOrientation* orientation)
{
    m_orientation = orientation;
    if (m_orientation)
        updateOrientation();
    notifyDispatcher();
}

void ScreenOrientationController::lock(WebScreenOrientationLockType orientation, WebLockOrientationCallback* callback)
{
    ASSERT(m_client);
    m_client->lockOrientation(orientation, callback);
}

void ScreenOrientationController::unlock()
{
    ASSERT(m_client);
    m_client->unlockOrientation();
}

void ScreenOrientationController::dispatchEventTimerFired(Timer<ScreenOrientationController>*)
{
    if (!m_orientation)
        return;
    m_orientation->dispatchEvent(Event::create(EventTypeNames::change));
}

void ScreenOrientationController::didUpdateData()
{
    // Do nothing.
}

void ScreenOrientationController::registerWithDispatcher()
{
    ScreenOrientationDispatcher::instance().addController(this);
}

void ScreenOrientationController::unregisterWithDispatcher()
{
    ScreenOrientationDispatcher::instance().removeController(this);
}

bool ScreenOrientationController::hasLastData()
{
    return true;
}

void ScreenOrientationController::notifyDispatcher()
{
    if (m_orientation && page()->visibilityState() == PageVisibilityStateVisible)
        startUpdating();
    else
        stopUpdating();
}

void ScreenOrientationController::trace(Visitor* visitor)
{
    visitor->trace(m_orientation);
    FrameDestructionObserver::trace(visitor);
    WillBeHeapSupplement<LocalFrame>::trace(visitor);
    PlatformEventController::trace(visitor);
}

} // namespace blink
