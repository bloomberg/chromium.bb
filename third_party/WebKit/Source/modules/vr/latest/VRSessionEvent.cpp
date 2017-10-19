// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/latest/VRSessionEvent.h"

namespace blink {

VRSessionEvent::VRSessionEvent() {}

VRSessionEvent::VRSessionEvent(const AtomicString& type, VRSession* session)
    : Event(type, true, false), session_(session) {}

VRSessionEvent::VRSessionEvent(const AtomicString& type,
                               const VRSessionEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasSession())
    session_ = initializer.session();
}

VRSessionEvent::~VRSessionEvent() {}

const AtomicString& VRSessionEvent::InterfaceName() const {
  return EventNames::VRSessionEvent;
}

void VRSessionEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  Event::Trace(visitor);
}

}  // namespace blink
