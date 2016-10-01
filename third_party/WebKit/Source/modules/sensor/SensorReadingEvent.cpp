// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/sensor/SensorReadingEvent.h"

namespace blink {

SensorReadingEvent::~SensorReadingEvent() = default;

SensorReadingEvent::SensorReadingEvent(const AtomicString& eventType,
                                       SensorReading* reading)
    : Event(eventType, false, false)  // Does not bubble and is not cancelable.
      ,
      m_reading(reading) {
  DCHECK(m_reading);
}

SensorReadingEvent::SensorReadingEvent(
    const AtomicString& eventType,
    const SensorReadingEventInit& initializer)
    : SensorReadingEvent(eventType, initializer.reading()) {}

const AtomicString& SensorReadingEvent::interfaceName() const {
  return EventNames::SensorReadingEvent;
}

DEFINE_TRACE(SensorReadingEvent) {
  Event::trace(visitor);
  visitor->trace(m_reading);
}

}  // namespace blink
