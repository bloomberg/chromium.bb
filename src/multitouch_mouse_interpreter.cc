// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/multitouch_mouse_interpreter.h"

#include <algorithm>

#include "gestures/include/mouse_interpreter.h"
#include "gestures/include/tracer.h"

namespace gestures {

void Origin::PushGesture(const Gesture& result) {
  if (result.type == kGestureTypeButtonsChange) {
    if (result.details.buttons.up & GESTURES_BUTTON_LEFT)
      button_going_up_left_ = result.end_time;
    if (result.details.buttons.up & GESTURES_BUTTON_MIDDLE)
      button_going_up_middle_ = result.end_time;
    if (result.details.buttons.up & GESTURES_BUTTON_RIGHT)
      button_going_up_right_ = result.end_time;
  }
}

stime_t Origin::ButtonGoingUp(int button) const {
  if (button == GESTURES_BUTTON_LEFT)
    return button_going_up_left_;
  if (button == GESTURES_BUTTON_MIDDLE)
    return button_going_up_middle_;
  if (button == GESTURES_BUTTON_RIGHT)
    return button_going_up_right_;
  return 0;
}

MultitouchMouseInterpreter::MultitouchMouseInterpreter(
    PropRegistry* prop_reg,
    Tracer* tracer)
    : Interpreter(NULL, tracer, false),
      state_buffer_(2),
      scroll_buffer_(15),
      prev_gesture_type_(kGestureTypeNull),
      current_gesture_type_(kGestureTypeNull),
      scroll_manager_(prop_reg),
      click_buffer_depth_(prop_reg, "Click Buffer Depth", 10),
      click_max_distance_(prop_reg, "Click Max Distance", 1.0),
      click_left_button_going_up_lead_time_(prop_reg,
          "Click Left Button Going Up Lead Time", 0.01),
      click_right_button_going_up_lead_time_(prop_reg,
          "Click Right Button Going Up Lead Time", 0.1),
      min_finger_move_distance_(prop_reg, "Minimum Mouse Finger Move Distance",
                                1.5),
      moving_min_rel_amount_(prop_reg, "Moving Min Rel Magnitude", 0.1) {
  InitName();
}

void MultitouchMouseInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                       stime_t* timeout) {
  if (!state_buffer_.Get(0)->fingers) {
    Err("Must call SetHardwareProperties() before interpreting anything.");
    return;
  }

  // Should we remove all fingers from our structures, or just removed ones?
  if ((hwstate->rel_x * hwstate->rel_x + hwstate->rel_y * hwstate->rel_y) >
      moving_min_rel_amount_.val_ * moving_min_rel_amount_.val_) {
    start_position_.clear();
    moving_.clear();
  } else {
    RemoveMissingIdsFromMap(&start_position_, *hwstate);
    RemoveMissingIdsFromSet(&moving_, *hwstate);
  }

  // Set start positions/moving
  for (size_t i = 0; i < hwstate->finger_cnt; i++) {
    const FingerState& fs = hwstate->fingers[i];
    if (MapContainsKey(start_position_, fs.tracking_id)) {
      // Is moving?
      if (!SetContainsValue(moving_, fs.tracking_id) &&  // not already moving &
          start_position_[fs.tracking_id].Sub(Vector2(fs)).MagSq() >=  // moving
          min_finger_move_distance_.val_ * min_finger_move_distance_.val_) {
        moving_.insert(fs.tracking_id);
      }
      continue;
    }
    start_position_[fs.tracking_id] = Vector2(fs);
  }

  // Mark all non-moving fingers as unable to cause scroll
  for (size_t i = 0; i < hwstate->finger_cnt; i++) {
    FingerState* fs = &hwstate->fingers[i];
    if (!SetContainsValue(moving_, fs->tracking_id))
      fs->flags |=
          GESTURES_FINGER_WARP_X_NON_MOVE | GESTURES_FINGER_WARP_Y_NON_MOVE;
  }

  // Record current HardwareState now.
  state_buffer_.PushState(*hwstate);

  // TODO(clchiou): Remove palm and thumb.
  gs_fingers_.clear();
  size_t num_fingers = std::min(kMaxGesturingFingers,
                                (size_t)state_buffer_.Get(0)->finger_cnt);
  const FingerState* fs = state_buffer_.Get(0)->fingers;
  for (size_t i = 0; i < num_fingers; i++)
    gs_fingers_.insert(fs[i].tracking_id);

  // Mouse events are given higher priority than multi-touch events.  The
  // interpreter first looks for mouse events.  If none of the mouse events
  // are found, the interpreter then looks for multi-touch events.
  result_.type = kGestureTypeNull;
  extra_result_.type = kGestureTypeNull;

  InterpretMouseEvent(*state_buffer_.Get(1), *state_buffer_.Get(0), &result_);
  origin_.PushGesture(result_);

  Gesture* result;
  if (result_.type == kGestureTypeNull)
    result = &result_;
  else
    result = &extra_result_;
  InterpretMultitouchEvent(result);
  origin_.PushGesture(*result);

  prev_gs_fingers_ = gs_fingers_;
  prev_gesture_type_ = current_gesture_type_;
  prev_result_ = result_;

  if (extra_result_.type != kGestureTypeNull)
    ProduceGesture(extra_result_);

  if (result_.type != kGestureTypeNull)
    ProduceGesture(result_);
}

