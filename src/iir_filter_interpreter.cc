// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/iir_filter_interpreter.h"

#include <utility>

namespace gestures {

void IirFilterInterpreter::IoHistory::Increment() {
  out_head = NextOutHead();
  in_head = NextInHead();
}

// Uses exact comparisions, rather than approximate float comparisions,
// for operator==
bool IirFilterInterpreter::IoHistory::operator==(
    const IirFilterInterpreter::IoHistory& that) const {
  for (size_t i = 0; i < kInSize; i++)
    if (in[i] != that.in[i])
      return false;
  for (size_t i = 0; i < kOutSize; i++)
    if (out[i] != that.out[i])
      return false;
  return true;
}

// The default filter is a low-pass 2nd order Butterworth IIR filter with a
// normalized cutoff frequency of 0.2.
IirFilterInterpreter::IirFilterInterpreter(PropRegistry* prop_reg,
                                           Interpreter* next)
    : b0_(prop_reg, "IIR b0", 0.0674552738890719, this),
      b1_(prop_reg, "IIR b1", 0.134910547778144, this),
      b2_(prop_reg, "IIR b2", 0.0674552738890719, this),
      b3_(prop_reg, "IIR b3", 0.0, this),
      a1_(prop_reg, "IIR a1", -1.1429805025399, this),
      a2_(prop_reg, "IIR a2", 0.412801598096189, this),
      iir_dist_thresh_(prop_reg, "IIR Distance Threshold", 10, this),
      using_iir_(true),
      is_semi_mt_device_(false) {
  next_.reset(next);
}

Gesture* IirFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                             stime_t* timeout) {
  if (is_semi_mt_device_)
    return next_->SyncInterpret(hwstate, timeout);

  // Delete old entries from map
  short dead_ids[histories_.size()];
  size_t dead_ids_len = 0;
  for (map<short, IoHistory, kMaxFingers>::iterator it = histories_.begin(),
           e = histories_.end(); it != e; ++it)
    if (!hwstate->GetFingerState((*it).first))
      dead_ids[dead_ids_len++] = (*it).first;
  for (size_t i = 0; i < dead_ids_len; ++i)
    histories_.erase(dead_ids[i]);

  // Modify current hwstate
  for (size_t i = 0; i < hwstate->finger_cnt; i++) {
    FingerState* fs = &hwstate->fingers[i];
    map<short, IoHistory, kMaxFingers>::iterator history =
        histories_.find(fs->tracking_id);
    if (history == histories_.end()) {
      // new finger
      IoHistory hist(*fs);
      histories_[fs->tracking_id] = hist;
      continue;
    }
    // existing finger, apply filter
    IoHistory* hist = &(*history).second;

    float dx = fs->position_x - hist->PrevOut(0)->position_x;
    float dy = fs->position_y - hist->PrevOut(0)->position_y;

    // IIR filter is too smooth for a quick finger movement. We do a simple
    // rolling average if the position change between current and previous
    // frames is larger than iir_dist_thresh_.
    if (dx * dx + dy * dy > iir_dist_thresh_.val_ * iir_dist_thresh_.val_)
      using_iir_ = false;
    else
      using_iir_ = true;

    // TODO(adlr): consider applying filter to other fields
    float FingerState::*fields[] = { &FingerState::position_x,
                                     &FingerState::position_y,
                                     &FingerState::pressure };
    for (size_t f_idx = 0; f_idx < arraysize(fields); f_idx++) {
      float FingerState::*field = fields[f_idx];
      if (using_iir_) {
        hist->NextOut()->*field =
            b3_.val_ * hist->PrevIn(2)->*field +
            b2_.val_ * hist->PrevIn(1)->*field +
            b1_.val_ * hist->PrevIn(0)->*field +
            b0_.val_ * fs->*field -
            a2_.val_ * hist->PrevOut(1)->*field -
            a1_.val_ * hist->PrevOut(0)->*field;
      } else {
        hist->NextOut()->*field = 0.5 * (fs->*field + hist->PrevOut(0)->*field);
      }
    }
    float FingerState::*pass_fields[] = { &FingerState::touch_major,
                                          &FingerState::touch_minor,
                                          &FingerState::width_major,
                                          &FingerState::width_minor,
                                          &FingerState::orientation };
    for (size_t f_idx = 0; f_idx < arraysize(fields); f_idx++)
      hist->NextOut()->*pass_fields[f_idx] = fs->*pass_fields[f_idx];
    hist->NextOut()->flags = fs->flags;
    *hist->NextIn() = *fs;
    *fs = *hist->NextOut();
    hist->Increment();
  }
  return next_->SyncInterpret(hwstate, timeout);
}

Gesture* IirFilterInterpreter::HandleTimer(stime_t now, stime_t* timeout) {
  return next_->HandleTimer(now, timeout);
}

void IirFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  is_semi_mt_device_ = hwprops.support_semi_mt;
  return next_->SetHardwareProperties(hwprops);
}

void IirFilterInterpreter::DoubleWasWritten(DoubleProperty* prop) {
  histories_.clear();
}

}  // namespace gestures
