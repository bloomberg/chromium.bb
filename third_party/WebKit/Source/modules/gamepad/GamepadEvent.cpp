// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/gamepad/GamepadEvent.h"

namespace blink {

GamepadEvent::GamepadEvent(const AtomicString& type,
                           bool can_bubble,
                           bool cancelable,
                           Gamepad* gamepad)
    : Event(type, can_bubble, cancelable), gamepad_(gamepad) {}

GamepadEvent::GamepadEvent(const AtomicString& type,
                           const GamepadEventInit& initializer)
    : Event(type, initializer) {
  if (initializer.hasGamepad())
    gamepad_ = initializer.gamepad();
}

GamepadEvent::~GamepadEvent() {}

const AtomicString& GamepadEvent::InterfaceName() const {
  return EventNames::GamepadEvent;
}

void GamepadEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(gamepad_);
  Event::Trace(visitor);
}

}  // namespace blink
