// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/semi_mt_correcting_filter_interpreter.h"

#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/logging.h"

namespace gestures {

SemiMtCorrectingFilterInterpreter::SemiMtCorrectingFilterInterpreter(
    PropRegistry* prop_reg, Interpreter* next)
    : next_id_(0),
      interpreter_enabled_(prop_reg, "Enable SemiMT Correcting Filter", 1),
      pressure_threshold_(prop_reg, "SemiMT Pressure Threshold", 30),
      hysteresis_pressure_(prop_reg, "SemiMT Hysteresis Pressure", 25) {
  memset(id_mapping_, -1, sizeof(id_mapping_));
  next_.reset(next);
}

Gesture* SemiMtCorrectingFilterInterpreter::SyncInterpret(
    HardwareState* hwstate, stime_t* timeout) {

  if (is_semi_mt_device_ && interpreter_enabled_.val_)
    LowPressureFilter(hwstate);

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

size_t SemiMtCorrectingFilterInterpreter::GetTrackingIdIndex(
    int origin_id) const {
  for (size_t i = 0; i < arraysize(id_mapping_); i++)
    if (id_mapping_[i].origin_id == origin_id)
      return i;
  return kInvalidIndex;
}

int SemiMtCorrectingFilterInterpreter::RenewTrackingId(int new_id) {
  size_t new_id_index = (id_mapping_[0].new_id == new_id) ? 0 : 1;
  int origin_id = id_mapping_[new_id_index].origin_id;

  id_mapping_[new_id_index].origin_id = -1;
  return GetNewTrackingId(origin_id);
}

int SemiMtCorrectingFilterInterpreter::GetNewTrackingId(int origin_id) {
  size_t new_tid_index = (id_mapping_[0].origin_id == -1) ? 0 : 1;
  size_t the_other_index = (new_tid_index == 0) ? 1 : 0;
  int used_id = id_mapping_[the_other_index].new_id;

  while (++next_id_ == used_id);
  id_mapping_[new_tid_index].new_id = next_id_;
  id_mapping_[new_tid_index].origin_id = origin_id;
  return next_id_;
}

void SemiMtCorrectingFilterInterpreter::RemoveFinger(
    HardwareState* s, size_t slot_index, size_t mapping_index) {

  if (slot_index == 0 && s->finger_cnt > 1)
    s->fingers[0] = s->fingers[1];
  if (s->finger_cnt)
    s->finger_cnt--;
  else
    Err("Finger count should not be zero.");

  if (s->touch_cnt)
    s->touch_cnt--;
  if (s->touch_cnt == 0) {
    id_mapping_[0].origin_id = id_mapping_[1].origin_id = -1;
  } else {
    if (mapping_index != kInvalidIndex)
      id_mapping_[mapping_index].origin_id = -1;
  }
}

void SemiMtCorrectingFilterInterpreter::LowPressureFilter(HardwareState* s) {
  for (int i = s->finger_cnt - 1; i >= 0; i--) {
    struct FingerState* slot = &s->fingers[i];
    size_t mapping_index = GetTrackingIdIndex(slot->tracking_id);
    if (mapping_index == kInvalidIndex) {
      if (slot->pressure > pressure_threshold_.val_)
        slot->tracking_id = GetNewTrackingId(slot->tracking_id);
      else
        RemoveFinger(s, i, mapping_index);
    } else {
      // Handle the hysteresis here
      if (slot->pressure > hysteresis_pressure_.val_)
        slot->tracking_id = id_mapping_[mapping_index].new_id;
      else
        RemoveFinger(s, i, mapping_index);
    }
  }
}

}  // namespace gestures
