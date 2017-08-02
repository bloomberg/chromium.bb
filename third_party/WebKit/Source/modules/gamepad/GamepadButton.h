// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GamepadButton_h
#define GamepadButton_h

#include "device/gamepad/public/cpp/gamepad.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {

class GamepadButton final : public GarbageCollected<GamepadButton>,
                            public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GamepadButton* Create();

  double value() const { return value_; }
  void SetValue(double val) { value_ = val; }

  bool pressed() const { return pressed_; }
  void SetPressed(bool val) { pressed_ = val; }

  bool touched() const { return touched_; }
  void SetTouched(bool val) { touched_ = val; }

  bool IsEqual(const device::GamepadButton&) const;
  void UpdateValuesFrom(const device::GamepadButton&);

  DEFINE_INLINE_TRACE() {}

 private:
  GamepadButton();
  double value_;
  bool pressed_;
  bool touched_;
};

typedef HeapVector<Member<GamepadButton>> GamepadButtonVector;

}  // namespace blink

#endif  // GamepadButton_h
