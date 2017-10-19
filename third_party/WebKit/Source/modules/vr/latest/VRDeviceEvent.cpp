// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRDeviceEvent.h"

namespace blink {

VRDeviceEvent::VRDeviceEvent() {}

VRDeviceEvent::VRDeviceEvent(const AtomicString& type, VRDevice* device)
    : Event(type, true, false), device_(device) {}

VRDeviceEvent::VRDeviceEvent(const AtomicString& type,
                             const VRDeviceEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasDevice())
    device_ = initializer.device();
}

VRDeviceEvent::~VRDeviceEvent() {}

const AtomicString& VRDeviceEvent::InterfaceName() const {
  return EventNames::VRDeviceEvent;
}

void VRDeviceEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  Event::Trace(visitor);
}

}  // namespace blink
