// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/palm_classifying_filter_interpreter.h"

#include <base/memory/scoped_ptr.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/util.h"

namespace gestures {

PalmClassifyingFilterInterpreter::PalmClassifyingFilterInterpreter(
    PropRegistry* prop_reg, Interpreter* next, FingerMetrics* finger_metrics)
    : FilterInterpreter(next),
      finger_metrics_(finger_metrics),
      palm_pressure_(prop_reg, "Palm Pressure", 200.0),
      palm_width_(prop_reg, "Palm Width", 21.2),
      palm_edge_min_width_(prop_reg, "Tap Exclusion Border Width", 8.0),
      palm_edge_width_(prop_reg, "Palm Edge Zone Width", 14.0),
      palm_edge_point_speed_(prop_reg, "Palm Edge Zone Min Point Speed", 100.0),
      palm_eval_timeout_(prop_reg, "Palm Eval Timeout", 0.1),
      palm_stationary_time_(prop_reg, "Palm Stationary Time", 2.0),
      palm_stationary_distance_(prop_reg, "Palm Stationary Distance", 4.0) {
  if (!finger_metrics_) {
    test_finger_metrics_.reset(new FingerMetrics(prop_reg));
    finger_metrics_ = test_finger_metrics_.get();
  }
}

Gesture* PalmClassifyingFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate,
    stime_t* timeout) {
  FillOriginInfo(*hwstate);
  UpdatePalmState(*hwstate);
  UpdatePalmFlags(hwstate);
  prev_time_ = hwstate->timestamp;
  if (next_.get())
    return next_->SyncInterpret(hwstate, timeout);
  return NULL;
}

void PalmClassifyingFilterInterpreter::SetHardwarePropertiesImpl(
    const HardwareProperties& hwprops) {
  hw_props_ = hwprops;
  if (next_.get())
    next_->SetHardwareProperties(hwprops);
}

void PalmClassifyingFilterInterpreter::FillOriginInfo(
    const HardwareState& hwstate) {
  RemoveMissingIdsFromMap(&origin_timestamps_, hwstate);
  RemoveMissingIdsFromMap(&origin_fingerstates_, hwstate);
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    if (MapContainsKey(origin_timestamps_, fs.tracking_id))
      continue;
    origin_timestamps_[fs.tracking_id] = hwstate.timestamp;
    origin_fingerstates_[fs.tracking_id] = fs;
  }
}

bool PalmClassifyingFilterInterpreter::FingerNearOtherFinger(
    const HardwareState& hwstate,
    size_t finger_idx) {
  const FingerState& fs = hwstate.fingers[finger_idx];
  for (int i = 0; i < hwstate.finger_cnt; ++i) {
    const FingerState& other_fs = hwstate.fingers[i];
    if (other_fs.tracking_id == fs.tracking_id)
      continue;
    bool too_close_to_other_finger =
        finger_metrics_->FingersCloseEnoughToGesture(fs, other_fs) &&
        !SetContainsValue(palm_, other_fs.tracking_id);
    if (too_close_to_other_finger)
      return true;
  }
  return false;
}

bool PalmClassifyingFilterInterpreter::FingerInPalmEnvelope(
    const FingerState& fs) {
  float limit = palm_edge_min_width_.val_ +
      (fs.pressure / palm_pressure_.val_) *
      (palm_edge_width_.val_ - palm_edge_min_width_.val_);
  return fs.position_x < limit ||
      fs.position_x > (hw_props_.right - limit) ||
      fs.position_y > (hw_props_.bottom - limit);
}

void PalmClassifyingFilterInterpreter::UpdatePalmState(
    const HardwareState& hwstate) {
  RemoveMissingIdsFromSet(&palm_, hwstate);
  RemoveMissingIdsFromSet(&pointing_, hwstate);
  RemoveMissingIdsFromSet(&non_stationary_palm_, hwstate);
  for (short i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    // Mark anything over the palm thresh as a palm
    if (fs.pressure >= palm_pressure_.val_ ||
        fs.touch_major >= palm_width_.val_) {
      palm_.insert(fs.tracking_id);
      pointing_.erase(fs.tracking_id);

      continue;
    }
  }

  const float kPalmStationaryDistSq =
      palm_stationary_distance_.val_ * palm_stationary_distance_.val_;

  for (short i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    bool prev_palm = SetContainsValue(palm_, fs.tracking_id);
    bool prev_pointing = SetContainsValue(pointing_, fs.tracking_id);

    // Lock onto palm
    if (prev_palm)
      continue;
    // If the finger is recently placed, remove it from pointing/fingers.
    // If it's still looking like pointing, it'll get readded.
    if (FingerAge(fs.tracking_id, hwstate.timestamp) <
        palm_eval_timeout_.val_) {
      pointing_.erase(fs.tracking_id);

      prev_pointing = false;
    }
    // If another finger is close by, let this be pointing
    if (!prev_pointing && (FingerNearOtherFinger(hwstate, i) ||
                           !FingerInPalmEnvelope(fs))) {
      pointing_.insert(fs.tracking_id);

    }
    // However, if the contact has been stationary for a while since it
    // touched down, it is a palm. We track a potential palm closely for the
    // first amount of time to see if it fits this pattern.
    if (FingerAge(fs.tracking_id, prev_time_) >
        palm_stationary_time_.val_ ||
        SetContainsValue(non_stationary_palm_, fs.tracking_id)) {
      // Finger is too old to reconsider or is moving a lot
      continue;
    }
    if (DistSq(origin_fingerstates_[fs.tracking_id], fs) >
        kPalmStationaryDistSq || !FingerInPalmEnvelope(fs)) {
      // Finger moving a lot or not in palm envelope; not a stationary palm.
      non_stationary_palm_.insert(fs.tracking_id);
      continue;
    }
    if (FingerAge(fs.tracking_id, prev_time_) <=
        palm_stationary_time_.val_ &&
        FingerAge(fs.tracking_id, hwstate.timestamp) >
        palm_stationary_time_.val_ &&
        !SetContainsValue(non_stationary_palm_, fs.tracking_id)) {
      // Enough time has passed. Make this stationary contact a palm.
      palm_.insert(fs.tracking_id);
      pointing_.erase(fs.tracking_id);

    }
  }
}

void PalmClassifyingFilterInterpreter::UpdatePalmFlags(HardwareState* hwstate) {
  for (short i = 0; i < hwstate->finger_cnt; i++) {
    FingerState* fs = &hwstate->fingers[i];
    if (SetContainsValue(palm_, fs->tracking_id) ||
        (!SetContainsValue(pointing_, fs->tracking_id) &&
         FingerInPalmEnvelope(*fs))) {
      // Finger is believed to be a palm, or it's ambiguous and not likely
      // co-pointing
      fs->flags |= GESTURES_FINGER_PALM;
    } else if (SetContainsValue(pointing_, fs->tracking_id) &&
               FingerInPalmEnvelope(*fs)) {
      fs->flags |= GESTURES_FINGER_POSSIBLE_PALM;
    }
  }
}

stime_t PalmClassifyingFilterInterpreter::FingerAge(short finger_id,
                                                    stime_t now) const {
  if (!MapContainsKey(origin_timestamps_, finger_id)) {
    Err("Don't have record of finger age for finger %d", finger_id);
    return -1;
  }
  return now - origin_timestamps_[finger_id];
}

}  // namespace gestures
