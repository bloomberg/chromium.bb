// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/semi_mt_correcting_filter_interpreter.h"

#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/logging.h"
#include "gestures/include/util.h"

namespace gestures {

SemiMtCorrectingFilterInterpreter::SemiMtCorrectingFilterInterpreter(
    PropRegistry* prop_reg, Interpreter* next)
    : last_id_(0),
      interpreter_enabled_(prop_reg, "Enable SemiMT Correcting Filter", 1),
      pressure_threshold_(prop_reg, "SemiMT Pressure Threshold", 30),
      hysteresis_pressure_(prop_reg, "SemiMT Hysteresis Pressure", 25) {
  memset(&prev_hwstate_, 0, sizeof(prev_hwstate_));
  next_.reset(next);
}

Gesture* SemiMtCorrectingFilterInterpreter::SyncInterpret(
    HardwareState* hwstate, stime_t* timeout) {

  if (is_semi_mt_device_) {
    if (interpreter_enabled_.val_) {
      LowPressureFilter(hwstate);
      AssignTrackingId(hwstate);
      prev_hwstate_ = *hwstate;
      std::copy(hwstate->fingers, hwstate->fingers + kMaxSemiMtFingers,
                prev_fingers_);
      prev_hwstate_.fingers = prev_fingers_;
    } else {
      memset(&prev_hwstate_, 0, sizeof(prev_hwstate_));
    }
  }

  return next_->SyncInterpret(hwstate, timeout);
}

Gesture* SemiMtCorrectingFilterInterpreter::HandleTimer(
    stime_t now, stime_t* timeout) {
  return next_->HandleTimer(now, timeout);
}

void SemiMtCorrectingFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hw_props) {
  is_semi_mt_device_ = hw_props.support_semi_mt;
  next_->SetHardwareProperties(hw_props);
}

void SemiMtCorrectingFilterInterpreter::AssignTrackingId(
    HardwareState* hwstate) {
  if (hwstate->finger_cnt == 0) {
    return;
  } else if (prev_hwstate_.finger_cnt == 0) {
    for (size_t i = 0; i < hwstate->finger_cnt; i++)
      hwstate->fingers[i].tracking_id = last_id_++;
  } else if (prev_hwstate_.finger_cnt == 1 && hwstate->finger_cnt == 2) {
      int finger0_tid = prev_hwstate_.fingers[0].tracking_id;
      hwstate->fingers[0].tracking_id = finger0_tid;
      hwstate->fingers[1].tracking_id = last_id_;
      while (++last_id_ == finger0_tid);
  } else if (prev_hwstate_.finger_cnt == 2 && hwstate->finger_cnt == 1) {
    float dist_sq_prev_finger0 =
        DistSq(prev_hwstate_.fingers[0], hwstate->fingers[0]);
    float dist_sq_prev_finger1 =
        DistSq(prev_hwstate_.fingers[1], hwstate->fingers[0]);
    if (dist_sq_prev_finger0 < dist_sq_prev_finger1)
      hwstate->fingers[0].tracking_id = prev_hwstate_.fingers[0].tracking_id;
    else
      hwstate->fingers[0].tracking_id = prev_hwstate_.fingers[1].tracking_id;
  } else {  // hwstate->finger_cnt == prev_hwstate_.finger_cnt_
    for (size_t i = 0; i < hwstate->finger_cnt; i++)
      hwstate->fingers[i].tracking_id = prev_hwstate_.fingers[i].tracking_id;
  }
}

void SemiMtCorrectingFilterInterpreter::LowPressureFilter(
    HardwareState* hwstate) {
  // The pressure value will be the same for both fingers for semi_mt device.
  // Therefore, we either keep or remove all fingers based on finger 0's
  // pressure.
  if (hwstate->finger_cnt == 0)
    return;
  float pressure = hwstate->fingers[0].pressure;
  if (((prev_hwstate_.finger_cnt == 0) &&
      (pressure < pressure_threshold_.val_)) ||
      ((prev_hwstate_.finger_cnt > 0) &&
      (pressure < hysteresis_pressure_.val_)))
    hwstate->finger_cnt = hwstate->touch_cnt = 0;
}

}  // namespace gestures
