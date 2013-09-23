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
#include "modules/device_orientation/NewDeviceOrientationController.h"

#include "RuntimeEnabledFeatures.h"
#include "core/dom/Document.h"
#include "core/events/EventNames.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "modules/device_orientation/DeviceOrientationDispatcher.h"
#include "modules/device_orientation/DeviceOrientationEvent.h"

namespace WebCore {

NewDeviceOrientationController::NewDeviceOrientationController(Document* document)
    : DOMWindowLifecycleObserver(document->domWindow())
    , DeviceSensorEventController(document)
{
}

NewDeviceOrientationController::~NewDeviceOrientationController()
{
}

void NewDeviceOrientationController::didChangeDeviceOrientation(DeviceOrientationData* deviceOrientationData)
{
    dispatchDeviceEvent(DeviceOrientationEvent::create(eventNames().deviceorientationEvent, deviceOrientationData));
}

const char* NewDeviceOrientationController::supplementName()
{
    return "NewDeviceOrientationController";
}

NewDeviceOrientationController* NewDeviceOrientationController::from(Document* document)
{
    NewDeviceOrientationController* controller = static_cast<NewDeviceOrientationController*>(Supplement<ScriptExecutionContext>::from(document, supplementName()));
    if (!controller) {
        controller = new NewDeviceOrientationController(document);
        Supplement<ScriptExecutionContext>::provideTo(document, supplementName(), adoptPtr(controller));
    }
    return controller;
}

bool NewDeviceOrientationController::hasLastData()
{
    return DeviceOrientationDispatcher::instance().latestDeviceOrientationData();
}

PassRefPtr<Event> NewDeviceOrientationController::getLastEvent()
{
    return DeviceOrientationEvent::create(eventNames().deviceorientationEvent,
        DeviceOrientationDispatcher::instance().latestDeviceOrientationData());
}

void NewDeviceOrientationController::registerWithDispatcher()
{
    DeviceOrientationDispatcher::instance().addDeviceOrientationController(this);
}

void NewDeviceOrientationController::unregisterWithDispatcher()
{
    DeviceOrientationDispatcher::instance().removeDeviceOrientationController(this);
}

bool NewDeviceOrientationController::isNullEvent(Event* event)
{
    ASSERT(event->type() == eventNames().deviceorientationEvent);
    DeviceOrientationEvent* orientationEvent = static_cast<DeviceOrientationEvent*>(event);
    return !orientationEvent->orientation()->canProvideEventData();
}

void NewDeviceOrientationController::didAddEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType == eventNames().deviceorientationEvent && RuntimeEnabledFeatures::deviceOrientationEnabled())
        startUpdating();
}

void NewDeviceOrientationController::didRemoveEventListener(DOMWindow* window, const AtomicString& eventType)
{
    if (eventType == eventNames().deviceorientationEvent)
        stopUpdating();
}

void NewDeviceOrientationController::didRemoveAllEventListeners(DOMWindow* window)
{
    stopUpdating();
}

} // namespace WebCore
