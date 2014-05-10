// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/device_light/DeviceLightController.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/frame/DOMWindow.h"
#include "core/page/Page.h"
#include "modules/device_light/DeviceLightDispatcher.h"
#include "modules/device_light/DeviceLightEvent.h"

namespace WebCore {

DeviceLightController::DeviceLightController(Document& document)
    : DeviceSensorEventController(document.page())
    , DOMWindowLifecycleObserver(document.domWindow())
    , m_document(document)
{
}

DeviceLightController::~DeviceLightController()
{
    stopUpdating();
}

void DeviceLightController::didChangeDeviceLight(double value)
{
    dispatchDeviceEvent(DeviceLightEvent::create(EventTypeNames::devicelight, value));
}

const char* DeviceLightController::supplementName()
{
    return "DeviceLightController";
}

DeviceLightController& DeviceLightController::from(Document& document)
{
    DeviceLightController* controller = static_cast<DeviceLightController*>(DocumentSupplement::from(document, supplementName()));
    if (!controller) {
        controller = new DeviceLightController(document);
        DocumentSupplement::provideTo(document, supplementName(), adoptPtrWillBeNoop(controller));
    }
    return *controller;
}

bool DeviceLightController::hasLastData()
{
    return DeviceLightDispatcher::instance().latestDeviceLightData() >= 0;
}

PassRefPtrWillBeRawPtr<Event> DeviceLightController::getLastEvent()
{
    return DeviceLightEvent::create(EventTypeNames::devicelight,
        DeviceLightDispatcher::instance().latestDeviceLightData());
}

void DeviceLightController::registerWithDispatcher()
{
    DeviceLightDispatcher::instance().addDeviceLightController(this);
}

void DeviceLightController::unregisterWithDispatcher()
{
    DeviceLightDispatcher::instance().removeDeviceLightController(this);
}

bool DeviceLightController::isNullEvent(Event* event)
{
    DeviceLightEvent* lightEvent = toDeviceLightEvent(event);
    return lightEvent->value() == std::numeric_limits<double>::infinity();
}

Document* DeviceLightController::document()
{
    return &m_document;
}

void DeviceLightController::didAddEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType == EventTypeNames::devicelight && RuntimeEnabledFeatures::deviceLightEnabled()) {
        if (page() && page()->visibilityState() == PageVisibilityStateVisible)
            startUpdating();
        m_hasEventListener = true;
    }
}

void DeviceLightController::didRemoveEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType != EventTypeNames::devicelight || window->hasEventListeners(EventTypeNames::devicelight))
        return;

    stopUpdating();
    m_hasEventListener = false;
}

void DeviceLightController::didRemoveAllEventListeners(DOMWindow* window)
{
    stopUpdating();
    m_hasEventListener = false;
}

} // namespace WebCore
