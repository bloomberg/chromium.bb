// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceLightController_h
#define DeviceLightController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/frame/DOMWindowLifecycleObserver.h"
#include "core/frame/DeviceSensorEventController.h"
#include "modules/EventModules.h"

namespace WebCore {

class DOMWindow;

class DeviceLightController FINAL : public NoBaseWillBeGarbageCollectedFinalized<DeviceLightController>, public DeviceSensorEventController, public DocumentSupplement, public DOMWindowLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DeviceLightController);
public:
    virtual ~DeviceLightController();

    static const char* supplementName();
    static DeviceLightController& from(Document&);

    void didChangeDeviceLight(double);

private:
    explicit DeviceLightController(Document&);
    virtual void registerWithDispatcher() OVERRIDE;
    virtual void unregisterWithDispatcher() OVERRIDE;

    // Inherited from DOMWindowLifecycleObserver.
    virtual void didAddEventListener(DOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveEventListener(DOMWindow*, const AtomicString&) OVERRIDE;
    virtual void didRemoveAllEventListeners(DOMWindow*) OVERRIDE;

    virtual bool hasLastData() OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<Event> getLastEvent() OVERRIDE;
    virtual bool isNullEvent(Event*) OVERRIDE;
    virtual Document* document() OVERRIDE;

    Document& m_document;
};

} // namespace WebCore

#endif // DeviceLightController_h
