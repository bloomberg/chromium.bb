/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DeviceSensorEventController_h
#define DeviceSensorEventController_h

#include "core/events/Event.h"
#include "core/page/PageLifecycleObserver.h"
#include "platform/Timer.h"

namespace WebCore {

class Document;

class DeviceSensorEventController : public PageLifecycleObserver {

public:
    void startUpdating();
    void stopUpdating();

protected:
    explicit DeviceSensorEventController(Document&);
    virtual ~DeviceSensorEventController();

    void dispatchDeviceEvent(const PassRefPtrWillBeRawPtr<Event>);

    virtual bool hasLastData() = 0;
    virtual PassRefPtrWillBeRawPtr<Event> getLastEvent() = 0;
    virtual void registerWithDispatcher() = 0;
    virtual void unregisterWithDispatcher() = 0;
    virtual bool isNullEvent(Event*) = 0;

    bool m_hasEventListener;

private:
    // Inherited from PageLifecycleObserver.
    virtual void pageVisibilityChanged() OVERRIDE FINAL;

    void fireDeviceEvent(Timer<DeviceSensorEventController>*);

    Document& m_document;
    bool m_isActive;
    bool m_needsCheckingNullEvents;
    Timer<DeviceSensorEventController> m_timer;
};

} // namespace WebCore

#endif // DeviceSensorEventController_h
