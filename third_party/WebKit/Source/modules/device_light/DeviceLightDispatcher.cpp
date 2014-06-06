// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_light/DeviceLightDispatcher.h"

#include "core/frame/DeviceSensorEventDispatcher.h"
#include "modules/device_light/DeviceLightController.h"
#include "public/platform/Platform.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

DeviceLightDispatcher& DeviceLightDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(DeviceLightDispatcher, deviceLightDispatcher, ());
    return deviceLightDispatcher;
}

DeviceLightDispatcher::DeviceLightDispatcher()
    : m_lastDeviceLightData(-1)
{
}

DeviceLightDispatcher::~DeviceLightDispatcher()
{
}


void DeviceLightDispatcher::addDeviceLightController(DeviceLightController* controller)
{
    addController(controller);
}

void DeviceLightDispatcher::removeDeviceLightController(DeviceLightController* controller)
{
    removeController(controller);
}

void DeviceLightDispatcher::startListening()
{
    blink::Platform::current()->setDeviceLightListener(this);
}

void DeviceLightDispatcher::stopListening()
{
    blink::Platform::current()->setDeviceLightListener(0);
    m_lastDeviceLightData = -1;
}

void DeviceLightDispatcher::didChangeDeviceLight(double value)
{
    m_lastDeviceLightData = value;

    {
        TemporaryChange<bool> changeIsDispatching(m_isDispatching, true);
        // Don't fire controllers removed or added during event dispatch.
        size_t size = m_controllers.size();
        for (size_t i = 0; i < size; ++i) {
            if (m_controllers[i])
                static_cast<DeviceLightController*>(m_controllers[i])->didChangeDeviceLight(m_lastDeviceLightData);
        }
    }

    if (m_needsPurge)
        purgeControllers();
}

double DeviceLightDispatcher::latestDeviceLightData() const
{
    return m_lastDeviceLightData;
}

} // namespace WebCore
