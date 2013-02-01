// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/finger_merge_filter_interpreter.h"

#include <cmath>

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

namespace gestures {

FingerMergeFilterInterpreter::FingerMergeFilterInterpreter(
    PropRegistry* prop_reg, Interpreter* next, Tracer* tracer)
    : FilterInterpreter(NULL, next, tracer, false),
      finger_merge_filter_enable_(prop_reg,
                                  "Finger Merge Filter Enabled", true),
      merge_distance_threshold_(prop_reg,
                                "Finger Merge Distance Thresh", 250.0),
      max_pressure_threshold_(prop_reg,
                              "Finger Merge Maximum Pressure", 75.0),
      min_major_threshold_(prop_reg,
                           "Finger Merge Minimum Touch Major", 280.0),
      merged_major_pressure_ratio_(prop_reg,
                                   "Merged Finger Touch Major Pressure Ratio",
                                   5.0),
      merged_major_threshold_(prop_reg,
                              "Merged Finger Touch Major Thresh", 380.0) {
  InitName();
}

Gesture* FingerMergeFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate, stime_t* timeout) {
  if (finger_merge_filter_enable_.val_)
    UpdateFingerMergeState(*hwstate);
  return next_->SyncInterpret(hwstate, timeout);
}

void FingerMergeFilterInterpreter::SetHardwarePropertiesImpl(
    const HardwareProperties& hw_props) {
  next_->SetHardwareProperties(hw_props);
}

void FingerMergeFilterInterpreter::UpdateFingerMergeState(
    const HardwareState& hwstate) {

  RemoveMissingIdsFromSet(&merge_tracking_ids_, hwstate);

  // Append GESTURES_FINGER_MERGE flag for close fingers and
  // fingers marked with the same flag previously
  for (short i = 0; i < hwstate.finger_cnt; i++) {
    FingerState *fs = hwstate.fingers;
    if (SetContainsValue(merge_tracking_ids_, fs[i].tracking_id))
      fs[i].flags |= GESTURES_FINGER_MERGE;
    for (short j = i + 1; j < hwstate.finger_cnt; j++) {
      float xfd = fabsf(fs[i].position_x - fs[j].position_x);
      float yfd = fabsf(fs[i].position_y - fs[j].position_y);
      if (xfd < merge_distance_threshold_.val_ &&
          yfd < merge_distance_threshold_.val_) {
        fs[i].flags |= GESTURES_FINGER_MERGE;
        fs[j].flags |= GESTURES_FINGER_MERGE;
        merge_tracking_ids_.insert(fs[i].tracking_id);
        merge_tracking_ids_.insert(fs[j].tracking_id);
      }
    }
  }

  // Detect if there is a merged finger
  for (short i = 0; i < hwstate.finger_cnt; i++) {
    FingerState *fs = &hwstate.fingers[i];

    // Basic criteria of a merged finger:
    //   1. large touch major
    //   2. small pressure value
    if (fs->flags & GESTURES_FINGER_MERGE ||
        fs->touch_major < min_major_threshold_.val_ ||
        fs->pressure > max_pressure_threshold_.val_) {
      continue;
    }

    // Filter out most false positive cases
    if (fs->touch_major > merged_major_pressure_ratio_.val_ * fs->pressure ||
        fs->touch_major > merged_major_threshold_.val_) {
      merge_tracking_ids_.insert(fs->tracking_id);
      fs->flags |= GESTURES_FINGER_MERGE;
    }
  }
}

}
