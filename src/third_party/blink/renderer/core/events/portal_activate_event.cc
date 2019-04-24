// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/portal_activate_event.h"

#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

PortalActivateEvent* PortalActivateEvent::Create() {
  return MakeGarbageCollected<PortalActivateEvent>();
}

PortalActivateEvent::PortalActivateEvent()
    : Event(event_type_names::kPortalactivate,
            Bubbles::kNo,
            Cancelable::kNo,
            CurrentTimeTicks()) {}

PortalActivateEvent::~PortalActivateEvent() = default;

void PortalActivateEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

const AtomicString& PortalActivateEvent::InterfaceName() const {
  return event_type_names::kPortalactivate;
}

}  // namespace blink
