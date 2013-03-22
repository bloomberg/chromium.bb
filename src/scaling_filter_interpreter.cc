// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/scaling_filter_interpreter.h"

#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/logging.h"
#include "gestures/include/tracer.h"

namespace gestures {

// Takes ownership of |next|:
ScalingFilterInterpreter::ScalingFilterInterpreter(
    PropRegistry* prop_reg, Interpreter* next, Tracer* tracer,
    GestureInterpreterDeviceClass devclass)
    : FilterInterpreter(NULL, next, tracer, false),
      devclass_(devclass),
      tp_x_scale_(1.0),
      tp_y_scale_(1.0),
      tp_x_translate_(0.0),
      tp_y_translate_(0.0),
      screen_x_scale_(1.0),
      screen_y_scale_(1.0),
      orientation_scale_(1.0),
      surface_area_from_pressure_(prop_reg,
                                  "Compute Surface Area from Pressure", true),
      tp_x_bias_(prop_reg, "Touchpad Device Output Bias on X-Axis", 0.0),
      tp_y_bias_(prop_reg, "Touchpad Device Output Bias on Y-Axis", 0.0),
      pressure_scale_(prop_reg, "Pressure Calibration Slope", 1.0),
      pressure_translate_(prop_reg, "Pressure Calibration Offset", 0.0),
      pressure_threshold_(prop_reg, "Pressure Minimum Threshold", 0.0),
      mouse_cpi_(prop_reg, "Mouse CPI", 1000.0) {
  InitName();
}

Gesture* ScalingFilterInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                     stime_t* timeout) {
  ScaleHardwareState(hwstate);
  Gesture* fg = next_->SyncInterpret(hwstate, timeout);
  for (Gesture* it = fg; it; it = it->next)
    ScaleGesture(it);
  return fg;
}

Gesture* ScalingFilterInterpreter::HandleTimerImpl(stime_t now,
                                                   stime_t* timeout) {
  Gesture* gs = next_->HandleTimer(now, timeout);
  for (Gesture* it = gs; it; it = it->next)
    ScaleGesture(it);
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
      if (touch_cnt > 0)
        touch_cnt--;
    }
  }
  hwstate->finger_cnt = finger_cnt;
  hwstate->touch_cnt = touch_cnt;
}

void ScalingFilterInterpreter::ScaleHardwareState(HardwareState* hwstate) {
  if (devclass_ == GESTURES_DEVCLASS_TOUCHPAD ||
      devclass_ == GESTURES_DEVCLASS_TOUCHSCREEN) {
    ScaleTouchpadHardwareState(hwstate);
  } else if (devclass_ == GESTURES_DEVCLASS_MOUSE) {
    ScaleMouseHardwareState(hwstate);
  } else if (devclass_ == GESTURES_DEVCLASS_MULTITOUCH_MOUSE) {
    ScaleTouchpadHardwareState(hwstate);
    ScaleMouseHardwareState(hwstate);
  } else {
    Err("Couldn't recognize devclass: %d", devclass_);
  }
}

void ScalingFilterInterpreter::ScaleMouseHardwareState(
    HardwareState* hwstate) {
  hwstate->rel_x = hwstate->rel_x / mouse_cpi_.val_ * 25.4;
  hwstate->rel_y = hwstate->rel_y / mouse_cpi_.val_ * 25.4;
  // TODO(clchiou): Scale rel_wheel and rel_hwheel
}

