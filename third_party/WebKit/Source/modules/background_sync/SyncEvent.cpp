// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_sync/SyncEvent.h"

namespace blink {

SyncEvent::SyncEvent(const AtomicString& type,
                     const String& tag,
                     bool last_chance,
                     WaitUntilObserver* observer)
    : ExtendableEvent(type, ExtendableEventInit(), observer),
      tag_(tag),
      last_chance_(last_chance) {}

SyncEvent::SyncEvent(const AtomicString& type, const SyncEventInit& init)
    : ExtendableEvent(type, init) {
  tag_ = init.tag();
  last_chance_ = init.lastChance();
}

SyncEvent::~SyncEvent() {}

const AtomicString& SyncEvent::InterfaceName() const {
  return EventNames::SyncEvent;
}

String SyncEvent::tag() {
  return tag_;
}

bool SyncEvent::lastChance() {
  return last_chance_;
}

void SyncEvent::Trace(blink::Visitor* visitor) {
  ExtendableEvent::Trace(visitor);
}

}  // namespace blink
