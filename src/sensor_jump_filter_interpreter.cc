// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/sensor_jump_filter_interpreter.h"

#include <base/memory/scoped_ptr.h>

#include "gestures/include/util.h"

namespace gestures {

SensorJumpFilterInterpreter::SensorJumpFilterInterpreter(PropRegistry* prop_reg,
                                                         Interpreter* next)
    : FilterInterpreter(next),
      enabled_(prop_reg, "Sensor Jump Filter Enable", 0),
      min_warp_dist_(prop_reg, "Sensor Jump Min Dist", 0.9),
      max_warp_dist_(prop_reg, "Sensor Jump Max Dist", 7.5),
      similar_multiplier_(prop_reg, "Sensor Jump Similar Multiplier", 0.9) {}

Gesture* SensorJumpFilterInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                        stime_t* timeout) {
  if (!enabled_.val_)
    return next_->SyncInterpret(hwstate, timeout);
  RemoveMissingIdsFromMap(&previous_input_[0], *hwstate);
  RemoveMissingIdsFromMap(&previous_input_[1], *hwstate);
  RemoveMissingIdsFromSet(&first_flag_[0], *hwstate);
  RemoveMissingIdsFromSet(&first_flag_[1], *hwstate);

  map<short, FingerState, kMaxFingers> current_input;

  for (size_t i = 0; i < hwstate->finger_cnt; i++)
    current_input[hwstate->fingers[i].tracking_id] = hwstate->fingers[i];

  for (size_t i = 0; i < hwstate->finger_cnt; i++) {
    short tracking_id = hwstate->fingers[i].tracking_id;
    if (!MapContainsKey(previous_input_[1], tracking_id) ||
        !MapContainsKey(previous_input_[0], tracking_id))
      continue;
    FingerState* fs[] = {
      &hwstate->fingers[i],  // newest
      &previous_input_[0][tracking_id],
      &previous_input_[1][tracking_id],  // oldest
    };
    float FingerState::* const fields[] = { &FingerState::position_x,
                                            &FingerState::position_y };
    unsigned warp[] = { GESTURES_FINGER_WARP_X, GESTURES_FINGER_WARP_Y };
    for (size_t f_idx = 0; f_idx < arraysize(fields); f_idx++) {
      float FingerState::* const field = fields[f_idx];
      const float val[] = {
        fs[0]->*field,  // newest
        fs[1]->*field,
        fs[2]->*field,  // oldest
      };
      const float delta[] = {
        val[0] - val[1],  // newer
        val[1] - val[2],  // older
      };
      const float kAllowableChange = fabsf(delta[1] * similar_multiplier_.val_);
      bool should_warp = false;
      bool should_store_flag = false;
      if (delta[0] * delta[1] < 0.0) {
        // switched direction
        should_store_flag = should_warp = true;
      } else if (fabsf(delta[0]) < min_warp_dist_.val_ ||
                 fabsf(delta[0]) > max_warp_dist_.val_) {
        // acceptable movement
      } else if (fabsf(delta[0] - delta[1]) <= kAllowableChange) {
        if (SetContainsValue(first_flag_[f_idx], tracking_id)) {
          // Was flagged last time. Flag one more time
          should_warp = true;
        }
      } else {
        should_store_flag = should_warp = true;
      }
      if (should_warp)
        fs[0]->flags |= warp[f_idx];
      if (should_store_flag)
        first_flag_[f_idx].insert(tracking_id);
      else
        first_flag_[f_idx].erase(tracking_id);
    }
  }

  // Update previous input/output state
  previous_input_[1] = previous_input_[0];
  previous_input_[0] = current_input;

  return next_->SyncInterpret(hwstate, timeout);
}

}  // namespace gestures
