// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/stuck_button_inhibitor_filter_interpreter.h"

#include "gestures/include/logging.h"
#include "gestures/include/tracer.h"

namespace gestures {

StuckButtonInhibitorFilterInterpreter::StuckButtonInhibitorFilterInterpreter(
    Interpreter* next, Tracer* tracer)
    : FilterInterpreter(NULL, next, tracer, false),
      incoming_button_must_be_up_(true),
      sent_buttons_down_(0),
      next_expects_timer_(false) {
  InitName();
}

Gesture* StuckButtonInhibitorFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate, stime_t* timeout) {
  HandleHardwareState(*hwstate);
  stime_t next_timeout = -1.0;
  Gesture* result = next_->SyncInterpret(hwstate, &next_timeout);
  return HandleGesture(result, next_timeout, timeout);;
}

Gesture* StuckButtonInhibitorFilterInterpreter::HandleTimerImpl(
    stime_t now, stime_t* timeout) {
  if (!next_expects_timer_) {
    if (!sent_buttons_down_) {
      Err("Bug: got callback, but no gesture to send.");
      return NULL;
    } else {
      Err("Mouse button seems stuck down. Sending button-up.");
      result_ = Gesture(kGestureButtonsChange,
                        now, now, 0, sent_buttons_down_);
      sent_buttons_down_ = 0;
      return &result_;
    }
  }
  stime_t next_timeout = -1.0;
  Gesture* result = next_->HandleTimer(now, &next_timeout);
  return HandleGesture(result, next_timeout, timeout);;
}

void StuckButtonInhibitorFilterInterpreter::HandleHardwareState(
    const HardwareState& hwstate) {
  incoming_button_must_be_up_ =
      hwstate.touch_cnt == 0 && hwstate.buttons_down == 0;
}

Gesture* StuckButtonInhibitorFilterInterpreter::HandleGesture(
    Gesture* gesture, stime_t next_timeout, stime_t* timeout) {
  if (gesture && gesture->next)
    gesture->next = HandleGesture(gesture->next, next_timeout, timeout);

  if (gesture && gesture->type == kGestureTypeButtonsChange) {
    // process buttons going down
    if (sent_buttons_down_ & gesture->details.buttons.down) {
      Err("Odd. Gesture is sending buttons down that are already down: "
          "Existing down: %d. New down: %d. fixing.",
          sent_buttons_down_, gesture->details.buttons.down);
      gesture->details.buttons.down &= ~sent_buttons_down_;
    }
    sent_buttons_down_ |= gesture->details.buttons.down;
    if ((~sent_buttons_down_) & gesture->details.buttons.up) {
      Err("Odd. Gesture is sending buttons up for buttons we didn't send down: "
          "Existing down: %d. New up: %d.",
          sent_buttons_down_, gesture->details.buttons.up);
      gesture->details.buttons.up &= sent_buttons_down_;
    }
    sent_buttons_down_ &= ~gesture->details.buttons.up;
    if (!gesture->details.buttons.up && !gesture->details.buttons.down)
      return gesture->next;  // remove from list
  }
  if (next_timeout >= 0.0) {
    // next_ is doing stuff, so don't interfere
    *timeout = next_timeout;
    next_expects_timer_ = true;
    return gesture;
  }
  next_expects_timer_ = false;
  if (incoming_button_must_be_up_ && sent_buttons_down_) {
    // We should lift the buttons before too long.
    const stime_t kTimeoutLength = 1.0;
    *timeout = kTimeoutLength;
  }
  return gesture;
}

}  // namespace gestures
