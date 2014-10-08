// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationController_h
#define DeviceOrientationController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/frame/DeviceSingleWindowEventController.h"

namespace blink {

class DeviceOrientationData;
class Event;

class DeviceOrientationController final : public DeviceSingleWindowEventController, public DocumentSupplement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DeviceOrientationController);
public:
    virtual ~DeviceOrientationController();

    static const char* supplementName();
    static DeviceOrientationController& from(Document&);

    // Inherited from DeviceSingleWindowEventController.
    void didUpdateData() override;

    void setOverride(DeviceOrientationData*);
    void clearOverride();

    virtual void trace(Visitor*) override;

private:
    explicit DeviceOrientationController(Document&);

    // Inherited from DeviceEventControllerBase.
    virtual void registerWithDispatcher() override;
    virtual void unregisterWithDispatcher() override;
    virtual bool hasLastData() override;

    // Inherited from DeviceSingleWindowEventController.
    virtual PassRefPtrWillBeRawPtr<Event> lastEvent() const override;
    virtual const AtomicString& eventTypeName() const override;
    virtual bool isNullEvent(Event*) const override;

    DeviceOrientationData* lastData() const;

    PersistentWillBeMember<DeviceOrientationData> m_overrideOrientationData;
};

} // namespace blink

#endif // DeviceOrientationController_h
