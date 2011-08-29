// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/lookahead_filter_interpreter.h"

#include <algorithm>
#include <math.h>

using std::max;
using std::min;

namespace gestures {

namespace {
static const stime_t kMaxDelay = 0.09;  // 90ms
}

LookaheadFilterInterpreter::LookaheadFilterInterpreter(Interpreter* next)
    : max_fingers_per_hwstate_(0), interpreter_due_(-1.0),
      min_nonsuppress_speed_(200.0), min_nonsuppress_speed_prop_(NULL),
      delay_(0.05), delay_prop_(NULL) {
  next_.reset(next);
}

LookaheadFilterInterpreter::~LookaheadFilterInterpreter() {}

Gesture* LookaheadFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                                   stime_t* timeout) {
  // Push back into queue
  if (free_list_.Empty()) {
    Err("Can't accept new hwstate b/c we're out of nodes!");
    return NULL;
  }
  QState* node = free_list_.PopFront();
  node->set_state(*hwstate);
  double delay = max(0.0, min(kMaxDelay, delay_));
  node->due_ = hwstate->timestamp + delay;
  node->completed_ = false;
  queue_.PushBack(node);

  return HandleTimer(hwstate->timestamp, timeout);
}

Gesture* LookaheadFilterInterpreter::HandleTimer(stime_t now,
                                                 stime_t* timeout) {
  Gesture* result = NULL;
  stime_t next_timeout = -1.0;
  while (true) {
    if (interpreter_due_ > 0.0) {
      if (interpreter_due_ > now)
        break;  // Spurious callback
      if (result)
        Err("Losing gesture");
      next_timeout = -1.0;
      result = next_->HandleTimer(now, &next_timeout);
    } else {
      if (queue_.Empty())
        break;
      // Get next uncompleted and overdue hwstate
      QState* node = queue_.Head();
      while (node != queue_.Tail() && node->completed_)
        node = node->next_;
      if (node->completed_ || node->due_ > now)
        break;
      if (result)
        Err("Losing gesture");
      next_timeout = -1.0;
      result = next_->SyncInterpret(&node->state_, &next_timeout);

      // Clear previously completed nodes
      while (!queue_.Empty() && queue_.Head()->completed_)
        free_list_.PushBack(queue_.PopFront());

      // Mark current node completed. This should be the only completed
      // node in the queue.
      node->completed_ = true;
    }
    if (result && ShouldSuppressResult(result, queue_.Head()))
      result = NULL;
    UpdateInterpreterDue(next_timeout, now, timeout);
  }
  UpdateInterpreterDue(next_timeout, now, timeout);
  return result;
}

bool LookaheadFilterInterpreter::ShouldSuppressResult(const Gesture* gesture,
                                                      QState* node) {
  float distance_sq = 0.0;
  // Slow movements should potentially be suppressed
  switch (gesture->type) {
    case kGestureTypeMove:
      distance_sq = gesture->details.move.dx * gesture->details.move.dx +
          gesture->details.move.dy * gesture->details.move.dy;
      break;
    case kGestureTypeScroll:
      distance_sq = gesture->details.scroll.dx * gesture->details.scroll.dx +
          gesture->details.scroll.dy * gesture->details.scroll.dy;
      break;
    default:
      // Non-movement: just allow it.
      return false;
  }
  stime_t time_delta = gesture->end_time - gesture->start_time;
  float min_nonsuppress_dist_sq =
      min_nonsuppress_speed_ * min_nonsuppress_speed_ *
      time_delta * time_delta;
  if (distance_sq >= min_nonsuppress_dist_sq)
    return false;
  // Speed is slow. Suppress if fingers have changed.
  for (QState* iter = node->next_; iter != queue_.End(); iter = iter->next_) {
    if (node->state_.finger_cnt != iter->state_.finger_cnt)
      return true;
    for (size_t i = 0; i < node->state_.finger_cnt; ++i)
      if (!iter->state_.GetFingerState(node->state_.fingers[i].tracking_id))
        return true;
  }
  return false;
}

void LookaheadFilterInterpreter::UpdateInterpreterDue(
    stime_t new_interpreter_timeout,
    stime_t now,
    stime_t* timeout) {
  stime_t next_hwstate_timeout = -1.0;
  // Scan queue_ to find when next hwstate is due.
  for (QState* node = queue_.Begin(); node != queue_.End();
       node = node->next_) {
    if (node->completed_)
      continue;
    next_hwstate_timeout = node->due_ - now;
    break;
  }

  interpreter_due_ = -1.0;
  if (new_interpreter_timeout >= 0.0 &&
      (new_interpreter_timeout < next_hwstate_timeout ||
       next_hwstate_timeout < 0.0)) {
    interpreter_due_ = new_interpreter_timeout + now;
    *timeout = new_interpreter_timeout;
  } else if (next_hwstate_timeout >= 0.0) {
    *timeout = next_hwstate_timeout;
  }
}

void LookaheadFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  const size_t kMaxQNodes = 16;
  queue_.DeleteAll();
  free_list_.DeleteAll();
  for (size_t i = 0; i < kMaxQNodes; ++i) {
    QState* node = new QState(hwprops.max_finger_cnt);
    free_list_.PushBack(node);
  }
  next_->SetHardwareProperties(hwprops);
}

void LookaheadFilterInterpreter::Configure(GesturesPropProvider* pp,
                                           void* data) {
  next_->Configure(pp, data);
  min_nonsuppress_speed_prop_ = pp->create_real_fn(
      data, "Input Queue Min Nonsuppression Speed",
      &min_nonsuppress_speed_, min_nonsuppress_speed_);
  delay_prop_ = pp->create_real_fn(data, "Input Queue Delay", &delay_, delay_);
}

void LookaheadFilterInterpreter::Deconfigure(GesturesPropProvider* pp,
                                             void* data) {
  pp->free_fn(data, min_nonsuppress_speed_prop_);
  min_nonsuppress_speed_prop_ = NULL;
  pp->free_fn(data, delay_prop_);
  delay_prop_ = NULL;
  next_->Deconfigure(pp, data);
}

LookaheadFilterInterpreter::QState::QState()
    : max_fingers_(0), completed_(false), next_(NULL), prev_(NULL) {
  fs_.reset();
  state_.fingers = NULL;
}

LookaheadFilterInterpreter::QState::QState(unsigned short max_fingers)
    : max_fingers_(max_fingers), completed_(false), next_(NULL), prev_(NULL) {
  fs_.reset(new FingerState[max_fingers]);
  state_.fingers = fs_.get();
}

void LookaheadFilterInterpreter::QState::set_state(
    const HardwareState& new_state) {
  state_.timestamp = new_state.timestamp;
  state_.buttons_down = new_state.buttons_down;
  state_.touch_cnt = new_state.touch_cnt;
  unsigned short copy_count = new_state.finger_cnt;
  if (new_state.finger_cnt > max_fingers_) {
    Err("State with too many fingers! (%u vs %u)",
        new_state.finger_cnt,
        max_fingers_);
    copy_count = max_fingers_;
  }
  state_.finger_cnt = copy_count;
  std::copy(new_state.fingers, new_state.fingers + copy_count, state_.fingers);
}

}  // namespace gestures
