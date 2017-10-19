// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadEvent_h
#define GamepadEvent_h

#include "modules/EventModules.h"
#include "modules/gamepad/Gamepad.h"
#include "modules/gamepad/GamepadEventInit.h"

namespace blink {

class GamepadEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GamepadEvent* Create(const AtomicString& type,
                              bool can_bubble,
                              bool cancelable,
                              Gamepad* gamepad) {
    return new GamepadEvent(type, can_bubble, cancelable, gamepad);
  }
  static GamepadEvent* Create(const AtomicString& type,
                              const GamepadEventInit& initializer) {
    return new GamepadEvent(type, initializer);
  }
  ~GamepadEvent() override;

  Gamepad* getGamepad() const { return gamepad_.Get(); }

  const AtomicString& InterfaceName() const override;

  virtual void Trace(blink::Visitor*);

 private:
  GamepadEvent(const AtomicString& type,
               bool can_bubble,
               bool cancelable,
               Gamepad*);
  GamepadEvent(const AtomicString&, const GamepadEventInit&);

  Member<Gamepad> gamepad_;
};

}  // namespace blink

#endif  // GamepadEvent_h
