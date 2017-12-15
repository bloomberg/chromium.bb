// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/xr/XRDeviceEvent.h"

namespace blink {

XRDeviceEvent::XRDeviceEvent() {}

XRDeviceEvent::XRDeviceEvent(const AtomicString& type, XRDevice* device)
    : Event(type, true, false), device_(device) {}

XRDeviceEvent::XRDeviceEvent(const AtomicString& type,
                             const XRDeviceEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasDevice())
    device_ = initializer.device();
}

XRDeviceEvent::~XRDeviceEvent() {}

const AtomicString& XRDeviceEvent::InterfaceName() const {
  return EventNames::XRDeviceEvent;
}

void XRDeviceEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(device_);
  Event::Trace(visitor);
}

}  // namespace blink
