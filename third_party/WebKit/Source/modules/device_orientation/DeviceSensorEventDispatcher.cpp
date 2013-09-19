/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/device_orientation/DeviceSensorEventDispatcher.h"

namespace WebCore {

DeviceSensorEventDispatcher::DeviceSensorEventDispatcher()
    : m_needsPurge(false)
    , m_isDispatching(false)
{
}

DeviceSensorEventDispatcher::~DeviceSensorEventDispatcher()
{
}

void DeviceSensorEventDispatcher::addController(DeviceSensorEventController* controller)
{
    bool wasEmpty = m_controllers.isEmpty();
    if (!m_controllers.contains(controller))
        m_controllers.append(controller);
    if (wasEmpty)
        startListening();
}

void DeviceSensorEventDispatcher::removeController(DeviceSensorEventController* controller)
{
    // Do not actually remove the controller from the vector, instead zero them out.
    // The zeros are removed in these two cases:
    // 1. either immediately if we are not dispatching any events,
    // 2. or after didChangeDeviceMotion/Orientation has dispatched all events.
    // This is to correctly handle the re-entrancy case when a controller is destroyed
    // while in the didChangeDeviceMotion/Orientation method.
    size_t index = m_controllers.find(controller);
    if (index == kNotFound)
        return;

    m_controllers[index] = 0;
    m_needsPurge = true;

    if (!m_isDispatching)
        purgeControllers();
}

void DeviceSensorEventDispatcher::purgeControllers()
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

} // namespace WebCore
