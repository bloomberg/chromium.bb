/*
 * Copyright 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/device_orientation/DeviceOrientationController.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/page/Page.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "modules/device_orientation/DeviceOrientationDispatcher.h"
#include "modules/device_orientation/DeviceOrientationEvent.h"

namespace WebCore {

DeviceOrientationController::DeviceOrientationController(Document& document)
    : DeviceSensorEventController(document)
    , DOMWindowLifecycleObserver(document.domWindow())
{
}

DeviceOrientationController::~DeviceOrientationController()
{
    stopUpdating();
}

void DeviceOrientationController::didChangeDeviceOrientation(DeviceOrientationData* deviceOrientationData)
{
    if (m_overrideOrientationData)
        return;
    dispatchDeviceEvent(DeviceOrientationEvent::create(EventTypeNames::deviceorientation, deviceOrientationData));
}

const char* DeviceOrientationController::supplementName()
{
    return "DeviceOrientationController";
}

DeviceOrientationController& DeviceOrientationController::from(Document& document)
{
    DeviceOrientationController* controller = static_cast<DeviceOrientationController*>(DocumentSupplement::from(document, supplementName()));
    if (!controller) {
        controller = new DeviceOrientationController(document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtr(controller));
    }
    return *controller;
}

DeviceOrientationData* DeviceOrientationController::lastData()
{
    return m_overrideOrientationData ? m_overrideOrientationData.get() : DeviceOrientationDispatcher::instance().latestDeviceOrientationData();
}

bool DeviceOrientationController::hasLastData()
{
    return lastData();
}

PassRefPtrWillBeRawPtr<Event> DeviceOrientationController::getLastEvent()
{
    return DeviceOrientationEvent::create(EventTypeNames::deviceorientation, lastData());
}

void DeviceOrientationController::registerWithDispatcher()
{
    DeviceOrientationDispatcher::instance().addDeviceOrientationController(this);
}

void DeviceOrientationController::unregisterWithDispatcher()
{
    DeviceOrientationDispatcher::instance().removeDeviceOrientationController(this);
}

bool DeviceOrientationController::isNullEvent(Event* event)
{
    DeviceOrientationEvent* orientationEvent = toDeviceOrientationEvent(event);
    return !orientationEvent->orientation()->canProvideEventData();
}

void DeviceOrientationController::didAddEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType == EventTypeNames::deviceorientation && RuntimeEnabledFeatures::deviceOrientationEnabled()) {
        if (page() && page()->visibilityState() == PageVisibilityStateVisible)
            startUpdating();
        m_hasEventListener = true;
    }
}

void DeviceOrientationController::didRemoveEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType == EventTypeNames::deviceorientation) {
        stopUpdating();
        m_hasEventListener = false;
    }
}

void DeviceOrientationController::didRemoveAllEventListeners(DOMWindow* window)
{
    stopUpdating();
    m_hasEventListener = false;
}

void DeviceOrientationController::setOverride(DeviceOrientationData* deviceOrientationData)
{
    m_overrideOrientationData.clear();
    didChangeDeviceOrientation(deviceOrientationData);
    m_overrideOrientationData = deviceOrientationData;
}

void DeviceOrientationController::clearOverride()
{
    if (!m_overrideOrientationData)
        return;
    m_overrideOrientationData.clear();
    DeviceOrientationData* orientation = lastData();
    if (orientation)
        didChangeDeviceOrientation(orientation);
}

} // namespace WebCore
