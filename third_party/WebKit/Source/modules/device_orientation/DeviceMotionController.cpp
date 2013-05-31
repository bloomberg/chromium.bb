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
#include "modules/device_orientation/DeviceMotionController.h"

#include "core/dom/Document.h"
#include "core/page/DOMWindow.h"
#include "modules/device_orientation/DeviceMotionData.h"
#include "modules/device_orientation/DeviceMotionDispatcher.h"
#include "modules/device_orientation/DeviceMotionEvent.h"

namespace WebCore {

DeviceMotionController::DeviceMotionController(Document* document)
    : m_document(document)
    , m_isActive(false)
    , m_timer(this, &DeviceMotionController::fireDeviceEvent)
{
}

DeviceMotionController::~DeviceMotionController()
{
    stopUpdating();
}

void DeviceMotionController::didChangeDeviceMotion(DeviceMotionData* deviceMotionData)
{
    dispatchDeviceEvent(DeviceMotionEvent::create(eventNames().devicemotionEvent, deviceMotionData));
}

bool DeviceMotionController::hasLastData()
{
    return DeviceMotionDispatcher::instance().latestDeviceMotionData();
}

PassRefPtr<Event> DeviceMotionController::getLastEvent()
{
    return DeviceMotionEvent::create(eventNames().devicemotionEvent,
        DeviceMotionDispatcher::instance().latestDeviceMotionData());
}

void DeviceMotionController::fireDeviceEvent(Timer<DeviceMotionController>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);
    ASSERT(hasLastData());

    m_timer.stop();
    dispatchDeviceEvent(getLastEvent());
}

void DeviceMotionController::dispatchDeviceEvent(PassRefPtr<Event> prpEvent)
{
    RefPtr<Event> event = prpEvent;
    if (m_document && m_document->domWindow()
        && !m_document->activeDOMObjectsAreSuspended()
        && !m_document->activeDOMObjectsAreStopped())
        m_document->domWindow()->dispatchEvent(event);
}

const char* DeviceMotionController::supplementName()
{
    return "DeviceMotionController";
}

DeviceMotionController* DeviceMotionController::from(Document* document)
{
    DeviceMotionController* controller = static_cast<DeviceMotionController*>(Supplement<ScriptExecutionContext>::from(document, supplementName()));
    if (!controller) {
        controller = new DeviceMotionController(document);
        Supplement<ScriptExecutionContext>::provideTo(document, supplementName(), adoptPtr(controller));
    }
    return controller;
}

void DeviceMotionController::startUpdating()
{
    if (m_isActive)
        return;

    DeviceMotionDispatcher::instance().addController(this);
    m_isActive = true;

    if (hasLastData() && !m_timer.isActive())
        // Make sure to fire the device motion data as soon as possible.
        m_timer.startOneShot(0);
}

void DeviceMotionController::stopUpdating()
{
    if (!m_isActive)
        return;

    DeviceMotionDispatcher::instance().removeController(this);
    m_isActive = false;
}

} // namespace WebCore
