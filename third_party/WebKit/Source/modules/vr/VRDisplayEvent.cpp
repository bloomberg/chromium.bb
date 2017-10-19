// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplayEvent.h"

namespace blink {

namespace {

String VRDisplayEventReasonToString(
    device::mojom::blink::VRDisplayEventReason reason) {
  switch (reason) {
    case device::mojom::blink::VRDisplayEventReason::NONE:
      return "";
    case device::mojom::blink::VRDisplayEventReason::NAVIGATION:
      return "navigation";
    case device::mojom::blink::VRDisplayEventReason::MOUNTED:
      return "mounted";
    case device::mojom::blink::VRDisplayEventReason::UNMOUNTED:
      return "unmounted";
  }

  NOTREACHED();
  return "";
}

}  // namespace

VRDisplayEvent* VRDisplayEvent::Create(
    const AtomicString& type,
    bool can_bubble,
    bool cancelable,
    VRDisplay* display,
    device::mojom::blink::VRDisplayEventReason reason) {
  return new VRDisplayEvent(type, can_bubble, cancelable, display,
                            VRDisplayEventReasonToString(reason));
}

VRDisplayEvent::VRDisplayEvent() {}

VRDisplayEvent::VRDisplayEvent(const AtomicString& type,
                               bool can_bubble,
                               bool cancelable,
                               VRDisplay* display,
                               String reason)
    : Event(type, can_bubble, cancelable), display_(display), reason_(reason) {}

VRDisplayEvent::VRDisplayEvent(const AtomicString& type,
                               const VRDisplayEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasDisplay())
    display_ = initializer.display();

  if (initializer.hasReason())
    reason_ = initializer.reason();
}

VRDisplayEvent::~VRDisplayEvent() {}

const AtomicString& VRDisplayEvent::InterfaceName() const {
  return EventNames::VRDisplayEvent;
}

void VRDisplayEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(display_);
  Event::Trace(visitor);
}

}  // namespace blink
