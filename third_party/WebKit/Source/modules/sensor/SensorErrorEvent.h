// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorErrorEvent_h
#define SensorErrorEvent_h

#include "core/dom/DOMException.h"
#include "modules/EventModules.h"
#include "modules/sensor/SensorErrorEventInit.h"
#include "platform/heap/Handle.h"

namespace blink {

class SensorErrorEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static SensorErrorEvent* Create(const AtomicString& event_type,
                                  DOMException* error) {
    return new SensorErrorEvent(event_type, error);
  }

  static SensorErrorEvent* Create(const AtomicString& event_type,
                                  const SensorErrorEventInit& initializer) {
    return new SensorErrorEvent(event_type, initializer);
  }

  ~SensorErrorEvent() override;

  virtual void Trace(blink::Visitor*);

  const AtomicString& InterfaceName() const override;

  DOMException* error() { return error_; }

 private:
  SensorErrorEvent(const AtomicString& event_type, DOMException* error);
  SensorErrorEvent(const AtomicString& event_type,
                   const SensorErrorEventInit& initializer);

  Member<DOMException> error_;
};

DEFINE_TYPE_CASTS(SensorErrorEvent,
                  Event,
                  event,
                  event->InterfaceName() == EventNames::SensorErrorEvent,
                  event.InterfaceName() == EventNames::SensorErrorEvent);

}  // namepsace blink

#endif  // SensorErrorEvent_h
