// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_HAPTIC_ACTUATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_HAPTIC_ACTUATOR_H_

#include "device/gamepad/public/cpp/gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_effect_parameters.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class GamepadHapticActuator final : public ScriptWrappable,
                                    public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(GamepadHapticActuator);

 public:
  static GamepadHapticActuator* Create(ExecutionContext* context,
                                       int pad_index);

  GamepadHapticActuator(ExecutionContext* context,
                        int pad_index,
                        device::GamepadHapticActuatorType type);
  ~GamepadHapticActuator() override;

  const String& type() const { return type_; }
  void SetType(device::GamepadHapticActuatorType);

  ScriptPromise playEffect(ScriptState*,
                           const String&,
                           const GamepadEffectParameters*);

  ScriptPromise reset(ScriptState*);

  void Trace(blink::Visitor*) override;

 private:
  void OnPlayEffectCompleted(ScriptPromiseResolver*,
                             device::mojom::GamepadHapticsResult);
  void OnResetCompleted(ScriptPromiseResolver*,
                        device::mojom::GamepadHapticsResult);
  void ResetVibrationIfNotPreempted();

  int pad_index_;
  String type_;
  bool should_reset_ = false;
};

typedef HeapVector<Member<GamepadHapticActuator>> GamepadHapticActuatorVector;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_GAMEPAD_GAMEPAD_HAPTIC_ACTUATOR_H_