void ScalingFilterInterpreter::ScaleTouchpadHardwareState(
    HardwareState* hwstate) {
  if (surface_area_from_pressure_.val_) {
    // Drop the small fingers, i.e. low pressures.
    FilterLowPressure(hwstate);
  }

  for (short i = 0; i < hwstate->finger_cnt; i++) {
    float cos_2_orit = 0.0, sin_2_orit = 0.0, rx_2 = 0.0, ry_2 = 0.0;
    hwstate->fingers[i].position_x *= tp_x_scale_;
    hwstate->fingers[i].position_x += tp_x_translate_;
    hwstate->fingers[i].position_y *= tp_y_scale_;
    hwstate->fingers[i].position_y += tp_y_translate_;

    // TODO(clchiou): Output orientation is computed on a pixel-unit circle,
    // and it is only equal to the orientation computed on a mm-unit circle
    // when tp_x_scale_ == tp_y_scale_.  Since what we really want is the
    // latter, fix this!
    hwstate->fingers[i].orientation *= orientation_scale_;

    if (hwstate->fingers[i].touch_major || hwstate->fingers[i].touch_minor) {
      cos_2_orit = cosf(hwstate->fingers[i].orientation);
      cos_2_orit *= cos_2_orit;
      sin_2_orit = sinf(hwstate->fingers[i].orientation);
      sin_2_orit *= sin_2_orit;
      rx_2 = tp_x_scale_ * tp_x_scale_;
      ry_2 = tp_y_scale_ * tp_y_scale_;
    }
    if (hwstate->fingers[i].touch_major) {
      float bias = tp_x_bias_.val_ * sin_2_orit + tp_y_bias_.val_ * cos_2_orit;
      hwstate->fingers[i].touch_major =
          fabsf(hwstate->fingers[i].touch_major - bias) *
          sqrtf(rx_2 * sin_2_orit + ry_2 * cos_2_orit);
    }
    if (hwstate->fingers[i].touch_minor) {
      float bias = tp_x_bias_.val_ * cos_2_orit + tp_y_bias_.val_ * sin_2_orit;
      hwstate->fingers[i].touch_minor =
          fabsf(hwstate->fingers[i].touch_minor - bias) *
          sqrtf(rx_2 * cos_2_orit + ry_2 * sin_2_orit);
    }

    // After calibration, touch_major could be smaller than touch_minor.
    // If so, swap them here and update orientation.
    if (orientation_scale_ &&
        hwstate->fingers[i].touch_major < hwstate->fingers[i].touch_minor) {
      std::swap(hwstate->fingers[i].touch_major,
                hwstate->fingers[i].touch_minor);
      if (hwstate->fingers[i].orientation > 0.0)
        hwstate->fingers[i].orientation -= M_PI_2;
      else
        hwstate->fingers[i].orientation += M_PI_2;
    }

    if (surface_area_from_pressure_.val_) {
      hwstate->fingers[i].pressure *= pressure_scale_.val_;
      hwstate->fingers[i].pressure += pressure_translate_.val_;
    } else {
      if (hwstate->fingers[i].touch_major && hwstate->fingers[i].touch_minor)
        hwstate->fingers[i].pressure = M_PI_4 *
            hwstate->fingers[i].touch_major * hwstate->fingers[i].touch_minor;
      else if (hwstate->fingers[i].touch_major)
        hwstate->fingers[i].pressure = M_PI_4 *
            hwstate->fingers[i].touch_major * hwstate->fingers[i].touch_major;
      else
        hwstate->fingers[i].pressure = 0;
    }
  }
}

void ScalingFilterInterpreter::ScaleGesture(Gesture* gs) {
  switch (gs->type) {
    case kGestureTypeMove:
      gs->details.move.dx *= screen_x_scale_;
      gs->details.move.dy *= screen_y_scale_;
      gs->details.move.ordinal_dx *= screen_x_scale_;
      gs->details.move.ordinal_dy *= screen_y_scale_;
      break;
    case kGestureTypeScroll:
      gs->details.scroll.dx *= screen_x_scale_;
      gs->details.scroll.dy *= screen_y_scale_;
      gs->details.scroll.ordinal_dx *= screen_x_scale_;
      gs->details.scroll.ordinal_dy *= screen_y_scale_;
      break;
    case kGestureTypeFling:
      gs->details.fling.vx *= screen_x_scale_;
      gs->details.fling.vy *= screen_y_scale_;
      gs->details.fling.ordinal_vx *= screen_x_scale_;
      gs->details.fling.ordinal_vy *= screen_y_scale_;
      break;
    default:
      break;
  }
}

void ScalingFilterInterpreter::SetHardwarePropertiesImpl(
    const HardwareProperties& hw_props) {

  tp_x_scale_ = 1.0 / hw_props.res_x;
  tp_y_scale_ = 1.0 / hw_props.res_y;
  tp_x_translate_ = -1.0 * (hw_props.left * tp_x_scale_);
  tp_y_translate_ = -1.0 * (hw_props.top * tp_y_scale_);

  screen_x_scale_ = hw_props.screen_x_dpi / 25.4;
  screen_y_scale_ = hw_props.screen_y_dpi / 25.4;

  if (hw_props.orientation_maximum)
    orientation_scale_ =
        M_PI / (hw_props.orientation_maximum -
                hw_props.orientation_minimum + 1);
  else
    orientation_scale_ = 0.0;  // no orientation is provided

  float friendly_orientation_minimum;
  float friendly_orientation_maximum;
  if (orientation_scale_) {
    friendly_orientation_minimum =
        orientation_scale_ * hw_props.orientation_minimum;
    friendly_orientation_maximum =
        orientation_scale_ * hw_props.orientation_maximum;
  } else {
    friendly_orientation_minimum = 0.0;
    friendly_orientation_maximum = 0.0;
  }

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
    friendly_orientation_minimum,  // radians
    friendly_orientation_maximum,  // radians
    hw_props.max_finger_cnt,
    hw_props.max_touch_cnt,
    hw_props.supports_t5r2,
    hw_props.support_semi_mt,
    hw_props.is_button_pad
  };

  next_->SetHardwareProperties(friendly_props);
}

}  // namespace gestures
