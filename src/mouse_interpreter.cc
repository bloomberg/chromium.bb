// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/mouse_interpreter.h"
#include "gestures/include/tracer.h"

#include <base/memory/scoped_ptr.h>

namespace gestures {

MouseInterpreter::MouseInterpreter(PropRegistry* prop_reg, Tracer* tracer)
    : Interpreter(NULL, tracer, false) {
  InitName();
  memset(&prev_state_, 0, sizeof(prev_state_));
}

Gesture* MouseInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                             stime_t* timeout) {
  const unsigned buttons[3] = {
    GESTURES_BUTTON_LEFT,
    GESTURES_BUTTON_MIDDLE,
    GESTURES_BUTTON_RIGHT,
  };
  unsigned down = 0, up = 0;

  for (unsigned i = 0; i < arraysize(buttons); i++) {
    if (!(prev_state_.buttons_down & buttons[i]) &&
        (hwstate->buttons_down & buttons[i]))
      down |= buttons[i];
    if ((prev_state_.buttons_down & buttons[i]) &&
        !(hwstate->buttons_down & buttons[i]))
      up |= buttons[i];
  }

  if (down || up) {
    result_ = Gesture(kGestureButtonsChange,
                      prev_state_.timestamp,
                      hwstate->timestamp,
                      down,
                      up);
  } else if (prev_state_.rel_hwheel || prev_state_.rel_wheel) {
    result_ = Gesture(kGestureScroll,
                      prev_state_.timestamp,
                      hwstate->timestamp,
                      prev_state_.rel_hwheel,
                      prev_state_.rel_wheel);
  } else if (prev_state_.rel_x || prev_state_.rel_y) {
    result_ = Gesture(kGestureMove,
                      prev_state_.timestamp,
                      hwstate->timestamp,
                      prev_state_.rel_x,
                      prev_state_.rel_y);
  } else {
    result_.type = kGestureTypeNull;
  }

  // Pass max_finger_cnt = 0 to DeepCopy() since we don't care fingers and
  // did not allocate any space for fingers.
  prev_state_.DeepCopy(*hwstate, 0);

  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

}  // namespace gestures
