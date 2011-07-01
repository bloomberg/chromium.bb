// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/immediate_interpreter.h"

#include <algorithm>

#include <base/logging.h>

#include "gestures/include/gestures.h"

using std::min;

namespace gestures {

ImmediateInterpreter::ImmediateInterpreter() {
  prev_state_.fingers = NULL;
}

ImmediateInterpreter::~ImmediateInterpreter() {
  if (prev_state_.fingers) {
    free(prev_state_.fingers);
    prev_state_.fingers = NULL;
  }
}

Gesture* ImmediateInterpreter::SyncInterpret(HardwareState* hwstate) {
  if (!prev_state_.fingers) {
    LOG(ERROR) << "Must call SetHardwareProperties() before Push().";
    return 0;
  }

  bool have_gesture = false;

  if (prev_state_.finger_cnt && prev_state_.fingers[0].pressure &&
      hwstate->finger_cnt && hwstate->fingers[0].pressure) {
    // This and the previous frame has pressure, so create a gesture

    // For now, simple: only one possible gesture
    result_ = Gesture(kGestureMove,
                      prev_state_.timestamp,
                      hwstate->timestamp,
                      hwstate->fingers[0].position_x -
                      prev_state_.fingers[0].position_x,
                      hwstate->fingers[0].position_y -
                      prev_state_.fingers[0].position_y);
    have_gesture = true;
  }
  SetPrevState(*hwstate);
  return have_gesture ? &result_ : NULL;
}

// For now, require fingers to be in the same slots
bool ImmediateInterpreter::SameFingers(const HardwareState& hwstate) const {
  if (hwstate.finger_cnt != prev_state_.finger_cnt)
    return false;
  for (int i = 0; i < hwstate.finger_cnt; ++i) {
    if (hwstate.fingers[i].tracking_id != prev_state_.fingers[i].tracking_id)
      return false;
  }
  return true;
}

void ImmediateInterpreter::SetPrevState(const HardwareState& hwstate) {
  prev_state_.timestamp = hwstate.timestamp;
  prev_state_.finger_cnt = min(hwstate.finger_cnt, hw_props_.max_finger_cnt);
  memcpy(prev_state_.fingers,
         hwstate.fingers,
         prev_state_.finger_cnt * sizeof(FingerState));
}

void ImmediateInterpreter::SetHardwareProperties(
    const HardwareProperties& hw_props) {
  hw_props_ = hw_props;
  if (prev_state_.fingers) {
    free(prev_state_.fingers);
    prev_state_.fingers = NULL;
  }
  prev_state_.fingers =
      reinterpret_cast<FingerState*>(calloc(hw_props_.max_finger_cnt,
                                            sizeof(FingerState)));
}

}  // namespace gestures
