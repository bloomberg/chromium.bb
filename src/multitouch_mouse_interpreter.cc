// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/mouse_interpreter.h"
#include "gestures/include/multitouch_mouse_interpreter.h"
#include "gestures/include/tracer.h"

namespace gestures {

MultitouchMouseInterpreter::MultitouchMouseInterpreter(
    PropRegistry* prop_reg,
    Tracer* tracer)
    : Interpreter(NULL, tracer, false),
      current_state_pos_(0) {
  InitName();
  memset(&states_, 0, sizeof(states_));
}

MultitouchMouseInterpreter::~MultitouchMouseInterpreter() {
  ResetStates(0);
}

Gesture* MultitouchMouseInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                       stime_t* timeout) {
  if (!State(0).fingers) {
    Err("Must call SetHardwareProperties() before interpreting anything.");
    return NULL;
  }

  // Record current HardwareState now.
  PushState(*hwstate);

  // Mouse events are given higher priority than multi-touch events.  The
  // interpreter first looks for mouse events.  If none of the mouse events
  // are found, the interpreter then looks for multi-touch events.
  result_.type = kGestureTypeNull;
  InterpretMouseEvent(State(-1), State(0), &result_);
  if (result_.type == kGestureTypeNull)
    InterpretMultitouchEvent(&result_);

  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

void MultitouchMouseInterpreter::SetHardwarePropertiesImpl(
    const HardwareProperties& hw_props) {
  hw_props_ = hw_props;
  ResetStates(hw_props_.max_finger_cnt);
}

void MultitouchMouseInterpreter::InterpretMultitouchEvent(Gesture* result) {
  // This function only interprets single finger scrolling gestures for now.

  // If fingers are changed, we are sure that single finger scrolling gestures
  // are not possible; so return now.
  if (!State(-1).SameFingersAs(State(0)))
    return;

  const HardwareState& curr = State(0);
  const HardwareState& prev = State(-1);

  float max_mag_sq = 0.0;  // square of max mag
  float dx = 0.0;
  float dy = 0.0;
  for (int i = 0; i < curr.finger_cnt; i++) {
    const FingerState* fs_curr = &curr.fingers[i];
    const FingerState* fs_prev = prev.GetFingerState(fs_curr->tracking_id);
    if (!fs_prev)
      continue;
    float local_dx = fs_curr->position_x - fs_prev->position_x;
    if (fs_curr->flags & GESTURES_FINGER_WARP_X_NON_MOVE)
      local_dx = 0.0;
    float local_dy = fs_curr->position_y - fs_prev->position_y;
    if (fs_curr->flags & GESTURES_FINGER_WARP_Y_NON_MOVE)
      local_dy = 0.0;
    float local_max_mag_sq = local_dx * local_dx + local_dy * local_dy;
    if (local_max_mag_sq > max_mag_sq) {
      max_mag_sq = local_max_mag_sq;
      dx = local_dx;
      dy = local_dy;
    }
  }

  if (max_mag_sq > 0) {
    *result = Gesture(kGestureScroll, prev.timestamp, curr.timestamp, dx, dy);
  }
}

void MultitouchMouseInterpreter::PushState(const HardwareState& state) {
  current_state_pos_ = (current_state_pos_ + 1) % arraysize(states_);
  states_[current_state_pos_].DeepCopy(state, hw_props_.max_finger_cnt);
}

void MultitouchMouseInterpreter::ResetStates(size_t max_finger_cnt) {
  for (size_t i = 0; i < arraysize(states_); i++) {
    delete[] states_[i].fingers;
    if (max_finger_cnt) {
      states_[i].fingers = new FingerState[max_finger_cnt];
      memset(states_[i].fingers, 0, sizeof(FingerState) * max_finger_cnt);
    } else {
      states_[i].fingers = NULL;
    }
  }
}

const HardwareState& MultitouchMouseInterpreter::State(int index) const {
  const int kNumStates = arraysize(states_);
  if (index < -kNumStates + 1 || 0 < index)
    Err("index out of range: %d", index);
  return states_[(current_state_pos_ + index + kNumStates) % kNumStates];
}

}  // namespace gestures
