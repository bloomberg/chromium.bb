// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceMotionController_h
#define DeviceMotionController_h

#include "core/dom/DocumentSupplementable.h"
#include "core/frame/DeviceSingleWindowEventController.h"

namespace blink {

class DeviceMotionData;
class Event;

class DeviceMotionController final : public DeviceSingleWindowEventController, public DocumentSupplement {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(DeviceMotionController);
public:
    virtual ~DeviceMotionController();

    static const char* supplementName();
    static DeviceMotionController& from(Document&);

    virtual void trace(Visitor*) override;

private:
    explicit DeviceMotionController(Document&);

    // Inherited from DeviceEventControllerBase.
    virtual void registerWithDispatcher() override;
    virtual void unregisterWithDispatcher() override;
    virtual bool hasLastData() override;

    // Inherited from DeviceSingleWindowEventController.
    virtual PassRefPtrWillBeRawPtr<Event> lastEvent() const override;
    virtual const AtomicString& eventTypeName() const override;
    virtual bool isNullEvent(Event*) const override;
};

} // namespace blink

#endif // DeviceMotionController_h
