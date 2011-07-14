// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/integral_gesture_filter_interpreter.h"

#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

namespace gestures {

// Takes ownership of |next|:
IntegralGestureFilterInterpreter::IntegralGestureFilterInterpreter(
    Interpreter* next)
    : x_move_remainder_(0.0),
      y_move_remainder_(0.0),
      hscroll_remainder_(0.0),
      vscroll_remainder_(0.0) {
  next_.reset(next);
}

IntegralGestureFilterInterpreter::~IntegralGestureFilterInterpreter() {}

Gesture* IntegralGestureFilterInterpreter::SyncInterpret(
    HardwareState* hwstate) {
  Gesture* fg = next_->SyncInterpret(hwstate);
  if (fg)
    HandleGesture(fg);
  return fg;
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
void IntegralGestureFilterInterpreter::HandleGesture(Gesture* gs) {
  switch (gs->type) {
    case kGestureTypeMove:
      gs->details.move.dx = Truncate(gs->details.move.dx,
                                     &x_move_remainder_);
      gs->details.move.dy = Truncate(gs->details.move.dy,
                                     &y_move_remainder_);
      break;
    case kGestureTypeScroll:
      gs->details.scroll.dx = Truncate(gs->details.scroll.dx,
                                       &hscroll_remainder_);
      gs->details.scroll.dy = Truncate(gs->details.scroll.dy,
                                       &vscroll_remainder_);
      break;
    default:
      break;
  }
}

void IntegralGestureFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hw_props) {
  next_->SetHardwareProperties(hw_props);
}

}  // namespace gestures
