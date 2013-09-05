// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/mouse_interpreter.h"

#include <math.h>

#include "gestures/include/tracer.h"

namespace gestures {

MouseInterpreter::MouseInterpreter(PropRegistry* prop_reg, Tracer* tracer)
    : Interpreter(NULL, tracer, false),
      scroll_max_allowed_input_speed_(prop_reg,
                                      "Mouse Scroll Max Input Speed",
                                      177.0,
                                      this) {
  InitName();
  memset(&prev_state_, 0, sizeof(prev_state_));

  // Scroll acceleration curve coefficients. See the definition for more
  // details on how to generate them.
  scroll_accel_curve_[0] = 1.5937e+01;
  scroll_accel_curve_[1] = 2.5547e-01;
  scroll_accel_curve_[2] = 1.9727e-02;
  scroll_accel_curve_[3] = 1.6313e-04;
  scroll_accel_curve_[4] = -1.0012e-06;
}

void MouseInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                         stime_t* timeout) {
  // Interpret mouse events in the order of pointer moves, scroll wheels and
  // button clicks.
  InterpretMouseMotionEvent(prev_state_, *hwstate);
  // Note that unlike touchpad scrolls, we interpret and send separate events
  // for horizontal/vertical mouse wheel scrolls. This is partly to match what
  // the xf86-input-evdev driver does and is partly because not all code in
  // Chrome honors MouseWheelEvent that has both X and Y offsets.
  InterpretScrollWheelEvent(*hwstate, true);
  InterpretScrollWheelEvent(*hwstate, false);
  InterpretMouseButtonEvent(prev_state_, *hwstate);

  // Pass max_finger_cnt = 0 to DeepCopy() since we don't care fingers and
  // did not allocate any space for fingers.
  prev_state_.DeepCopy(*hwstate, 0);
}

double MouseInterpreter::ComputeScroll(double input_speed) {
  double result = 0.0;
  double term = 1.0;
  double allowed_speed = fabs(input_speed);
  if (allowed_speed > scroll_max_allowed_input_speed_.val_)
    allowed_speed = scroll_max_allowed_input_speed_.val_;

  // Compute the accelerated scroll value.
  for (size_t i = 0; i < arraysize(scroll_accel_curve_); i++) {
    result += term * scroll_accel_curve_[i];
    term *= allowed_speed;
  }
  if (input_speed < 0)
    result = -result;
  return result;
}

void MouseInterpreter::InterpretScrollWheelEvent(const HardwareState& hwstate,
                                                 bool is_vertical) {
  // Vertical wheel or horizontal wheel.
  float current_wheel_value = hwstate.rel_hwheel;
  WheelRecord* last_wheel_record = &last_hwheel_;
  if (is_vertical) {
    current_wheel_value = hwstate.rel_wheel;
    last_wheel_record = &last_wheel_;
  }

  // Check if the wheel is scrolled.
  if (current_wheel_value) {
    stime_t start_time, end_time = hwstate.timestamp;
    // Check if this scroll is in same direction as previous scroll event.
    if ((current_wheel_value < 0 && last_wheel_record->value < 0) ||
        (current_wheel_value > 0 && last_wheel_record->value > 0)) {
      start_time = last_wheel_record->timestamp;
    } else {
      start_time = end_time;
    }

    // If start_time == end_time, compute click_speed using dt = 1 second.
    stime_t dt = (end_time - start_time) ?: 1.0;
    float offset = ComputeScroll(current_wheel_value / dt);
    last_wheel_record->timestamp = hwstate.timestamp;
    last_wheel_record->value = current_wheel_value;

    if (is_vertical) {
      // For historical reasons the vertical wheel (REL_WHEEL) is inverted
      ProduceGesture(Gesture(kGestureScroll, start_time, end_time, 0,
                             -offset));
    } else {
      ProduceGesture(Gesture(kGestureScroll, start_time, end_time, offset, 0));
    }
  }
}

void MouseInterpreter::InterpretMouseButtonEvent(
    const HardwareState& prev_state, const HardwareState& hwstate) {
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
    ProduceGesture(Gesture(kGestureButtonsChange,
                           prev_state.timestamp,
                           hwstate.timestamp,
                           down,
                           up));
  }
}

void MouseInterpreter::InterpretMouseMotionEvent(
    const HardwareState& prev_state,
    const HardwareState& hwstate) {
  if (hwstate.rel_x || hwstate.rel_y) {
    ProduceGesture(Gesture(kGestureMove,
                           prev_state.timestamp,
                           hwstate.timestamp,
                           hwstate.rel_x,
                           hwstate.rel_y));
  }
}

}  // namespace gestures
