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
#if ENABLE(OILPAN)
    DEFINE_STATIC_LOCAL(Persistent<ScreenOrientationDispatcher>, screenOrientationDispatcher, (new ScreenOrientationDispatcher()));
    return *screenOrientationDispatcher;
#else
    DEFINE_STATIC_LOCAL(ScreenOrientationDispatcher, screenOrientationDispatcher, ());
    return screenOrientationDispatcher;
#endif
}

ScreenOrientationDispatcher::ScreenOrientationDispatcher()
#if !ENABLE(OILPAN)
    : m_needsPurge(false)
    , m_isDispatching(false)
#endif
{
}

void ScreenOrientationDispatcher::addController(ScreenOrientationController* controller)
{
    bool wasEmpty = m_controllers.isEmpty();
#if ENABLE(OILPAN)
    m_controllers.add(controller);
#else
    if (!m_controllers.contains(controller))
        m_controllers.append(controller);
#endif
    if (wasEmpty)
        startListening();
}

#if !ENABLE(OILPAN)
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
#endif

void ScreenOrientationDispatcher::didChangeScreenOrientation(blink::WebScreenOrientationType orientation)
{
#if ENABLE(OILPAN)
    if (m_controllers.isEmpty()) {
        stopListening();
        return;
    }
    // The on-stack iterator will make m_controllers strong while iterating,
    // therefore no controllers can be removed during iteration.
    for (HeapHashSet<WeakMember<ScreenOrientationController> >::iterator it = m_controllers.begin(); it != m_controllers.end(); ++it) {
        (*it)->didChangeScreenOrientation(orientation);
    }
#else
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
#endif

void ScreenOrientationDispatcher::startListening()
{
    blink::Platform::current()->setScreenOrientationListener(this);
}

void ScreenOrientationDispatcher::stopListening()
{
    blink::Platform::current()->setScreenOrientationListener(0);
}

#if ENABLE(OILPAN)
void ScreenOrientationDispatcher::trace(Visitor* visitor)
{
    // Weak processing will remove controllers once they are dead.
    visitor->trace(m_controllers);
}
#endif

} // namespace WebCore
