// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionCloseEvent.h"

#include "modules/presentation/PresentationConnectionCloseEventInit.h"

namespace blink {

PresentationConnectionCloseEvent::PresentationConnectionCloseEvent(
    const AtomicString& event_type,
    const String& reason,
    const String& message)
    : Event(event_type, false /* canBubble */, false /* cancelable */),
      reason_(reason),
      message_(message) {}

PresentationConnectionCloseEvent::PresentationConnectionCloseEvent(
    const AtomicString& event_type,
    const PresentationConnectionCloseEventInit& initializer)
    : Event(event_type, initializer),
      reason_(initializer.reason()),
      message_(initializer.message()) {}

const AtomicString& PresentationConnectionCloseEvent::InterfaceName() const {
  return EventNames::PresentationConnectionCloseEvent;
}

void PresentationConnectionCloseEvent::Trace(blink::Visitor* visitor) {
  Event::Trace(visitor);
}

}  // namespace blink
