// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/trend_classifying_filter_interpreter.h"

#include <cmath>

#include "gestures/include/filter_interpreter.h"
#include "gestures/include/finger_metrics.h"
#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"
#include "gestures/include/prop_registry.h"
#include "gestures/include/tracer.h"

namespace {

// Constants for multiplication. Used in
// TrendClassifyingFilterInterpreter::ComputeKTVariance (see the header file).
const double k1_18 = 1.0 / 18.0;
const double k2_3 = 2.0 / 3.0;

}  // namespace {}

namespace gestures {

TrendClassifyingFilterInterpreter::TrendClassifyingFilterInterpreter(
    PropRegistry* prop_reg, Interpreter* next, Tracer* tracer)
    : FilterInterpreter(NULL, next, tracer, false),
      trend_classifying_filter_enable_(
          prop_reg, "Trend Classifying Filter Enabled", true),
      second_order_enable_(
          prop_reg, "Trend Classifying 2nd-order Motion Enabled", true),
      min_num_of_samples_(
          prop_reg, "Trend Classifying Min Num of Samples", 6),
      num_of_samples_(
          prop_reg, "Trend Classifying Num of Samples", 20),
      z_threshold_(
          prop_reg, "Trend Classifying Z Threshold", 2.5758293035489004) {
  InitName();

  KState::InitMemoryManager(kMaxFingers * num_of_samples_.val_);
  FingerHistory::InitMemoryManager(kMaxFingers);
}

void TrendClassifyingFilterInterpreter::SyncInterpretImpl(
    HardwareState* hwstate, stime_t* timeout) {
  if (trend_classifying_filter_enable_.val_)
    UpdateFingerState(*hwstate);
  next_->SyncInterpret(hwstate, timeout);
}

double TrendClassifyingFilterInterpreter::ComputeKTVariance(const int tie_n2,
    const int tie_n3, const size_t n_samples) {
  // Replace divisions with multiplications for better performance
  double var_n = n_samples * (n_samples - 1) * (2 * n_samples + 5) * k1_18;
  double var_t = k2_3 * tie_n3 + tie_n2;
  return var_n - var_t;
}

void TrendClassifyingFilterInterpreter::InterpretTestResult(
    const TrendType trend_type,
    const unsigned flag_increasing,
    const unsigned flag_decreasing,
    unsigned* flags) {
  if (trend_type == TREND_INCREASING)
    *flags |= flag_increasing;
  else if (trend_type == TREND_DECREASING)
    *flags |= flag_decreasing;
}

void TrendClassifyingFilterInterpreter::AddNewStateToBuffer(
    FingerHistory* history, const FingerState& fs) {
  // The history buffer is already full, pop one
  if (history->size() == static_cast<size_t>(num_of_samples_.val_))
    KState::Free(history->PopFront());

  // Push the new finger state to the back of buffer
  KState* current = KState::Allocate();
  if (!current) {
    Err("KState buffer out of space");
    return;
  }
  current->Init(fs);
  KState* previous_end = history->Tail();
  history->PushBack(current);
  if (history->size() == 1)
    return;

  current->dx_ = current->x_ - previous_end->x_;
  current->dy_ = current->y_ - previous_end->y_;
  // Update the nodes already in the buffer and compute the Kendall score/
  // variance along the way. Complexity is O(|buffer|) per finger.
  int tie_n2[4] = { 0, 0, 0, 0 };
  int tie_n3[4] = { 0, 0, 0, 0 };
  for (KState* it = history->Begin(); it != history->Tail(); it = it->next_) {
    UpdateKTValuePair(it->x_, current->x_, &it->x_sum_, &it->x_ties_,
        &current->x_score_, &tie_n2[0], &tie_n3[0]);
    UpdateKTValuePair(it->y_, current->y_, &it->y_sum_, &it->y_ties_,
        &current->y_score_, &tie_n2[1], &tie_n3[1]);
    if (it != history->Begin()) {
      // Skip the first state since its dx/dy are undefined
      UpdateKTValuePair(it->dx_, current->dx_, &it->dx_sum_, &it->dx_ties_,
          &current->dx_score_, &tie_n2[2], &tie_n3[2]);
      UpdateKTValuePair(it->dy_, current->dy_, &it->dy_sum_, &it->dy_ties_,
          &current->dy_score_, &tie_n2[3], &tie_n3[3]);
    }
  }
  size_t n_samples = history->size();
  current->x_var_ = ComputeKTVariance(tie_n2[0], tie_n3[0], n_samples);
  current->y_var_ = ComputeKTVariance(tie_n2[1], tie_n3[1], n_samples);
  current->dx_var_ = ComputeKTVariance(tie_n2[2], tie_n3[2], n_samples - 1);
  current->dy_var_ = ComputeKTVariance(tie_n2[3], tie_n3[3], n_samples - 1);
}

TrendClassifyingFilterInterpreter::TrendType
TrendClassifyingFilterInterpreter::RunKTTest(const int score,
    const double var, const size_t n_samples) {
  // Sample size is too small for a meaningful result
  if (n_samples < static_cast<size_t>(min_num_of_samples_.val_))
    return TREND_NONE;

  // A zero score implies purely random behavior. Need to special-case it
  // because the test might be fooled with a zero variance (e.g. all
  // observations are tied).
  if (!score)
    return TREND_NONE;

  // The test conduct the hypothesis test based on the fact that S/sqrt(Var(S))
  // approximately follows the normal distribution. To optimize for speed,
  // we reformulate the expression to drop the sqrt and division operations.
  if (score * score < z_threshold_.val_ * z_threshold_.val_ * var)
    return TREND_NONE;
  return (score > 0) ? TREND_INCREASING : TREND_DECREASING;
}

void TrendClassifyingFilterInterpreter::UpdateFingerState(
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
      // placement new
      new (hp) FingerHistory();
      histories_[fs[i].tracking_id] = hp;
    } else {
      hp = histories_[fs[i].tracking_id];
    }

