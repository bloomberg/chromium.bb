// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/gamepad/GamepadHapticActuator.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"

namespace {

const char kGamepadHapticActuatorTypeVibration[] = "vibration";
const char kGamepadHapticActuatorTypeDualRumble[] = "dual-rumble";

const char kGamepadHapticsResultNotSupported[] = "not-supported";

}  // namespace

namespace blink {

// static
GamepadHapticActuator* GamepadHapticActuator::Create(int pad_index) {
  return new GamepadHapticActuator(
      pad_index, device::GamepadHapticActuatorType::kDualRumble);
}

GamepadHapticActuator::GamepadHapticActuator(
    int pad_index,
    device::GamepadHapticActuatorType type) {
  SetType(type);
}

GamepadHapticActuator::~GamepadHapticActuator() = default;

void GamepadHapticActuator::SetType(device::GamepadHapticActuatorType type) {
  switch (type) {
    case device::GamepadHapticActuatorType::kVibration:
      type_ = kGamepadHapticActuatorTypeVibration;
      break;
    case device::GamepadHapticActuatorType::kDualRumble:
      type_ = kGamepadHapticActuatorTypeDualRumble;
      break;
    default:
      NOTREACHED();
  }
}

ScriptPromise GamepadHapticActuator::playEffect(
    ScriptState* script_state,
    const String& type,
    const GamepadEffectParameters& params) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(kGamepadHapticsResultNotSupported);

  NOTIMPLEMENTED();
  return promise;
}

ScriptPromise GamepadHapticActuator::reset(ScriptState* script_state) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(kGamepadHapticsResultNotSupported);

  NOTIMPLEMENTED();
  return promise;
}

void GamepadHapticActuator::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