void MultitouchMouseInterpreter::Initialize(
    const HardwareProperties* hw_props,
    Metrics* metrics,
    MetricsProperties* mprops,
    GestureConsumer* consumer) {
  Interpreter::Initialize(hw_props, metrics, mprops, consumer);
  state_buffer_.Reset(hw_props->max_finger_cnt);
}

void MultitouchMouseInterpreter::InterpretMultitouchEvent(Gesture* result) {
  // If a gesturing finger just left, do fling/lift
  if (current_gesture_type_ == kGestureTypeScroll &&
      AnyGesturingFingerLeft(*state_buffer_.Get(0),
                             prev_gs_fingers_)) {
    current_gesture_type_ = kGestureTypeFling;
    scroll_manager_.ComputeFling(state_buffer_, scroll_buffer_, result);
    if (result && result->type == kGestureTypeFling)
      result->details.fling.vx = 0.0;
  } else if (gs_fingers_.size() > 0) {
    // In general, finger movements are interpreted as scroll, but as
    // clicks and scrolls on multi-touch mice are both single-finger
    // gesture, we have to recognize and separate clicks from scrolls,
    // when a user is actually clicking.
    //
    // This is how we do for now: We look for characteristic patterns of
    // clicks, and if we find one, we hold off emitting scroll gesture for
    // a few time frames to prevent premature scrolls.
    //
    // The patterns we look for:
    // * Small finger movements when button is down
    // * Finger movements after button goes up

    bool update_scroll_buffer =
        scroll_manager_.ComputeScroll(state_buffer_,
                                      prev_gs_fingers_,
                                      gs_fingers_,
                                      prev_gesture_type_,
                                      prev_result_,
                                      result,
                                      &scroll_buffer_);
    current_gesture_type_ = result->type;

    bool hold_off_scroll = false;
    const HardwareState& state = *state_buffer_.Get(0);
    // Check small finger movements when button is down
    if (state.buttons_down) {
      float dist_sq, dt;
      scroll_buffer_.GetSpeedSq(click_buffer_depth_.val_, &dist_sq, &dt);
      if (dist_sq < click_max_distance_.val_ * click_max_distance_.val_)
        hold_off_scroll = true;
    }
    // Check button going up lead time
    stime_t now = state.timestamp;
    stime_t button_left_age =
        now - origin_.ButtonGoingUp(GESTURES_BUTTON_LEFT);
    stime_t button_right_age =
        now - origin_.ButtonGoingUp(GESTURES_BUTTON_RIGHT);
    hold_off_scroll = hold_off_scroll ||
        (button_left_age < click_left_button_going_up_lead_time_.val_) ||
        (button_right_age < click_right_button_going_up_lead_time_.val_);

    if (hold_off_scroll && result->type == kGestureTypeScroll) {
      current_gesture_type_ = kGestureTypeNull;
      result->type = kGestureTypeNull;
    }
    if (current_gesture_type_ == kGestureTypeScroll &&
        !update_scroll_buffer) {
      return;
    }
  }
  scroll_manager_.UpdateScrollEventBuffer(current_gesture_type_,
                                          &scroll_buffer_);
}

}  // namespace gestures
