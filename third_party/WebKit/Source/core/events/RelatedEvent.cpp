// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/RelatedEvent.h"

namespace blink {

RelatedEvent::~RelatedEvent() {}

RelatedEvent* RelatedEvent::Create(const AtomicString& type,
                                   bool can_bubble,
                                   bool cancelable,
                                   EventTarget* related_target) {
  return new RelatedEvent(type, can_bubble, cancelable, related_target);
}

RelatedEvent* RelatedEvent::Create(const AtomicString& type,
                                   const RelatedEventInit& initializer) {
  return new RelatedEvent(type, initializer);
}

RelatedEvent::RelatedEvent(const AtomicString& type,
                           bool can_bubble,
                           bool cancelable,
                           EventTarget* related_target)
    : Event(type, can_bubble, cancelable), related_target_(related_target) {}

RelatedEvent::RelatedEvent(const AtomicString& event_type,
                           const RelatedEventInit& initializer)
    : Event(event_type, initializer) {
  if (initializer.hasRelatedTarget())
    related_target_ = initializer.relatedTarget();
}

DEFINE_TRACE(RelatedEvent) {
  visitor->Trace(related_target_);
  Event::Trace(visitor);
}

}  // namespace blink
