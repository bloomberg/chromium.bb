// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/display_lock/before_activate_event.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

const AtomicString& BeforeActivateEvent::InterfaceName() const {
  return event_interface_names::kBeforeActivateEvent;
}

BeforeActivateEvent::BeforeActivateEvent(Element& activated_element)
    : Event(event_type_names::kBeforeactivate,
            Bubbles::kYes,
            Cancelable::kYes,
            ComposedMode::kScoped),
      activated_element_(activated_element) {}

void BeforeActivateEvent::Trace(Visitor* visitor) {
  visitor->Trace(activated_element_);
  Event::Trace(visitor);
}

}  // namespace blink
