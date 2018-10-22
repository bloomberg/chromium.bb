// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/invisible_dom/activate_invisible_event.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/event_names.h"

namespace blink {

const AtomicString& ActivateInvisibleEvent::InterfaceName() const {
  return EventNames::ActivateInvisibleEvent;
}

bool ActivateInvisibleEvent::IsActivateInvisibleEvent() const {
  return true;
}

ActivateInvisibleEvent::ActivateInvisibleEvent(Element* activated_element)
    : Event(EventTypeNames::activateinvisible,
            Bubbles::kYes,
            Cancelable::kYes,
            ComposedMode::kScoped),
      activated_element_(activated_element) {}

void ActivateInvisibleEvent::Trace(Visitor* visitor) {
  visitor->Trace(activated_element_);
  Event::Trace(visitor);
}

}  // namespace blink
