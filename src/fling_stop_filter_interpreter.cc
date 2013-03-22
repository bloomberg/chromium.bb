// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/fling_stop_filter_interpreter.h"

#include "gestures/include/util.h"

namespace gestures {

FlingStopFilterInterpreter::FlingStopFilterInterpreter(PropRegistry* prop_reg,
                                                       Interpreter* next,
                                                       Tracer* tracer)
    : FilterInterpreter(NULL, next, tracer, false),
      prev_touch_cnt_(0),
      fling_stop_deadline_(0.0),
      next_timer_deadline_(0.0),
      fling_stop_timeout_(prop_reg, "Fling Stop Timeout", 0.08) {
  InitName();
}

Gesture* FlingStopFilterInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                       stime_t* timeout) {
  UpdateFlingStopDeadline(*hwstate);

  stime_t next_timeout = -1.0;
  Gesture* result = next_->SyncInterpret(hwstate, &next_timeout);
  if (fling_stop_deadline_ != 0.0 &&
      result && result->type == kGestureTypeScroll) {
    // Don't need to stop fling, since the user is scrolling
    result->details.scroll.stop_fling = 1;
    fling_stop_deadline_ = 0.0;
  } else if (fling_stop_deadline_ != 0.0 &&
             (hwstate->timestamp > fling_stop_deadline_ ||
              (result && result->type == kGestureTypeButtonsChange))) {
    // Try to sub in a fling-stop for this gesture, as we should have received
    // a callback by now
    result_ = Gesture(kGestureFling, prev_timestamp_,
                      hwstate->timestamp, 0.0, 0.0,
                      GESTURES_FLING_TAP_DOWN);
    AppendGesture(&result_, result);
    result = &result_;
    fling_stop_deadline_ = 0.0;
  }
  *timeout = SetNextDeadlineAndReturnTimeoutVal(hwstate->timestamp,
                                                next_timeout);
  return result;
}

void FlingStopFilterInterpreter::UpdateFlingStopDeadline(
    const HardwareState& hwstate) {
  if (fling_stop_timeout_.val_ <= 0.0)
    return;

  stime_t now = hwstate.timestamp;
  bool finger_added = hwstate.touch_cnt > prev_touch_cnt_;

  if (finger_added && fling_stop_deadline_ == 0.0) {
    // first finger added in a while. Note it.
    fling_stop_deadline_ = now + fling_stop_timeout_.val_;
    return;
  }

  prev_timestamp_ = now;
  prev_touch_cnt_ = hwstate.touch_cnt;
}

stime_t FlingStopFilterInterpreter::SetNextDeadlineAndReturnTimeoutVal(
    stime_t now,
    stime_t next_timeout) {
  next_timer_deadline_ = next_timeout >= 0.0 ? now + next_timeout : 0.0;
  stime_t local_timeout = fling_stop_deadline_ == 0.0 ? -1.0 :
      std::max(fling_stop_deadline_ - now, 0.0);

  if (next_timeout < 0.0 && local_timeout < 0.0)
    return -1.0;
  if (next_timeout < 0.0)
    return local_timeout;
  if (local_timeout < 0.0)
    return next_timeout;
  return std::min(next_timeout, local_timeout);
}

Gesture* FlingStopFilterInterpreter::HandleTimerImpl(stime_t now,
                                                     stime_t* timeout) {
  bool call_next = false;
  if (fling_stop_deadline_ > 0.0 && next_timer_deadline_ > 0.0)
    call_next = fling_stop_deadline_ > next_timer_deadline_;
  else
    call_next = next_timer_deadline_ > 0.0;

  if (!call_next) {
    if (fling_stop_deadline_ > now) {
      Err("Spurious callback. now: %f, fs deadline: %f, next deadline: %f",
          now, fling_stop_deadline_, next_timer_deadline_);
      return NULL;
    }
    fling_stop_deadline_ = 0.0;
    // Easy, just return fling stop
    result_ = Gesture(kGestureFling, prev_timestamp_,
                      now, 0.0, 0.0,
                      GESTURES_FLING_TAP_DOWN);
    stime_t next_timeout = next_timer_deadline_ == 0.0 ? -1.0 :
        std::max(0.0, next_timer_deadline_ - now);
    *timeout = SetNextDeadlineAndReturnTimeoutVal(now, next_timeout);
    return &result_;
  }
  // Call next_
  if (next_timer_deadline_ > now) {
    Err("Spurious callback. now: %f, fs deadline: %f, next deadline: %f",
        now, fling_stop_deadline_, next_timer_deadline_);
    return NULL;
  }
  stime_t next_timeout = -1.0;
  Gesture* ret = next_->HandleTimer(now, &next_timeout);
  *timeout = SetNextDeadlineAndReturnTimeoutVal(now, next_timeout);
  return ret;
}

}  // namespace gestures
