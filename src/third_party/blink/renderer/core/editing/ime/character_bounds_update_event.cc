// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/ime/character_bounds_update_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_character_bounds_update_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

CharacterBoundsUpdateEvent::CharacterBoundsUpdateEvent(
    const CharacterBoundsUpdateEventInit* dict) {
  if (dict->hasRangeStart())
    range_start_ = dict->rangeStart();

  if (dict->hasRangeEnd())
    range_end_ = dict->rangeEnd();
}

CharacterBoundsUpdateEvent::CharacterBoundsUpdateEvent(uint32_t range_start,
                                                       uint32_t range_end)
    : Event(event_type_names::kCharacterboundsupdate,
            Bubbles::kNo,
            Cancelable::kNo,
            ComposedMode::kComposed,
            base::TimeTicks::Now()),
      range_start_(range_start),
      range_end_(range_end) {}

CharacterBoundsUpdateEvent* CharacterBoundsUpdateEvent::Create(
    const CharacterBoundsUpdateEventInit* dict) {
  return MakeGarbageCollected<CharacterBoundsUpdateEvent>(dict);
}

CharacterBoundsUpdateEvent::~CharacterBoundsUpdateEvent() = default;

uint32_t CharacterBoundsUpdateEvent::rangeStart() const {
  return range_start_;
}

uint32_t CharacterBoundsUpdateEvent::rangeEnd() const {
  return range_end_;
}

const AtomicString& CharacterBoundsUpdateEvent::InterfaceName() const {
  return event_interface_names::kTextUpdateEvent;
}

}  // namespace blink
