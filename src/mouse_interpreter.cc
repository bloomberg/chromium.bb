// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/mouse_interpreter.h"
#include "gestures/include/tracer.h"

namespace gestures {

MouseInterpreter::MouseInterpreter(PropRegistry* prop_reg, Tracer* tracer)
    : Interpreter(NULL, tracer, false) {
  InitName();
  memset(&prev_state_, 0, sizeof(prev_state_));
}

Gesture* MouseInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                             stime_t* timeout) {
  result_.type = kGestureTypeNull;
  InterpretMouseEvent(prev_state_, *hwstate, &result_);

  // Pass max_finger_cnt = 0 to DeepCopy() since we don't care fingers and
  // did not allocate any space for fingers.
  prev_state_.DeepCopy(*hwstate, 0);

  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

void InterpretMouseEvent(const HardwareState& prev_state,
                         const HardwareState& hwstate,
                         Gesture* result) {
  const unsigned buttons[3] = {
    GESTURES_BUTTON_LEFT,
    GESTURES_BUTTON_MIDDLE,
    GESTURES_BUTTON_RIGHT,
  };
  unsigned down = 0, up = 0;

  for (unsigned i = 0; i < arraysize(buttons); i++) {
    if (!(prev_state.buttons_down & buttons[i]) &&
        (hwstate.buttons_down & buttons[i]))
      down |= buttons[i];
    if ((prev_state.buttons_down & buttons[i]) &&
        !(hwstate.buttons_down & buttons[i]))
      up |= buttons[i];
  }

  if (down || up) {
    *result = Gesture(kGestureButtonsChange,
                      prev_state.timestamp,
                      hwstate.timestamp,
                      down,
                      up);
  } else if (hwstate.rel_hwheel || hwstate.rel_wheel) {
    *result = Gesture(kGestureScroll,
                      prev_state.timestamp,
                      hwstate.timestamp,
                      -hwstate.rel_hwheel,
                      -hwstate.rel_wheel);
  } else if (hwstate.rel_x || hwstate.rel_y) {
    *result = Gesture(kGestureMove,
                      prev_state.timestamp,
                      hwstate.timestamp,
                      hwstate.rel_x,
                      hwstate.rel_y);
  }
}

}  // namespace gestures
