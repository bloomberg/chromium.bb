// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/screen_orientation/ScreenOrientationDispatcher.h"

#include "modules/screen_orientation/ScreenOrientationController.h"
#include "public/platform/Platform.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

ScreenOrientationDispatcher& ScreenOrientationDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(ScreenOrientationDispatcher, screenOrientationDispatcher, ());
    return screenOrientationDispatcher;
}

ScreenOrientationDispatcher::ScreenOrientationDispatcher()
    : m_needsPurge(false)
    , m_isDispatching(false)
{
}

void ScreenOrientationDispatcher::addController(ScreenOrientationController* controller)
{
    bool wasEmpty = m_controllers.isEmpty();
    if (!m_controllers.contains(controller))
        m_controllers.append(controller);
    if (wasEmpty)
        startListening();
}

void ScreenOrientationDispatcher::removeController(ScreenOrientationController* controller)
{
    // Do not actually remove the controllers from the vector, instead zero them out.
    // The zeros are removed in these two cases:
    // 1. either immediately if we are not dispatching any events,
    // 2. or after didChangeScreenOrientation has dispatched all events.
    // This is to correctly handle the re-entrancy case when a controller is destroyed
    // while in the didChangeScreenOrientation() method.
    size_t index = m_controllers.find(controller);
    if (index == kNotFound)
        return;

    m_controllers[index] = 0;
    m_needsPurge = true;

    if (!m_isDispatching)
        purgeControllers();
}

void ScreenOrientationDispatcher::purgeControllers()
{
    ASSERT(m_needsPurge);

    size_t i = 0;
    while (i < m_controllers.size()) {
        if (!m_controllers[i]) {
            m_controllers[i] = m_controllers.last();
            m_controllers.removeLast();
        } else {
            ++i;
        }
    }

    m_needsPurge = false;

    if (m_controllers.isEmpty())
        stopListening();
}

void ScreenOrientationDispatcher::didChangeScreenOrientation(blink::WebScreenOrientationType orientation)
{
    {
        TemporaryChange<bool> changeIsDispatching(m_isDispatching, true);
        // Don't fire controllers removed or added during event dispatch.
        size_t size = m_controllers.size();
        for (size_t i = 0; i < size; ++i) {
            if (m_controllers[i])
                static_cast<ScreenOrientationController*>(m_controllers[i])->didChangeScreenOrientation(orientation);
        }
    }

    if (m_needsPurge)
        purgeControllers();
}

void ScreenOrientationDispatcher::startListening()
{
    blink::Platform::current()->setScreenOrientationListener(this);
}

void ScreenOrientationDispatcher::stopListening()
{
    blink::Platform::current()->setScreenOrientationListener(0);
}

} // namespace WebCore
