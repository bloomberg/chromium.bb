// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/metrics_filter_interpreter.h"

#include <cmath>

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"
#include "gestures/include/util.h"

namespace gestures {

MetricsFilterInterpreter::MetricsFilterInterpreter(PropRegistry* prop_reg,
                                                       Interpreter* next,
                                                       Tracer* tracer)
    : FilterInterpreter(NULL, next, tracer, false),
      noisy_ground_distance_threshold_(
          prop_reg, "Metrics Noisy Ground Distance", 10.0),
      noisy_ground_time_threshold_(
          prop_reg, "Metrics Noisy Ground Time", 0.1) {
  InitName();

  MState::InitMemoryManager(kMaxFingers * MState::kHistorySize);
  FingerHistory::InitMemoryManager(kMaxFingers);
}

void MetricsFilterInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                   stime_t* timeout) {
  UpdateFingerState(*hwstate);
  next_->SyncInterpret(hwstate, timeout);
}

void MetricsFilterInterpreter::AddNewStateToBuffer(
    FingerHistory* history, const FingerState& fs,
    const HardwareState& hwstate) {
  // The history buffer is already full, pop one
  if (history->size() == static_cast<size_t>(MState::kHistorySize))
    MState::Free(history->PopFront());

  // Push the new finger state to the back of buffer
  MState* current = MState::Allocate();
  if (!current) {
    Err("MState buffer out of space");
    return;
  }
  current->Init(fs, hwstate);
  history->PushBack(current);
}

void MetricsFilterInterpreter::UpdateFingerState(
    const HardwareState& hwstate) {
  FingerHistoryMap removed;
  RemoveMissingIdsFromMap(&histories_, hwstate, &removed);
  for (FingerHistoryMap::const_iterator it =
       removed.begin(); it != removed.end(); ++it)
    FingerHistory::Free(it->second);

  FingerState *fs = hwstate.fingers;
  for (short i = 0; i < hwstate.finger_cnt; i++) {
    FingerHistory* hp;

    // Update the map if the contact is new
    if (!MapContainsKey(histories_, fs[i].tracking_id)) {
      hp = FingerHistory::Allocate();
      if (!hp) {
        Err("FingerHistory out of space");
        continue;
      }
      hp->Init();
      histories_[fs[i].tracking_id] = hp;
    } else {
      hp = histories_[fs[i].tracking_id];
    }

    // Check if the finger history contains interesting patterns
    AddNewStateToBuffer(hp, fs[i], hwstate);
    DetectNoisyGround(hp);
  }
}

bool MetricsFilterInterpreter::DetectNoisyGround(
    const FingerHistory* history) {
  MState* current = history->Tail();
  size_t n_samples = history->size();
  // Noise pattern takes 3 samples
  if (n_samples < 3)
    return false;

  MState* past_1 = current->prev_;
  MState* past_2 = past_1->prev_;
  // Noise pattern needs to happen in a short period of time
  if(current->timestamp - past_2->timestamp >
      noisy_ground_time_threshold_.val_) {
    return false;
  }

  // vec[when][x,y]
  float vec[2][2];
  vec[0][0] = current->data.position_x - past_1->data.position_x;
  vec[0][1] = current->data.position_y - past_1->data.position_y;
  vec[1][0] = past_1->data.position_x - past_2->data.position_x;
  vec[1][1] = past_1->data.position_y - past_2->data.position_y;
  const float thr = noisy_ground_distance_threshold_.val_;
  // We dictate the noise pattern as two consecutive big moves in
  // opposite directions in either X or Y
  for (size_t i = 0; i < arraysize(vec[0]); i++)
    if ((vec[0][i] < -thr && vec[1][i] > thr) ||
        (vec[0][i] > thr && vec[1][i] < -thr)) {
      ProduceGesture(Gesture(kGestureMetrics, past_2->timestamp,
                     current->timestamp, kGestureMetricsTypeNoisyGround,
                     vec[0][i], vec[1][i]));
      return true;
    }
  return false;
}

}  // namespace gestures
