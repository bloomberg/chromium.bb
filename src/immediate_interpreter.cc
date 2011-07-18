// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/immediate_interpreter.h"

#include <algorithm>

#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"

using std::min;

namespace gestures {

namespace {

// TODO(adlr): make these configurable:
const int kPalmPressure = 100;

}  // namespace {}

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
    Log("Must call SetHardwareProperties() before Push().");
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

void ImmediateInterpreter::ResetSameFingersState(stime_t now) {
  palm_.clear();
  pending_palm_.clear();
  pointing_.clear();
  changed_time_ = now;
}

void ImmediateInterpreter::UpdatePalmState(const HardwareState& hwstate) {
  for (short i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    bool prev_palm = palm_.find(fs.tracking_id) != palm_.end();
    bool prev_pointing = pointing_.find(fs.tracking_id) != pointing_.end();

    // Lock onto palm permanently.
    if (prev_palm)
      continue;

    // TODO(adlr): handle low-pressure palms at edge of pad by inserting them
    // into pending_palm_
    if (fs.pressure >= kPalmPressure) {
      palm_.insert(fs.tracking_id);
      pointing_.erase(fs.tracking_id);
      pending_palm_.erase(fs.tracking_id);
      continue;
    }
    if (prev_pointing)
      continue;
    pointing_.insert(fs.tracking_id);
  }
}

namespace {
struct GetGesturingFingersCompare {
  // Returns true if finger_a is strictly closer to keyboard than finger_b
  bool operator()(const FingerState* finger_a, const FingerState* finger_b) {
    return finger_a->position_y < finger_b->position_y;
  }
};
}  // namespace {}

set<short, kMaxGesturingFingers> ImmediateInterpreter::GetGesturingFingers(
    const HardwareState& hwstate) const {
  const size_t kMaxSize = 2;  // We support up to 2 finger gestures
  if (pointing_.size() <= kMaxSize ||
      (hw_props_.supports_t5r2 && pointing_.size() > 2))
    return pointing_;

  const FingerState* fs[hwstate.finger_cnt];
  for (size_t i = 0; i < hwstate.finger_cnt; ++i)
    fs[i] = &hwstate.fingers[i];

  GetGesturingFingersCompare compare;
  // Pull the kMaxSize FingerStates w/ the lowest position_y to the
  // front of fs[].
  std::partial_sort(fs, fs + kMaxSize, fs + hwstate.finger_cnt, compare);
  set<short, kMaxGesturingFingers> ret;
  ret.insert(fs[0]->tracking_id);
  ret.insert(fs[1]->tracking_id);
  return ret;
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
