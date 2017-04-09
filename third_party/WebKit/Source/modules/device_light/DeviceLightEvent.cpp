// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/device_light/DeviceLightEvent.h"

namespace blink {

DeviceLightEvent::~DeviceLightEvent() {}

DeviceLightEvent::DeviceLightEvent(const AtomicString& event_type, double value)
    // The DeviceLightEvent bubbles but is not cancelable.
    : Event(event_type, true, false), value_(value) {}

DeviceLightEvent::DeviceLightEvent(const AtomicString& event_type,
                                   const DeviceLightEventInit& initializer)
    : Event(event_type, initializer),
      value_(initializer.hasValue() ? initializer.value()
                                    : std::numeric_limits<double>::infinity()) {
  SetCanBubble(true);
}

const AtomicString& DeviceLightEvent::InterfaceName() const {
  return EventNames::DeviceLightEvent;
}

}  // namespace blink
