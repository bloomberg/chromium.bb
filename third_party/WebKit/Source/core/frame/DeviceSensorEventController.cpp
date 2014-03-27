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
#include "core/frame/DeviceSensorEventController.h"

#include "core/dom/Document.h"
#include "core/frame/DOMWindow.h"
#include "core/page/Page.h"

namespace WebCore {

DeviceSensorEventController::DeviceSensorEventController(Document& document)
    : PageLifecycleObserver(document.page())
    , m_hasEventListener(false)
    , m_document(document)
    , m_isActive(false)
    , m_needsCheckingNullEvents(true)
    , m_timer(this, &DeviceSensorEventController::fireDeviceEvent)
{
}

DeviceSensorEventController::~DeviceSensorEventController()
{
}

void DeviceSensorEventController::fireDeviceEvent(Timer<DeviceSensorEventController>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_timer);
    ASSERT(hasLastData());

    m_timer.stop();
    dispatchDeviceEvent(getLastEvent());
}

void DeviceSensorEventController::dispatchDeviceEvent(PassRefPtr<Event> prpEvent)
{
    if (!m_document.domWindow() || m_document.activeDOMObjectsAreSuspended() || m_document.activeDOMObjectsAreStopped())
        return;

    RefPtr<Event> event = prpEvent;
    m_document.domWindow()->dispatchEvent(event);

    if (m_needsCheckingNullEvents) {
        if (isNullEvent(event.get()))
            stopUpdating();
        else
            m_needsCheckingNullEvents = false;
    }
}

void DeviceSensorEventController::startUpdating()
{
    if (m_isActive)
        return;

    if (hasLastData() && !m_timer.isActive()) {
        // Make sure to fire the data as soon as possible.
        m_timer.startOneShot(0, FROM_HERE);
    }

    registerWithDispatcher();
    m_isActive = true;
}

void DeviceSensorEventController::stopUpdating()
{
    if (!m_isActive)
        return;

    if (m_timer.isActive())
        m_timer.stop();

    unregisterWithDispatcher();
    m_isActive = false;
}

void DeviceSensorEventController::pageVisibilityChanged()
{
    if (!m_hasEventListener)
        return;

    if (page()->visibilityState() == PageVisibilityStateVisible)
        startUpdating();
    else
        stopUpdating();
}

} // namespace WebCore
