// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/fling_stop_filter_interpreter.h"

namespace gestures {

FlingStopFilterInterpreter::FlingStopFilterInterpreter(PropRegistry* prop_reg,
                                                       Interpreter* next)
    : prev_finger_cnt_(0),
      finger_arrived_time_(0.0),
      fling_stop_timeout_(prop_reg, "Fling Stop Timeout", 0.08) {
  next_.reset(next);
}

Gesture* FlingStopFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                                   stime_t* timeout) {
  stime_t now = hwstate->timestamp;
  short finger_cnt = hwstate->finger_cnt;

  Gesture* result = next_->SyncInterpret(hwstate, timeout);
  if (!result) {
    result_ = Gesture();
    result = &result_;
  }
  bool finger_added = finger_cnt > prev_finger_cnt_;
  EvaluateFlingStop(finger_added, now, result);

  prev_timestamp_ = now;
  prev_finger_cnt_ = finger_cnt;
  return result;
}

void FlingStopFilterInterpreter::EvaluateFlingStop(bool finger_added,
                                                   stime_t now,
                                                   Gesture* result) {
  if (fling_stop_timeout_.val_ <= 0.0)
    return;

  if (finger_added && finger_arrived_time_ == 0.0) {
    // first finger added in a while. Note it.
    finger_arrived_time_ = now;
    return;
  }
  if (finger_arrived_time_ == 0.0) {
    // Nothing of interest happening.
    return;
  }
  // If we get to here, a finger was added some time in the recent past.
  if (result->type != kGestureTypeNull || now >
      finger_arrived_time_ + fling_stop_timeout_.val_) {
    // We should override the current gesture with stop-fling, or make a new
    // gesture if we don't have one yet.
    if (result->type == kGestureTypeScroll) {
      // We are scrolling. Stop the fling and start scrolling
      result->details.scroll.stop_fling = 1;
    } else {
      *result = Gesture(kGestureFling, prev_timestamp_,
                        now, 0.0, 0.0, GESTURES_FLING_TAP_DOWN);
    }
    finger_arrived_time_ = 0.0;
  }
}

Gesture* FlingStopFilterInterpreter::HandleTimer(stime_t now,
                                                 stime_t* timeout) {
  return next_->HandleTimer(now, timeout);
}

void FlingStopFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  next_->SetHardwareProperties(hwprops);
}

}  // namespace gestures