    // Check if the score demonstrates statistical significance
    AddNewStateToBuffer(hp, fs[i]);
    KState* current = hp->Tail();
    size_t n_samples = hp->size();
    TrendType x_result = RunKTTest(
        current->x_score_, current->x_var_, n_samples);
    TrendType y_result = RunKTTest(
        current->y_score_, current->y_var_, n_samples);
    InterpretTestResult(x_result, GESTURES_FINGER_TREND_INC_X,
        GESTURES_FINGER_TREND_DEC_X, &(fs[i].flags));
    InterpretTestResult(y_result, GESTURES_FINGER_TREND_INC_Y,
        GESTURES_FINGER_TREND_DEC_Y, &(fs[i].flags));
    if (second_order_enable_.val_) {
      TrendType dx_result = RunKTTest(
          current->dx_score_, current->dx_var_, n_samples - 1);
      TrendType dy_result = RunKTTest(
          current->dy_score_, current->dy_var_, n_samples - 1);
      InterpretTestResult(dx_result, GESTURES_FINGER_TREND_INC_X,
          GESTURES_FINGER_TREND_DEC_X, &(fs[i].flags));
      InterpretTestResult(dy_result, GESTURES_FINGER_TREND_INC_Y,
          GESTURES_FINGER_TREND_DEC_Y, &(fs[i].flags));
    }
  }
}

void TrendClassifyingFilterInterpreter::KState::Init() {
  x_ = 0.0, y_ = 0.0;
  dx_ = 0.0, dy_ = 0.0;
  x_sum_ = 0, x_ties_ = 0;
  y_sum_ = 0, y_ties_ = 0;
  dx_sum_ = 0, dx_ties_ = 0;
  dy_sum_ = 0, dy_ties_ = 0;
  x_score_ = 0, y_score_ = 0;
  dx_score_ = 0, dy_score_ = 0;
  x_var_ = 0.0, y_var_ = 0.0;
  dx_var_ = 0.0, dy_var_ = 0.0;
  next_ = NULL, prev_ = NULL;
}

void TrendClassifyingFilterInterpreter::KState::Init(const FingerState& fs) {
  Init();
  x_ = fs.position_x, y_ = fs.position_y;
}

}
