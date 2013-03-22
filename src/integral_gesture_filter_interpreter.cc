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
    : FilterInterpreter(NULL, next, tracer, false),
      hscroll_remainder_(0.0),
      vscroll_remainder_(0.0),
      hscroll_ordinal_remainder_(0.0),
      vscroll_ordinal_remainder_(0.0) {
  InitName();
}

Gesture* IntegralGestureFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate, stime_t* timeout) {
  if (hwstate->finger_cnt == 0 && hwstate->touch_cnt == 0)
    hscroll_ordinal_remainder_ = vscroll_ordinal_remainder_ =
        hscroll_remainder_ = vscroll_remainder_ = 0.0;
  Gesture* fg = next_->SyncInterpret(hwstate, timeout);
  return HandleGesture(fg);
}

Gesture* IntegralGestureFilterInterpreter::HandleTimerImpl(stime_t now,
                                                           stime_t* timeout) {
  Gesture* gs = next_->HandleTimer(now, timeout);
  return HandleGesture(gs);
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
Gesture* IntegralGestureFilterInterpreter::HandleGesture(Gesture* gs) {
  if (!gs)
    return gs;

  if (gs->next) {
    gs->next = HandleGesture(gs->next);
  }

  switch (gs->type) {
    case kGestureTypeMove:
      if (gs->details.move.dx == 0.0 && gs->details.move.dy == 0.0 &&
          gs->details.move.ordinal_dx == 0.0 &&
          gs->details.move.ordinal_dy == 0.0)
        return gs->next;  // remove from list
      break;
    case kGestureTypeScroll:
      gs->details.scroll.dx = Truncate(gs->details.scroll.dx,
                                       &hscroll_remainder_);
      gs->details.scroll.dy = Truncate(gs->details.scroll.dy,
                                       &vscroll_remainder_);
      gs->details.scroll.ordinal_dx = Truncate(gs->details.scroll.ordinal_dx,
                                       &hscroll_ordinal_remainder_);
      gs->details.scroll.ordinal_dy = Truncate(gs->details.scroll.ordinal_dy,
                                       &vscroll_ordinal_remainder_);
      if (gs->details.scroll.dx == 0.0 && gs->details.scroll.dy == 0.0 &&
          gs->details.scroll.ordinal_dx == 0.0 &&
          gs->details.scroll.ordinal_dy == 0.0) {
        if (gs->details.scroll.stop_fling)
          *gs = Gesture(kGestureFling, gs->start_time, gs->end_time,
                        0, 0, GESTURES_FLING_TAP_DOWN);
        else
          return gs->next;  // remove from list
      }
      break;
    default:
      break;
  }

  return gs;
}

}  // namespace gestures
