// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/multitouch_mouse_interpreter.h"

#include <algorithm>

#include "gestures/include/mouse_interpreter.h"
#include "gestures/include/tracer.h"

namespace gestures {

MultitouchMouseInterpreter::MultitouchMouseInterpreter(
    PropRegistry* prop_reg,
    Tracer* tracer)
    : Interpreter(NULL, tracer, false),
      state_buffer_(2),
      scroll_buffer_(15),
      prev_gesture_type_(kGestureTypeNull),
      current_gesture_type_(kGestureTypeNull),
      scroll_manager_(prop_reg) {
  InitName();
}

Gesture* MultitouchMouseInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                       stime_t* timeout) {
  if (!state_buffer_.Get(0)->fingers) {
    Err("Must call SetHardwareProperties() before interpreting anything.");
    return NULL;
  }

  // Record current HardwareState now.
  state_buffer_.PushState(*hwstate);

  // TODO(clchiou): Remove palm and thumb.
  gs_fingers_.clear();
  size_t num_fingers = std::min(kMaxGesturingFingers,
                                (size_t)state_buffer_.Get(0)->finger_cnt);
  const FingerState* fs = state_buffer_.Get(0)->fingers;
  for (size_t i = 0; i < num_fingers; i++)
    gs_fingers_.insert(fs[i].tracking_id);

  // Mouse events are given higher priority than multi-touch events.  The
  // interpreter first looks for mouse events.  If none of the mouse events
  // are found, the interpreter then looks for multi-touch events.
  result_.type = kGestureTypeNull;
  extra_result_.type = kGestureTypeNull;

  InterpretMouseEvent(*state_buffer_.Get(1), *state_buffer_.Get(0), &result_);

  if (result_.type == kGestureTypeNull)
    InterpretMultitouchEvent(&result_);
  else
    InterpretMultitouchEvent(&extra_result_);

  if (extra_result_.type != kGestureTypeNull)
    result_.next = &extra_result_;

  prev_gs_fingers_ = gs_fingers_;
  prev_gesture_type_ = current_gesture_type_;
  prev_result_ = result_;
  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

void MultitouchMouseInterpreter::SetHardwarePropertiesImpl(
    const HardwareProperties& hw_props) {
  hw_props_ = hw_props;
  state_buffer_.Reset(hw_props_.max_finger_cnt);
}

void MultitouchMouseInterpreter::InterpretMultitouchEvent(Gesture* result) {
  // If a gesturing finger just left, do fling/lift
  if (current_gesture_type_ == kGestureTypeScroll &&
      AnyGesturingFingerLeft(*state_buffer_.Get(0),
                             prev_gs_fingers_)) {
    current_gesture_type_ = kGestureTypeFling;
    scroll_manager_.ComputeFling(state_buffer_, scroll_buffer_, result);
    if (result && result->type == kGestureTypeFling)
      result->details.fling.vx = 0.0;
  } else if (gs_fingers_.size() > 0) {
    // TODO(clchiou): For now, any finger movements are interpreted as
    // scrolling.
    current_gesture_type_ = kGestureTypeScroll;
    if (!scroll_manager_.ComputeScroll(state_buffer_,
                                       prev_gs_fingers_,
                                       gs_fingers_,
                                       prev_gesture_type_,
                                       prev_result_,
                                       result,
                                       &scroll_buffer_))
      return;
  }
  scroll_manager_.UpdateScrollEventBuffer(current_gesture_type_,
                                          &scroll_buffer_);
}

}  // namespace gestures
