// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceLightEvent_h
#define DeviceLightEvent_h

#include "modules/EventModules.h"
#include "modules/device_light/DeviceLightEventInit.h"
#include "platform/heap/Handle.h"

namespace blink {

class DeviceLightEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~DeviceLightEvent() override;

  static DeviceLightEvent* Create(const AtomicString& event_type,
                                  double value) {
    return new DeviceLightEvent(event_type, value);
  }
  static DeviceLightEvent* Create(const AtomicString& event_type,
                                  const DeviceLightEventInit& initializer) {
    return new DeviceLightEvent(event_type, initializer);
  }

  double value() const { return value_; }

  const AtomicString& InterfaceName() const override;

 private:
  DeviceLightEvent(const AtomicString& event_type, double value);
  DeviceLightEvent(const AtomicString& event_type,
                   const DeviceLightEventInit& initializer);

  double value_;
};

DEFINE_TYPE_CASTS(DeviceLightEvent,
                  Event,
                  event,
                  event->InterfaceName() == EventNames::DeviceLightEvent,
                  event.InterfaceName() == EventNames::DeviceLightEvent);

}  // namespace blink

#endif  // DeviceLightEvent_h
