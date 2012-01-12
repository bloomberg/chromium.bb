// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/scaling_filter_interpreter.h"

#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/logging.h"

namespace gestures {

// Takes ownership of |next|:
ScalingFilterInterpreter::ScalingFilterInterpreter(PropRegistry* prop_reg,
                                                   Interpreter* next)
    : tp_x_scale_(1.0),
      tp_y_scale_(1.0),
      tp_x_translate_(0.0),
      tp_y_translate_(0.0),
      screen_x_scale_(1.0),
      screen_y_scale_(1.0),
      pressure_scale_(prop_reg, "Pressure Calibration Slope", 1.0),
      pressure_translate_(prop_reg, "Pressure Calibration Offset", 0.0),
      pressure_threshold_(prop_reg, "Pressure Minimum Threshold", 0.0) {
  next_.reset(next);
}

Gesture* ScalingFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                                 stime_t* timeout) {
  ScaleHardwareState(hwstate);
  Gesture* fg = next_->SyncInterpret(hwstate, timeout);
  if (fg)
    ScaleGesture(fg);
  return fg;
}

Gesture* ScalingFilterInterpreter::HandleTimer(stime_t now, stime_t* timeout) {
  Gesture* gs = next_->HandleTimer(now, timeout);
  if (gs)
    ScaleGesture(gs);
  return gs;
}

// Ignore the finger events with low pressure values especially for the SEMI_MT
// devices such as Synaptics touchpad on Cr-48.
void ScalingFilterInterpreter::FilterLowPressure(HardwareState* hwstate) {
  unsigned short finger_cnt = hwstate->finger_cnt;
  unsigned short touch_cnt = hwstate->touch_cnt;
  float threshold = 0.0;
  if (pressure_scale_.val_ > 0.0) {
    threshold = (pressure_threshold_.val_ - pressure_translate_.val_)
        / pressure_scale_.val_ ;
  }
  for (short i = finger_cnt - 1 ; i >= 0; i--) {
    if (hwstate->fingers[i].pressure < threshold) {
      if (i != finger_cnt - 1)
        hwstate->fingers[i] = hwstate->fingers[finger_cnt - 1];
      finger_cnt--;
      touch_cnt--;
    }
  }
  hwstate->finger_cnt = finger_cnt;
  hwstate->touch_cnt = touch_cnt;
}

void ScalingFilterInterpreter::ScaleHardwareState(HardwareState* hwstate) {
  // Drop the small fingers, i.e. low pressures.
  FilterLowPressure(hwstate);

  for (short i = 0; i < hwstate->finger_cnt; i++) {
    hwstate->fingers[i].position_x *= tp_x_scale_;
    hwstate->fingers[i].position_x += tp_x_translate_;
    hwstate->fingers[i].position_y *= tp_y_scale_;
    hwstate->fingers[i].position_y += tp_y_translate_;
    hwstate->fingers[i].pressure *= pressure_scale_.val_;
    hwstate->fingers[i].pressure += pressure_translate_.val_;
    // TODO(adlr): scale other fields
  }
}

void ScalingFilterInterpreter::ScaleGesture(Gesture* gs) {
  switch (gs->type) {
    case kGestureTypeMove:
      gs->details.move.dx = gs->details.move.dx * screen_x_scale_;
      gs->details.move.dy = gs->details.move.dy * screen_y_scale_;
      break;
    case kGestureTypeScroll:
      gs->details.scroll.dx = gs->details.scroll.dx * screen_x_scale_;
      gs->details.scroll.dy = gs->details.scroll.dy * screen_y_scale_;
      break;
    default:
      break;
  }
}

void ScalingFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hw_props) {

  tp_x_scale_ = 1.0 / hw_props.res_x;
  tp_y_scale_ = 1.0 / hw_props.res_y;
  tp_x_translate_ = -1.0 * (hw_props.left * tp_x_scale_);
  tp_y_translate_ = -1.0 * (hw_props.top * tp_y_scale_);

  screen_x_scale_ = hw_props.screen_x_dpi / 25.4;
  screen_y_scale_ = hw_props.screen_y_dpi / 25.4;

  // Make fake idealized hardware properties to report to next_.
  HardwareProperties friendly_props = {
    0.0,  // left
    0.0,  // top
    (hw_props.right - hw_props.left) * tp_x_scale_,  // right
    (hw_props.bottom - hw_props.top) * tp_y_scale_,  // bottom
    1.0,  // X pixels/mm
    1.0,  // Y pixels/mm
    25.4,  // screen dpi x
    25.4,  // screen dpi y
    hw_props.max_finger_cnt,
    hw_props.max_touch_cnt,
    hw_props.supports_t5r2,
    hw_props.support_semi_mt,
    hw_props.is_button_pad
  };

  next_->SetHardwareProperties(friendly_props);
}

}  // namespace gestures
