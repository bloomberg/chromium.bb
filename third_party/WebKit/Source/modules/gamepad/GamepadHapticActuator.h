// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadHapticActuator_h
#define GamepadHapticActuator_h

#include "bindings/core/v8/ScriptPromise.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "modules/gamepad/GamepadEffectParameters.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class GamepadHapticActuator final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GamepadHapticActuator* Create(int pad_index);
  ~GamepadHapticActuator();

  const String& type() const { return type_; }
  void SetType(device::GamepadHapticActuatorType);

  ScriptPromise playEffect(ScriptState*,
                           const String&,
                           const GamepadEffectParameters&);

  ScriptPromise reset(ScriptState*);

  void Trace(blink::Visitor*);

 private:
  GamepadHapticActuator(int pad_index, device::GamepadHapticActuatorType);

  String type_;
};

typedef HeapVector<Member<GamepadHapticActuator>> GamepadHapticActuatorVector;

}  // namespace blink

#endif  // GamepadHapticActuator_h
