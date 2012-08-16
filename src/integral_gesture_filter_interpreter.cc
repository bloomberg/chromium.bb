// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/integral_gesture_filter_interpreter.h"

#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/tracer.h"

namespace gestures {

// Takes ownership of |next|:
IntegralGestureFilterInterpreter::IntegralGestureFilterInterpreter(
    Interpreter* next, Tracer* tracer)
    : FilterInterpreter(next, tracer),
      x_move_remainder_(0.0),
      y_move_remainder_(0.0),
      hscroll_remainder_(0.0),
      vscroll_remainder_(0.0) {
  InitName();
}

IntegralGestureFilterInterpreter::~IntegralGestureFilterInterpreter() {}

Gesture* IntegralGestureFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate, stime_t* timeout) {
  if (hwstate->finger_cnt == 0 && hwstate->touch_cnt == 0)
    x_move_remainder_ = y_move_remainder_ = hscroll_remainder_ =
        vscroll_remainder_ = 0.0;
  Gesture* fg = next_->SyncInterpret(hwstate, timeout);
  Gesture** ret = &fg;
  HandleGesture(ret);
  return *ret;
}

Gesture* IntegralGestureFilterInterpreter::HandleTimerImpl(stime_t now,
                                                           stime_t* timeout) {
  Gesture* gs = next_->HandleTimer(now, timeout);
  Gesture** ret = &gs;
  HandleGesture(ret);
  return *ret;
}

namespace {
float Truncate(float input, float* overflow) {
  input += *overflow;
  float input_ret = truncf(input);
  *overflow = input - input_ret;
  return input_ret;
}
}  // namespace {}

// Truncate the fractional part off any input, but store it. If the
// absolute value of an input is < 1, we will change it to 0, unless
// there has been enough fractional accumulation to bring it above 1.
void IntegralGestureFilterInterpreter::HandleGesture(Gesture** ret) {
  if (!*ret)
    return;
  Gesture* gs = *ret;
  switch (gs->type) {
    case kGestureTypeMove:
      gs->details.move.dx = Truncate(gs->details.move.dx,
                                     &x_move_remainder_);
      gs->details.move.dy = Truncate(gs->details.move.dy,
                                     &y_move_remainder_);
      if (gs->details.move.dx == 0.0 && gs->details.move.dy == 0.0)
        *ret = NULL;
      break;
    case kGestureTypeScroll:
      gs->details.scroll.dx = Truncate(gs->details.scroll.dx,
                                       &hscroll_remainder_);
      gs->details.scroll.dy = Truncate(gs->details.scroll.dy,
                                       &vscroll_remainder_);
      if (gs->details.scroll.dx == 0.0 && gs->details.scroll.dy == 0.0) {
        if (gs->details.scroll.stop_fling)
          *gs = Gesture(kGestureFling, gs->start_time, gs->end_time,
                        0, 0, GESTURES_FLING_TAP_DOWN);
        else
          *ret = NULL;
      }
      break;
    default:
      break;
  }
}

}  // namespace gestures
