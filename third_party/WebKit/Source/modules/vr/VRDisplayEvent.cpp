// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/vr/VRDisplayEvent.h"

namespace blink {

VRDisplayEvent::VRDisplayEvent() {}

VRDisplayEvent::VRDisplayEvent(const AtomicString& type,
                               bool canBubble,
                               bool cancelable,
                               VRDisplay* display,
                               String reason)
    : Event(type, canBubble, cancelable),
      m_display(display),
      m_reason(reason) {}

VRDisplayEvent::VRDisplayEvent(const AtomicString& type,
                               const VRDisplayEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasDisplay())
    m_display = initializer.display();

  if (initializer.hasReason())
    m_reason = initializer.reason();
}

VRDisplayEvent::~VRDisplayEvent() {}

const AtomicString& VRDisplayEvent::interfaceName() const {
  return EventNames::VRDisplayEvent;
}

DEFINE_TRACE(VRDisplayEvent) {
  visitor->trace(m_display);
  Event::trace(visitor);
}

}  // namespace blink
