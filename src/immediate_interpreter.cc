// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/immediate_interpreter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>

#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"
#include "gestures/include/util.h"

using std::bind1st;
using std::for_each;
using std::make_pair;
using std::max;
using std::mem_fun;
using std::min;

namespace gestures {

namespace {

float MaxMag(float a, float b) {
  if (fabsf(a) > fabsf(b))
    return a;
  return b;
}
float MinMag(float a, float b) {
  if (fabsf(a) < fabsf(b))
    return a;
  return b;
}

}  // namespace {}

void TapRecord::NoteTouch(short the_id, const FingerState& fs) {
  if (&fs == NULL) {
    Err("Error! Bad FingerState!");
    return;
  }
  // New finger must be close enough to an existing finger
  if (!touched_.empty()) {
    bool reject_new_finger = true;
    for (map<short, FingerState, kMaxTapFingers>::const_iterator it =
             touched_.begin(), e = touched_.end(); it != e; ++it) {
      const FingerState& existing_fs = (*it).second;
      if (immediate_interpreter_->FingersCloseEnoughToGesture(existing_fs,
                                                              fs)) {
        reject_new_finger = false;
        break;
      }
    }
    if (reject_new_finger)
      return;
  }
  touched_[the_id] = fs;
}

void TapRecord::NoteRelease(short the_id) {
  if (touched_.find(the_id) != touched_.end())
    released_.insert(the_id);
}

void TapRecord::Remove(short the_id) {
  min_tap_pressure_met_.erase(the_id);
  min_cotap_pressure_met_.erase(the_id);
  touched_.erase(the_id);
  released_.erase(the_id);
}

float TapRecord::CotapMinPressure() const {
  return immediate_interpreter_->tap_min_pressure() * 0.5;
}

void TapRecord::Update(const HardwareState& hwstate,
                       const HardwareState& prev_hwstate,
                       const set<short, kMaxTapFingers>& added,
                       const set<short, kMaxTapFingers>& removed,
                       const set<short, kMaxFingers>& dead) {
  Log("Updating TapRecord.");
  if (!t5r2_ && (hwstate.finger_cnt != hwstate.touch_cnt ||
                 prev_hwstate.finger_cnt != prev_hwstate.touch_cnt)) {
    // switch to T5R2 mode
    t5r2_ = true;
    t5r2_touched_size_ = touched_.size();
    t5r2_released_size_ = released_.size();
  }
  if (t5r2_) {
    short diff = static_cast<short>(hwstate.touch_cnt) -
        static_cast<short>(prev_hwstate.touch_cnt);
    if (diff > 0)
      t5r2_touched_size_ += diff;
    else if (diff < 0)
      t5r2_released_size_ += -diff;
  }
  for (set<short, kMaxTapFingers>::const_iterator it = added.begin(),
           e = added.end(); it != e; ++it)
    Log("Added: %d", *it);
  for (set<short, kMaxTapFingers>::const_iterator it = removed.begin(),
           e = removed.end(); it != e; ++it)
    Log("Removed: %d", *it);
  for (set<short, kMaxFingers>::const_iterator it = dead.begin(),
           e = dead.end(); it != e; ++it)
    Log("Dead: %d", *it);
  for_each(dead.begin(), dead.end(),
           bind1st(mem_fun(&TapRecord::Remove), this));
  for (set<short, kMaxTapFingers>::const_iterator it = added.begin(),
           e = added.end(); it != e; ++it)
    NoteTouch(*it, *hwstate.GetFingerState(*it));
  for_each(removed.begin(), removed.end(),
           bind1st(mem_fun(&TapRecord::NoteRelease), this));
  // Check if min tap/cotap pressure met yet
  const float cotap_min_pressure = CotapMinPressure();
  for (map<short, FingerState, kMaxTapFingers>::iterator it =
           touched_.begin(), e = touched_.end();
       it != e; ++it) {
    const FingerState* fs = hwstate.GetFingerState((*it).first);
    if (fs) {
      if (fs->pressure >= immediate_interpreter_->tap_min_pressure())
        min_tap_pressure_met_.insert(fs->tracking_id);
      if (fs->pressure >= cotap_min_pressure) {
        min_cotap_pressure_met_.insert(fs->tracking_id);
        if ((*it).second.pressure < cotap_min_pressure) {
          // Update existing record, since the old one hadn't met the cotap
          // pressure
          (*it).second = *fs;
        }
      }
    }
  }
  Log("Done Updating TapRecord.");
}

void TapRecord::Clear() {
  min_tap_pressure_met_.clear();
  min_cotap_pressure_met_.clear();
  t5r2_ = false;
  t5r2_touched_size_ = 0;
  t5r2_released_size_ = 0;
  touched_.clear();
  released_.clear();
}

bool TapRecord::Moving(const HardwareState& hwstate,
                       const float dist_max) const {
  const float cotap_min_pressure = CotapMinPressure();
  for (map<short, FingerState, kMaxTapFingers>::const_iterator it =
           touched_.begin(), e = touched_.end(); it != e; ++it) {
    const FingerState* fs = hwstate.GetFingerState((*it).first);
    if (!fs)
      continue;
    // Only look for moving when current frame meets cotap pressure and
    // our history contains a contact that's met cotap pressure.
    if (fs->pressure < cotap_min_pressure ||
        (*it).second.pressure < cotap_min_pressure)
      continue;
    // Compute distance moved
    float dist_x = fs->position_x - (*it).second.position_x;
    float dist_y = fs->position_y - (*it).second.position_y;
    bool moving =
        dist_x * dist_x + dist_y * dist_y > dist_max * dist_max;
    Log("Moving? x %f y %f (%s)", dist_x, dist_y, moving ? "Yes" : "No");
    if (moving)
      return true;
  }
  return false;
}

bool TapRecord::TapBegan() const {
  if (t5r2_)
    return t5r2_touched_size_ > 0;
  return !touched_.empty();
}

bool TapRecord::TapComplete() const {
  Log("called TapComplete()");
  bool ret = false;
  if (t5r2_)
    ret = t5r2_touched_size_ && t5r2_touched_size_ == t5r2_released_size_;
  else
    ret = !touched_.empty() && (touched_.size() == released_.size());
  for (map<short, FingerState, kMaxTapFingers>::const_iterator
           it = touched_.begin(), e = touched_.end(); it != e; ++it)
    Log("touched_: %d", (*it).first);
  for (set<short, kMaxTapFingers>::const_iterator it = released_.begin(),
           e = released_.end(); it != e; ++it)
    Log("released_: %d", *it);
  Log("TapComplete() returning %d", ret);
  return ret;
}

bool TapRecord::MinTapPressureMet() const {
  // True if any touching finger met minimum pressure
  return t5r2_ || !min_tap_pressure_met_.empty();
}

int TapRecord::TapType() const {
  size_t touched_size =
      t5r2_ ? t5r2_touched_size_ : min_cotap_pressure_met_.size();
  int ret = GESTURES_BUTTON_LEFT;
  if (touched_size > 1)
    ret = GESTURES_BUTTON_RIGHT;
  if (touched_size == 3 &&
      immediate_interpreter_->three_finger_click_enable_.val_ &&
      (!t5r2_ || immediate_interpreter_->t5r2_three_finger_click_enable_.val_))
    ret = GESTURES_BUTTON_MIDDLE;
  return ret;
}

// static
ScrollEvent ScrollEvent::Add(const ScrollEvent& evt_a,
                             const ScrollEvent& evt_b) {
  ScrollEvent ret = { evt_a.dx + evt_b.dx,
                      evt_a.dy + evt_b.dy,
                      evt_a.dt + evt_b.dt };
  return ret;
}

void ScrollEventBuffer::Insert(float dx, float dy, float dt) {
  head_ = (head_ + max_size_ - 1) % max_size_;
  buf_[head_].dx = dx;
  buf_[head_].dy = dy;
  buf_[head_].dt = dt;
  size_ = std::min(size_ + 1, max_size_);
}

void ScrollEventBuffer::Clear() {
  size_ = 0;
}

const ScrollEvent& ScrollEventBuffer::Get(size_t offset) const {
  if (offset >= size_) {
    Err("Out of bounds access!");
    // avoid returning null pointer
    static ScrollEvent dummy_event = { 0.0, 0.0, 0.0 };
    return dummy_event;
  }
  return buf_[(head_ + offset) % max_size_];
}

ImmediateInterpreter::ImmediateInterpreter(PropRegistry* prop_reg)
    : button_type_(0),
      sent_button_down_(false),
      button_down_timeout_(0.0),
      started_moving_time_(-1.0),
      finger_leave_time_(0.0),
      tap_to_click_state_(kTtcIdle),
      tap_to_click_state_entered_(0.0),
      tap_record_(this),
      last_movement_timestamp_(0.0),
      last_swipe_timestamp_(0.0),
      current_gesture_type_(kGestureTypeNull),
      scroll_buffer_(3),
      tap_enable_(prop_reg, "Tap Enable", false),
      tap_timeout_(prop_reg, "Tap Timeout", 0.2),
      inter_tap_timeout_(prop_reg, "Inter-Tap Timeout", 0.15),
      tap_drag_delay_(prop_reg, "Tap Drag Delay", 0.1),
      tap_drag_timeout_(prop_reg, "Tap Drag Timeout", 0.7),
      drag_lock_enable_(prop_reg, "Tap Drag Lock Enable", 0),
      tap_move_dist_(prop_reg, "Tap Move Distance", 2.0),
      tap_min_pressure_(prop_reg, "Tap Minimum Pressure", 25.0),
      three_finger_click_enable_(prop_reg, "Three Finger Click Enable", 0),
      t5r2_three_finger_click_enable_(prop_reg,
                                      "T5R2 Three Finger Click Enable",
                                      0),
      palm_pressure_(prop_reg, "Palm Pressure", 200.0),
      palm_edge_min_width_(prop_reg, "Tap Exclusion Border Width", 8.0),
      palm_edge_width_(prop_reg, "Palm Edge Zone Width", 14.0),
      palm_edge_point_speed_(prop_reg, "Palm Edge Zone Min Point Speed", 100.0),
      change_move_distance_(prop_reg, "Change Min Move Distance", 3.0),
      change_timeout_(prop_reg, "Change Timeout", 0.04),
      evaluation_timeout_(prop_reg, "Evaluation Timeout", 0.2),
      damp_scroll_min_movement_factor_(prop_reg,
                                       "Damp Scroll Min Move Factor",
                                       0.2),
      two_finger_pressure_diff_thresh_(prop_reg,
                                       "Two Finger Pressure Diff Thresh",
                                       32.0),
      two_finger_pressure_diff_factor_(prop_reg,
                                       "Two Finger Pressure Diff Factor",
                                       1.65),
      thumb_movement_factor_(prop_reg, "Thumb Movement Factor", 0.5),
      thumb_eval_timeout_(prop_reg, "Thumb Evaluation Timeout", 0.06),
      two_finger_close_horizontal_distance_thresh_(
          prop_reg,
          "Two Finger Horizontal Close Distance Thresh",
          50.0),
      two_finger_close_vertical_distance_thresh_(
          prop_reg,
          "Two Finger Vertical Close Distance Thresh",
          30.0),
      two_finger_scroll_distance_thresh_(prop_reg,
                                         "Two Finger Scroll Distance Thresh",
                                         2.0),
      three_finger_close_distance_thresh_(prop_reg,
                                          "Three Finger Close Distance Thresh",
                                          50.0),
      three_finger_swipe_distance_thresh_(prop_reg,
                                          "Three Finger Swipe Distance Thresh",
                                          10.0),
      max_pressure_change_(prop_reg, "Max Allowed Pressure Change", 30.0),
      scroll_stationary_finger_max_distance_(
          prop_reg, "Scroll Stationary Finger Max Distance", 1.0),
      bottom_zone_size_(prop_reg, "Bottom Zone Size", 10.0),
      button_evaluation_timeout_(prop_reg, "Button Evaluation Timeout", 0.03),
      keyboard_touched_timeval_high_(prop_reg, "Keyboard Touched Timeval High",
                                     0),
      keyboard_touched_timeval_low_(prop_reg, "Keyboard Touched Timeval Low",
                                    0, this),
      keyboard_touched_(0.0),
      keyboard_palm_prevent_timeout_(prop_reg, "Keyboard Palm Prevent Timeout",
                                     0.5),
      motion_tap_prevent_timeout_(prop_reg, "Motion Tap Prevent Timeout",
                                  0.05),
      tapping_finger_min_separation_(prop_reg, "Tap Min Separation", 10.0),
      vertical_scroll_snap_slope_(prop_reg, "Vertical Scroll Snap Slope",
                                  tanf(DegToRad(50.0))),  // 50 deg. from horiz.
      horizontal_scroll_snap_slope_(prop_reg, "Horizontal Scroll Snap Slope",
                                    tanf(DegToRad(30.0))),
      zoom_min_movement_(prop_reg, "Zoom Min Movement", 1.5),
      zoom_lock_min_movement_(prop_reg, "Zoom Lock Min Movement", 2.0),
      zoom_enable_(prop_reg, "Zoom Enable", 0.0) {
  memset(&prev_state_, 0, sizeof(prev_state_));
}

ImmediateInterpreter::~ImmediateInterpreter() {
  if (prev_state_.fingers) {
    free(prev_state_.fingers);
    prev_state_.fingers = NULL;
  }
}

Gesture* ImmediateInterpreter::SyncInterpret(HardwareState* hwstate,
                                             stime_t* timeout) {
  if (!prev_state_.fingers) {
    Err("Must call SetHardwareProperties() before Push().");
    return 0;
  }

  result_.type = kGestureTypeNull;
  const bool same_fingers = prev_state_.SameFingersAs(*hwstate) &&
      (hwstate->buttons_down == prev_state_.buttons_down);
  if (!same_fingers) {
    // Fingers changed, do nothing this time
    ResetSameFingersState(hwstate->timestamp);
    FillStartPositions(*hwstate);
  }

  if (hwstate->finger_cnt < prev_state_.finger_cnt)
    finger_leave_time_ = hwstate->timestamp;

  UpdatePalmState(*hwstate);
  UpdateThumbState(*hwstate);
  set<short, kMaxGesturingFingers> gs_fingers = GetGesturingFingers(*hwstate);

  UpdateStartedMovingTime(*hwstate, gs_fingers);
  UpdateButtons(*hwstate);
  UpdateTapGesture(hwstate,
                   gs_fingers,
                   same_fingers,
                   hwstate->timestamp,
                   timeout);

  UpdateCurrentGestureType(*hwstate, gs_fingers);
  if (result_.type == kGestureTypeNull)
    FillResultGesture(*hwstate, gs_fingers);

  SetPrevState(*hwstate);
  prev_gs_fingers_ = gs_fingers;
  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

Gesture* ImmediateInterpreter::HandleTimer(stime_t now, stime_t* timeout) {
  result_.type = kGestureTypeNull;
  UpdateTapGesture(NULL,
                   set<short, kMaxGesturingFingers>(),
                   false,
                   now,
                   timeout);
  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

void ImmediateInterpreter::ResetSameFingersState(stime_t now) {
  palm_.clear();
  pointing_.clear();
  fingers_.clear();
  start_positions_.clear();
  changed_time_ = now;
  two_finger_start_distance_ = -1;
}

bool ImmediateInterpreter::FingersCloseEnoughToGesture(
    const FingerState& finger_a,
    const FingerState& finger_b) const {
  float horiz_axis_sq = two_finger_close_horizontal_distance_thresh_.val_ *
      two_finger_close_horizontal_distance_thresh_.val_;
  float vert_axis_sq = two_finger_close_vertical_distance_thresh_.val_ *
      two_finger_close_vertical_distance_thresh_.val_;
  float dx = finger_b.position_x - finger_a.position_x;
  float dy = finger_b.position_y - finger_a.position_y;
  // Equation of ellipse:
  //    ,.--+--..
  //  ,'   V|    `.   x^2   y^2
  // |      +------|  --- + --- < 1
  //  \        H  /   H^2   V^2
  //   `-..__,,.-'
  return vert_axis_sq * dx * dx + horiz_axis_sq * dy * dy <
      vert_axis_sq * horiz_axis_sq;
}

bool ImmediateInterpreter::FingerNearOtherFinger(const HardwareState& hwstate,
                                                 size_t finger_idx) {
  const FingerState& fs = hwstate.fingers[finger_idx];
  for (int i = 0; i < hwstate.finger_cnt; ++i) {
    const FingerState& other_fs = hwstate.fingers[i];
    if (other_fs.tracking_id == fs.tracking_id)
      continue;
    bool too_close_to_other_finger =
        FingersCloseEnoughToGesture(fs, other_fs) &&
        !SetContainsValue(palm_, other_fs.tracking_id);
    if (too_close_to_other_finger)
      return true;
  }
  return false;
}

bool ImmediateInterpreter::FingerInPalmEdgeZone(const FingerState& fs) {
  return fs.position_x < palm_edge_width_.val_ ||
      fs.position_x > (hw_props_.right - palm_edge_width_.val_) ||
      fs.position_y < palm_edge_width_.val_ ||
      fs.position_y > (hw_props_.bottom - palm_edge_width_.val_);
}

bool ImmediateInterpreter::FingerInPalmEnvelope(const FingerState& fs) {
  float limit = palm_edge_min_width_.val_ +
      (fs.pressure / palm_pressure_.val_) *
      (palm_edge_width_.val_ - palm_edge_min_width_.val_);
  return fs.position_x < limit ||
      fs.position_x > (hw_props_.right - limit) ||
      fs.position_y < limit ||
      fs.position_y > (hw_props_.bottom - limit);
}

void ImmediateInterpreter::UpdatePalmState(const HardwareState& hwstate) {
  for (short i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    // Mark anything over the palm thresh as a palm
    if (fs.pressure >= palm_pressure_.val_) {
      palm_.insert(fs.tracking_id);
      pointing_.erase(fs.tracking_id);
      fingers_.erase(fs.tracking_id);
      continue;
    }
  }

  for (short i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    bool prev_palm = SetContainsValue(palm_, fs.tracking_id);
    bool prev_pointing = SetContainsValue(pointing_, fs.tracking_id);

    // Lock onto palm/pointing
    if (prev_palm || prev_pointing)
      continue;
    // If another finger is close by, let this be pointing
    if (FingerNearOtherFinger(hwstate, i) || !FingerInPalmEnvelope(fs))
      pointing_.insert(fs.tracking_id);
      fingers_.insert(fs.tracking_id);
  }
}

float ImmediateInterpreter::DistanceTravelledSq(const FingerState& fs) const {
  Point delta = FingerTraveledVector(fs);
  return delta.x_ * delta.x_ + delta.y_ * delta.y_;
}

ImmediateInterpreter::Point ImmediateInterpreter::FingerTraveledVector(
    const FingerState& fs) const {
  if (!MapContainsKey(start_positions_, fs.tracking_id))
    return Point(0.0f, 0.0f);
  const Point& start = start_positions_[fs.tracking_id];
  float dx = fs.position_x - start.x_;
  float dy = fs.position_y - start.y_;
  return Point(dx, dy);
}

float ImmediateInterpreter::TwoFingerDistanceSq(
    const HardwareState& hwstate) const {
  if (fingers_.size() == 2) {
    const FingerState* finger_a = hwstate.GetFingerState(*fingers_.begin());
    const FingerState* finger_b = hwstate.GetFingerState(*(fingers_.begin()+1));
    if (finger_a == NULL || finger_b == NULL) {
      Err("Finger unexpectedly NULL");
      return -1;
    }
    return DistSq(*finger_a, *finger_b);
  } else {
    return -1;
  }
}

// Updates thumb_ below.
void ImmediateInterpreter::UpdateThumbState(const HardwareState& hwstate) {
  // Remove old ids from thumb_
  RemoveMissingIdsFromMap(&thumb_, hwstate);
  float min_pressure = INFINITY;
  const FingerState* min_fs = NULL;
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    if (SetContainsValue(palm_, fs.tracking_id))
      continue;
    if (fs.pressure < min_pressure) {
      min_pressure = fs.pressure;
      min_fs = &fs;
    }
  }
  if (!min_fs)
    // Only palms on the touchpad
    return;
  float thumb_dist_sq_thresh = DistanceTravelledSq(*min_fs) *
      thumb_movement_factor_.val_ * thumb_movement_factor_.val_;
  // Make all large-pressure contacts located below the min-pressure
  // contact as thumbs.
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    if (SetContainsValue(palm_, fs.tracking_id))
      continue;
    if (fs.pressure > min_pressure + two_finger_pressure_diff_thresh_.val_ &&
        fs.pressure > min_pressure * two_finger_pressure_diff_factor_.val_ &&
        fs.position_y > min_fs->position_y &&
        DistanceTravelledSq(fs) < thumb_dist_sq_thresh) {
      if (!MapContainsKey(thumb_, fs.tracking_id))
        thumb_[fs.tracking_id] = hwstate.timestamp;
    } else if (MapContainsKey(thumb_, fs.tracking_id) &&
               hwstate.timestamp <
               max(started_moving_time_,
                   thumb_[fs.tracking_id]) + thumb_eval_timeout_.val_) {
      thumb_.erase(fs.tracking_id);
    }
  }
  for (map<short, stime_t, kMaxFingers>::const_iterator it = thumb_.begin();
       it != thumb_.end(); ++it)
    pointing_.erase((*it).first);
}

bool ImmediateInterpreter::KeyboardRecentlyUsed(stime_t now) const {
  // For tests, values of 0 mean keyboard not used recently.
  if (keyboard_touched_ == 0.0)
    return false;
  // Sanity check. If keyboard_touched_ is more than 10 seconds away from now,
  // ignore it.
  if (fabsf(now - keyboard_touched_) > 10)
    return false;

  return keyboard_touched_ + keyboard_palm_prevent_timeout_.val_ > now;
}

namespace {
struct GetGesturingFingersCompare {
  // Returns true if finger_a is strictly closer to keyboard than finger_b
  bool operator()(const FingerState* finger_a, const FingerState* finger_b) {
    return finger_a->position_y < finger_b->position_y;
  }
};
}  // namespace {}

set<short, kMaxGesturingFingers> ImmediateInterpreter::GetGesturingFingers(
    const HardwareState& hwstate) const {
  // We support up to kMaxGesturingFingers finger gestures
  if (pointing_.size() <= kMaxGesturingFingers)
    return pointing_;

  const FingerState* fs[hwstate.finger_cnt];
  for (size_t i = 0; i < hwstate.finger_cnt; ++i)
    fs[i] = &hwstate.fingers[i];

  // Pull the kMaxSize FingerStates w/ the lowest position_y to the
  // front of fs[].
  GetGesturingFingersCompare compare;
  set<short, kMaxGesturingFingers> ret;
  size_t sorted_cnt;
  if (hwstate.finger_cnt > kMaxGesturingFingers) {
    std::partial_sort(fs, fs + kMaxGesturingFingers, fs + hwstate.finger_cnt,
                      compare);
    sorted_cnt = kMaxGesturingFingers;
  } else {
    std::sort(fs, fs + hwstate.finger_cnt, compare);
    sorted_cnt = hwstate.finger_cnt;
  }
  for (size_t i = 0; i < sorted_cnt; i++)
    ret.insert(fs[i]->tracking_id);
  return ret;
}

void ImmediateInterpreter::UpdateCurrentGestureType(
    const HardwareState& hwstate,
    const set<short, kMaxGesturingFingers>& gs_fingers) {

  size_t num_gesturing = gs_fingers.size();

  // Physical button or tap overrides current gesture state
  if (sent_button_down_ || tap_to_click_state_ == kTtcDrag) {
    current_gesture_type_ = kGestureTypeMove;
    return;
  }

  // current gesture state machine
  switch(current_gesture_type_) {

    case kGestureTypeContactInitiated:
    case kGestureTypeButtonsChange:
      break;

    case kGestureTypeSwipe:
    case kGestureTypeFling:
    case kGestureTypeMove:
    case kGestureTypeNull:
      // When a finger leaves, we hold the gesture processing for
      // change_timeout_ time.
      if (hwstate.timestamp < finger_leave_time_ + change_timeout_.val_) {
        current_gesture_type_ = kGestureTypeNull;
        return;
      }

      // Scrolling detection for T5R2 devices
      if ((hw_props_.supports_t5r2 || hw_props_.support_semi_mt) &&
          (hwstate.touch_cnt > 2)) {
        current_gesture_type_ = kGestureTypeScroll;
        return;
      }

      // Finger gesture decision process
      if (num_gesturing == 0) {
        current_gesture_type_ = kGestureTypeNull;
      } else if (num_gesturing == 1) {
        current_gesture_type_ = kGestureTypeMove;
      } else {
        if (changed_time_ > started_moving_time_ ||
            hwstate.timestamp - started_moving_time_ <
            evaluation_timeout_.val_ ||
            current_gesture_type_ == kGestureTypeNull) {
          if (num_gesturing == 2) {
            const FingerState* fingers[] = {
              hwstate.GetFingerState(*gs_fingers.begin()),
              hwstate.GetFingerState(*(gs_fingers.begin() + 1))
            };
            if (!fingers[0] || !fingers[1]) {
              Err("Unable to find gesturing fingers!");
              return;
            }
            // See if two pointers are close together
            bool potential_two_finger_gesture =
                TwoFingersGesturing(*fingers[0], *fingers[1]);
            if (!potential_two_finger_gesture) {
              current_gesture_type_ = kGestureTypeMove;
            } else {
              current_gesture_type_ =
                  GetTwoFingerGestureType(*fingers[0], *fingers[1]);
            }
          } else if (num_gesturing == 3) {
            const FingerState* fingers[] = {
              hwstate.GetFingerState(*gs_fingers.begin()),
              hwstate.GetFingerState(*(gs_fingers.begin() + 1)),
              hwstate.GetFingerState(*(gs_fingers.begin() + 2))
            };
            if (!fingers[0] || !fingers[1] || !fingers[2]) {
              Err("Unable to find gesturing fingers!");
              return;
            }
            current_gesture_type_ = GetThreeFingerGestureType(fingers);
            if (current_gesture_type_ == kGestureTypeSwipe)
              last_swipe_timestamp_ = hwstate.timestamp;
          } else {
            Log("TODO(adlr): support > 3 finger gestures.");
          }
        }
      }

      if ((current_gesture_type_ == kGestureTypeMove ||
           current_gesture_type_ == kGestureTypeNull) &&
          zoom_enable_.val_) {
        // Calculate and cache distance between fingers at start of gesture.
        if (two_finger_start_distance_ < 0) {
          two_finger_start_distance_ = sqrtf(TwoFingerDistanceSq(hwstate));
        }
        current_gesture_type_ = DeterminePotentialZoomGestureType(hwstate);
      }
      break;

    case kGestureTypeScroll:
      // If a scrolling finger just left, do fling
      for (set<short, kMaxGesturingFingers>::const_iterator
           it = prev_scroll_fingers_.begin(), e = prev_scroll_fingers_.end();
           it != e; ++it) {
        if (!hwstate.GetFingerState(*it)) {
          current_gesture_type_ = kGestureTypeFling;
          return;
        }
      }
      break;
    case kGestureTypeZoom:
      if (fingers_.size() == 2) {
        return;
      } else {
        current_gesture_type_ = kGestureTypeNull;
      }
      break;
  }
}

GestureType ImmediateInterpreter::DeterminePotentialZoomGestureType(
    const HardwareState& hwstate) const {

  if (fingers_.size() != 2) {
    return current_gesture_type_;
  }

  const FingerState* finger1 = hwstate.GetFingerState(*fingers_.begin());
  const FingerState* finger2 = hwstate.GetFingerState(*(fingers_.begin()+1));
  if (finger1 == NULL || finger2 == NULL) {
    Err("Finger unexpectedly NULL");
    return current_gesture_type_;
  }

  if (!MapContainsKey(start_positions_, finger1->tracking_id) ||
      !MapContainsKey(start_positions_, finger2->tracking_id)) {
    return current_gesture_type_;
  }

  float zoom_min_mov_sq = zoom_min_movement_.val_ *
      zoom_min_movement_.val_;
  float zoom_lock_min_mov_sq = zoom_lock_min_movement_.val_ *
      zoom_lock_min_movement_.val_;

  if (changed_time_ > started_moving_time_ ||
      hwstate.timestamp - started_moving_time_ < evaluation_timeout_.val_) {
    Point delta1 = FingerTraveledVector(*finger1);
    Point delta2 = FingerTraveledVector(*finger2);

    // dot > 0 for fingers moving in a similar direction
    // dot < 0 for fingers moving in opposite directions
    float dot  = delta1.x_ * delta2.x_ + delta1.y_ * delta2.y_;
    float d1sq = delta1.x_ * delta1.x_ + delta1.y_ * delta1.y_;
    float d2sq = delta2.x_ * delta2.x_ + delta2.y_ * delta2.y_;

    if (d1sq > zoom_lock_min_mov_sq && d2sq > zoom_lock_min_mov_sq && dot < 0)
      return kGestureTypeZoom;  // pinch zoom accepted and locked.

    if (d1sq > zoom_min_mov_sq && d2sq > zoom_min_mov_sq && dot < 0)
      return kGestureTypeNull;  // possible pinch zoom. Don't move cursor.
  }
  return current_gesture_type_;
}

bool ImmediateInterpreter::TwoFingersGesturing(
    const FingerState& finger1,
    const FingerState& finger2) const {
  // Make sure the pressure difference isn't too great for vertically
  // aligned contacts
  float pdiff = fabsf(finger1.pressure - finger2.pressure);
  float xdist = fabsf(finger1.position_x - finger2.position_x);
  float ydist = fabsf(finger1.position_y - finger2.position_y);
  if (pdiff > two_finger_pressure_diff_thresh_.val_ && ydist > xdist)
    return false;

  // Next, make sure distance between fingers isn't too great
  if (!FingersCloseEnoughToGesture(finger1, finger2))
    return false;

  const float kMin2fDistThreshSq = tapping_finger_min_separation_.val_ *
      tapping_finger_min_separation_.val_;
  float dist_sq = xdist * xdist + ydist * ydist;
  // Make sure distance between fingers isn't too small
  if (dist_sq < kMin2fDistThreshSq)
    return false;

  // Next, if fingers are vertically aligned and one is in the bottom zone,
  // consider that one a resting thumb (thus, do not scroll/right click)
  if (xdist < ydist && (FingerInDampenedZone(finger1) ||
                        FingerInDampenedZone(finger2)))
    return false;
  return true;
}

GestureType ImmediateInterpreter::GetTwoFingerGestureType(
    const FingerState& finger1,
    const FingerState& finger2) {
  if (!MapContainsKey(start_positions_, finger1.tracking_id) ||
      !MapContainsKey(start_positions_, finger2.tracking_id))
    return kGestureTypeNull;

  // Compute distance traveled since fingers changed for each finger
  float dx1 = finger1.position_x - start_positions_[finger1.tracking_id].x_;
  float dy1 = finger1.position_y - start_positions_[finger1.tracking_id].y_;
  float dx2 = finger2.position_x - start_positions_[finger2.tracking_id].x_;
  float dy2 = finger2.position_y - start_positions_[finger2.tracking_id].y_;

  float large_dx = MaxMag(dx1, dx2);
  float large_dy = MaxMag(dy1, dy2);
  float small_dx = MinMag(dx1, dx2);
  float small_dy = MinMag(dy1, dy2);

  bool dampened_zone_occupied = false;
  // movements of the finger in the dampened zone. If there are multiple
  // fingers in the dampened zone, dx is min(dx_1, dx_2), dy is min(dy_1, dy_2).
  float damp_dx = INFINITY;
  float damp_dy = INFINITY;
  float non_damp_dx = 0.0;
  float non_damp_dy = 0.0;
  if (FingerInDampenedZone(finger1)) {
    dampened_zone_occupied = true;
    damp_dx = dx1;
    damp_dy = dy1;
    non_damp_dx = dx2;
    non_damp_dy = dy2;
  }
  if (FingerInDampenedZone(finger2)) {
    dampened_zone_occupied = true;
    damp_dx = MinMag(damp_dx, dx2);
    damp_dy = MinMag(damp_dy, dy2);
    non_damp_dx = MaxMag(non_damp_dx, dx1);
    non_damp_dy = MaxMag(non_damp_dy, dy1);
  }

  if (fabsf(large_dx) > fabsf(large_dy)) {
    // consider horizontal scroll
    if (fabsf(large_dx) < two_finger_scroll_distance_thresh_.val_)
      return kGestureTypeNull;
    if (fabsf(small_dx) < scroll_stationary_finger_max_distance_.val_)
      small_dx = 0.0;
    if (large_dx * small_dx < 0.0)
      return kGestureTypeMove;  // not same direction
    if (dampened_zone_occupied) {
      // Require damp to move at least some amount with the other finger
      if (fabsf(damp_dx) <
          damp_scroll_min_movement_factor_.val_ * fabsf(non_damp_dx)) {
        return kGestureTypeMove;
      }
    }
    return kGestureTypeScroll;
  } else {
    // consider vertical scroll
    if (fabsf(large_dy) < two_finger_scroll_distance_thresh_.val_)
      return kGestureTypeNull;
    if (fabsf(small_dy) < scroll_stationary_finger_max_distance_.val_)
      small_dy = 0.0;
    if (large_dy * small_dy < 0.0)
      return kGestureTypeMove;
    if (dampened_zone_occupied) {
      // Require damp to move at least some amount with the other finger
      if (fabsf(damp_dy) <
          damp_scroll_min_movement_factor_.val_ * fabsf(non_damp_dy)) {
        return kGestureTypeMove;
      }
    }
    return kGestureTypeScroll;
  }
}

GestureType ImmediateInterpreter::GetThreeFingerGestureType(
    const FingerState* const fingers[3]) {
  const FingerState* x_fingers[] = { fingers[0], fingers[1], fingers[2] };
  const FingerState* y_fingers[] = { fingers[0], fingers[1], fingers[2] };
  qsort(x_fingers, 3, sizeof(*x_fingers), CompareX<FingerState>);
  qsort(y_fingers, 3, sizeof(*y_fingers), CompareY<FingerState>);

  bool horizontal =
      (x_fingers[2]->position_x - x_fingers[0]->position_x) >=
      (y_fingers[2]->position_y - y_fingers[0]->position_y);
  const FingerState* min_finger = horizontal ? x_fingers[0] : y_fingers[0];
  const FingerState* center_finger = horizontal ? x_fingers[1] : y_fingers[1];
  const FingerState* max_finger = horizontal ? x_fingers[2] : y_fingers[2];

  if (DistSq(*min_finger, *max_finger) >
      three_finger_close_distance_thresh_.val_ *
      three_finger_close_distance_thresh_.val_) {
    return kGestureTypeNull;
  }

  double min_finger_dx =
      min_finger->position_x - start_positions_[min_finger->tracking_id].x_;
  double center_finger_dx =
      center_finger->position_x -
      start_positions_[center_finger->tracking_id].x_;
  double max_finger_dx =
      max_finger->position_x - start_positions_[max_finger->tracking_id].x_;

  // All three fingers must move in the same direction.
  if ((min_finger_dx > 0 && !(center_finger_dx > 0 && max_finger_dx > 0)) ||
      (min_finger_dx < 0 && !(center_finger_dx < 0 && max_finger_dx < 0))) {
    return kGestureTypeNull;
  }

  // All three fingers must have traveled far enough.
  if (fabsf(min_finger_dx) < three_finger_swipe_distance_thresh_.val_ ||
      fabsf(min_finger_dx) < three_finger_swipe_distance_thresh_.val_ ||
      fabsf(min_finger_dx) < three_finger_swipe_distance_thresh_.val_) {
    return kGestureTypeNull;
  }

  // Only produce the swipe gesture once per state change.
  if (last_swipe_timestamp_ >= changed_time_)
    return kGestureTypeNull;

  return kGestureTypeSwipe;
}

const char* ImmediateInterpreter::TapToClickStateName(TapToClickState state) {
  switch (state) {
    case kTtcIdle: return "Idle";
    case kTtcFirstTapBegan: return "FirstTapBegan";
    case kTtcTapComplete: return "TapComplete";
    case kTtcSubsequentTapBegan: return "SubsequentTapBegan";
    case kTtcDrag: return "Drag";
    case kTtcDragRelease: return "DragRelease";
    case kTtcDragRetouch: return "DragRetouch";
    default: return "<unknown>";
  }
}

stime_t ImmediateInterpreter::TimeoutForTtcState(TapToClickState state) {
  switch (state) {
    case kTtcIdle: return tap_timeout_.val_;
    case kTtcFirstTapBegan: return tap_timeout_.val_;
    case kTtcTapComplete: return inter_tap_timeout_.val_;
    case kTtcSubsequentTapBegan: return tap_timeout_.val_;
    case kTtcDrag: return tap_timeout_.val_;
    case kTtcDragRelease: return tap_drag_timeout_.val_;
    case kTtcDragRetouch: return tap_timeout_.val_;
    default:
      Log("Unknown state!");
      return 0.0;
  }
}

void ImmediateInterpreter::SetTapToClickState(TapToClickState state,
                                              stime_t now) {
  if (tap_to_click_state_ != state) {
    tap_to_click_state_ = state;
    tap_to_click_state_entered_ = now;
  }
}

void ImmediateInterpreter::UpdateTapGesture(
    const HardwareState* hwstate,
    const set<short, kMaxGesturingFingers>& gs_fingers,
    const bool same_fingers,
    stime_t now,
    stime_t* timeout) {
  unsigned down = 0;
  unsigned up = 0;
  UpdateTapState(hwstate, gs_fingers, same_fingers, now, &down, &up, timeout);
  if (down == 0 && up == 0) {
    Log("No tap gesture");
    return;
  }
  Log("Yes tap gesture");
  result_ = Gesture(kGestureButtonsChange,
                    prev_state_.timestamp,
                    now,
                    down,
                    up);
}

void ImmediateInterpreter::UpdateTapState(
    const HardwareState* hwstate,
    const set<short, kMaxGesturingFingers>& gs_fingers,
    const bool same_fingers,
    stime_t now,
    unsigned* buttons_down,
    unsigned* buttons_up,
    stime_t* timeout) {
  if (tap_to_click_state_ == kTtcIdle && !tap_enable_.val_)
    return;
  Log("Entering UpdateTapState");

  set<short, kMaxGesturingFingers> tap_gs_fingers;

  if (hwstate) {
    for (int i = 0; i < hwstate->finger_cnt; ++i)
      Log("HWSTATE: %d", hwstate->fingers[i].tracking_id);
    for (set<short, kMaxGesturingFingers>::const_iterator it =
             gs_fingers.begin(), e = gs_fingers.end(); it != e; ++it) {
      const FingerState* fs = hwstate->GetFingerState(*it);
      if (!fs) {
        Err("Missing finger state?!");
        continue;
      }
      Log("GS: %d", *it);
      tap_gs_fingers.insert(*it);
    }
  }
  set<short, kMaxTapFingers> added_fingers;

  // Fingers removed from the pad entirely
  set<short, kMaxTapFingers> removed_fingers;

  // Fingers that were gesturing, but now aren't
  set<short, kMaxFingers> dead_fingers;

  const bool phys_button_down = hwstate && hwstate->buttons_down != 0;

  bool is_timeout = (now - tap_to_click_state_entered_ >
                     TimeoutForTtcState(tap_to_click_state_));

  if (hwstate && (!same_fingers || prev_tap_gs_fingers_ != tap_gs_fingers)) {
    // See if fingers were added
    for (set<short, kMaxGesturingFingers>::const_iterator it =
             tap_gs_fingers.begin(), e = tap_gs_fingers.end(); it != e; ++it)
      if (!SetContainsValue(prev_tap_gs_fingers_, *it)) {
        // Gesturing finger wasn't in prev state. It's new.
        const FingerState* fs = hwstate->GetFingerState(*it);
        if (FingerTooCloseToTap(*hwstate, *fs) ||
            FingerTooCloseToTap(prev_state_, *fs))
          continue;
        if (fs->flags & GESTURES_FINGER_NO_TAP)
          continue;
        added_fingers.insert(*it);
        Log("TTC: Added %d", *it);
      }

    // See if fingers were removed or are now non-gesturing (dead)
    for (set<short, kMaxGesturingFingers>::const_iterator it =
             prev_tap_gs_fingers_.begin(), e = prev_tap_gs_fingers_.end();
         it != e; ++it) {
      if (tap_gs_fingers.find(*it) != tap_gs_fingers.end())
        // still gesturing; neither removed nor dead
        continue;
      if (!hwstate->GetFingerState(*it)) {
        // Previously gesturing finger isn't in current state. It's gone.
        removed_fingers.insert(*it);
        Log("TTC: Removed %d", *it);
      } else {
        // Previously gesturing finger is in current state. It's dead.
        dead_fingers.insert(*it);
        Log("TTC: Dead %d", *it);
      }
    }
  }

  prev_tap_gs_fingers_ = tap_gs_fingers;

  // The state machine:

  // If you are updating the code, keep this diagram correct.
  // We have a TapRecord which stores current tap state.
  // Also, if the physical button is down, we go to (or stay in) Idle state.

  //     Start
  //       ↓
  //    [Idle**] <----------------------------------------------------------,
  //       ↓ added finger(s)                                                |
  //  ,>[FirstTapBegan] -<right click: send right click, timeout/movement>->|
  //  |    ↓ released all fingers                                           |
  // ,->[TapComplete*] --<timeout: send click>----------------------------->|
  // ||    | | two finger touching: send left click.                        |
  // |'----+-'                                                              |
  // |     ↓ add finger(s)                                                  |
  // |  [SubsequentTapBegan] --<timeout/move w/o delay: send click>------->|
  // |     | | released all fingers send button click                       |
  // |<----+-'                                                              |
  // |     ↓ timeout/movement with delay: send button down                  |
  // | ,->[Drag] --<detect 2 finger gesture: send button up>--------------->|
  // | |   ↓ release all fingers                                            |
  // | |  [DragRelease*]  --<timeout: send button up>---------------------->|
  // | |   ↓ add finger(s)                                                  |
  // | |  [DragRetouch]  --<remove fingers (left tap): send button up>----->|
  // | |   | | timeout/movement
  // | '---+-'
  // |     |  remove all fingers (non-left tap): send button up
  // '-----'
  //
  // * When entering TapComplete or DragRelease, we set a timer, since
  //   we will have no fingers on the pad and want to run possibly before
  //   fingers are put on the pad. Note that we use different timeouts
  //   based on which state we're in (tap_timeout_ or tap_drag_timeout_).
  // ** When entering idle, we reset the TapRecord.

  Log("TTC State: %s", TapToClickStateName(tap_to_click_state_));
  if (!hwstate)
    Log("This is a timer callback");
  if (phys_button_down || KeyboardRecentlyUsed(now)) {
    Log("Physical button down or keyboard recently used. Going to Idle state");
    SetTapToClickState(kTtcIdle, now);
    return;
  }

  switch (tap_to_click_state_) {
    case kTtcIdle:
      tap_record_.Clear();
      if (hwstate &&
          hwstate->timestamp - last_movement_timestamp_ >=
          motion_tap_prevent_timeout_.val_) {
        tap_record_.Update(
            *hwstate, prev_state_, added_fingers, removed_fingers,
            dead_fingers);
        if (tap_record_.TapBegan())
          SetTapToClickState(kTtcFirstTapBegan, now);
      }
      break;
    case kTtcFirstTapBegan:
      if (is_timeout) {
        SetTapToClickState(kTtcIdle, now);
        break;
      }
      if (!hwstate) {
        Log("hwstate NULL but no timeout?!");
        break;
      }
      tap_record_.Update(
          *hwstate, prev_state_, added_fingers, removed_fingers, dead_fingers);
      Log("Is tap? %d Is moving? %d",
          tap_record_.TapComplete(),
          tap_record_.Moving(*hwstate, tap_move_dist_.val_));
      if (tap_record_.TapComplete()) {
        if (!tap_record_.MinTapPressureMet()) {
          SetTapToClickState(kTtcIdle, now);
        } else if (tap_record_.TapType() == GESTURES_BUTTON_LEFT) {
          SetTapToClickState(kTtcTapComplete, now);
        } else {
          *buttons_down = *buttons_up = tap_record_.TapType();
          SetTapToClickState(kTtcIdle, now);
        }
      } else if (tap_record_.Moving(*hwstate, tap_move_dist_.val_)) {
        SetTapToClickState(kTtcIdle, now);
      }
      break;
    case kTtcTapComplete:
      if (!added_fingers.empty()) {

        tap_record_.Clear();
        tap_record_.Update(
            *hwstate, prev_state_, added_fingers, removed_fingers,
            dead_fingers);

        // If more than one finger is touching: Send click
        // and return to FirstTapBegan state.
        if (tap_record_.TapType() != GESTURES_BUTTON_LEFT) {
          *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(kTtcFirstTapBegan, now);
        } else {
          SetTapToClickState(kTtcSubsequentTapBegan, now);
        }
      } else if (is_timeout) {
        *buttons_down = *buttons_up =
            tap_record_.MinTapPressureMet() ? tap_record_.TapType() : 0;
        SetTapToClickState(kTtcIdle, now);
      }
      break;
    case kTtcSubsequentTapBegan:
      if (!is_timeout && !hwstate) {
        Log("hwstate NULL but not a timeout?!");
        break;
      }
      if (hwstate)
        tap_record_.Update(
            *hwstate, prev_state_, added_fingers, removed_fingers,
            dead_fingers);
      if (is_timeout || tap_record_.Moving(*hwstate, tap_move_dist_.val_)) {
        if (tap_record_.TapType() == GESTURES_BUTTON_LEFT) {
          if (is_timeout) {
            // moving with just one finger. Start dragging.
            *buttons_down = GESTURES_BUTTON_LEFT;
            SetTapToClickState(kTtcDrag, now);
          } else {
            bool drag_delay_met = (now - tap_to_click_state_entered_
                                   > tap_drag_delay_.val_);
            if (drag_delay_met) {
              *buttons_down = GESTURES_BUTTON_LEFT;
              SetTapToClickState(kTtcDrag, now);
            } else {
              *buttons_down = GESTURES_BUTTON_LEFT;
              *buttons_up = GESTURES_BUTTON_LEFT;
              SetTapToClickState(kTtcIdle, now);
            }
          }
        } else if (!tap_record_.TapComplete()) {
          // not just one finger. Send button click and go to idle.
          *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(kTtcIdle, now);
        }
        break;
      }
      if (tap_record_.TapComplete()) {
        *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(kTtcTapComplete, now);
        Log("Subsequent left tap complete");
      }
      break;
    case kTtcDrag:
      if (hwstate)
        tap_record_.Update(
            *hwstate, prev_state_, added_fingers, removed_fingers,
            dead_fingers);
      if (tap_record_.TapComplete()) {
        tap_record_.Clear();
        if (drag_lock_enable_.val_) {
          SetTapToClickState(kTtcDragRelease, now);
        } else {
          *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(kTtcIdle, now);
        }
      }
      if (tap_record_.TapType() != GESTURES_BUTTON_LEFT &&
          now - tap_to_click_state_entered_ <= evaluation_timeout_.val_) {
        // We thought we were dragging, but actually we're doing a
        // non-tap-to-click multitouch gesture.
        *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(kTtcIdle, now);
      }
      break;
    case kTtcDragRelease:
      if (!added_fingers.empty()) {
        tap_record_.Update(
            *hwstate, prev_state_, added_fingers, removed_fingers,
            dead_fingers);
        SetTapToClickState(kTtcDragRetouch, now);
      } else if (is_timeout) {
        *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(kTtcIdle, now);
      }
      break;
    case kTtcDragRetouch:
      if (hwstate)
        tap_record_.Update(
            *hwstate, prev_state_, added_fingers, removed_fingers,
            dead_fingers);
      if (tap_record_.TapComplete()) {
        *buttons_up = GESTURES_BUTTON_LEFT;
        if (tap_record_.TapType() == GESTURES_BUTTON_LEFT)
          SetTapToClickState(kTtcIdle, now);
        else
          SetTapToClickState(kTtcTapComplete, now);
        break;
      }
      if (is_timeout) {
        SetTapToClickState(kTtcDrag, now);
        break;
      }
      if (!hwstate) {
        Log("not timeout but hwstate is NULL?!");
        break;
      }
      if (tap_record_.Moving(*hwstate, tap_move_dist_.val_))
        SetTapToClickState(kTtcDrag, now);
      break;
  }
  Log("TTC: New state: %s", TapToClickStateName(tap_to_click_state_));
  // Take action based on new state:
  switch (tap_to_click_state_) {
    case kTtcTapComplete:
      *timeout = TimeoutForTtcState(tap_to_click_state_);
      break;
    case kTtcDragRelease:
      *timeout = TimeoutForTtcState(tap_to_click_state_);
      break;
    default:  // so gcc doesn't complain about missing enums
      break;
  }
}

bool ImmediateInterpreter::FingerTooCloseToTap(const HardwareState& hwstate,
                                               const FingerState& fs) {
  const float kMinAllowableSq =
      tapping_finger_min_separation_.val_ * tapping_finger_min_separation_.val_;
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState* iter_fs = &hwstate.fingers[i];
    if (iter_fs->tracking_id == fs.tracking_id)
      continue;
    float dist_sq = DistSq(fs, *iter_fs);
    if (dist_sq < kMinAllowableSq)
      return true;
  }
  return false;
}

void ImmediateInterpreter::SetPrevState(const HardwareState& hwstate) {
  prev_state_.timestamp = hwstate.timestamp;
  prev_state_.buttons_down = hwstate.buttons_down;
  prev_state_.touch_cnt = hwstate.touch_cnt;
  prev_state_.finger_cnt = min(hwstate.finger_cnt, hw_props_.max_finger_cnt);
  memcpy(prev_state_.fingers,
         hwstate.fingers,
         prev_state_.finger_cnt * sizeof(FingerState));
}

bool ImmediateInterpreter::FingerInDampenedZone(
    const FingerState& finger) const {
  // TODO(adlr): cache thresh
  float thresh = hw_props_.bottom - bottom_zone_size_.val_;
  return finger.position_y > thresh;
}

void ImmediateInterpreter::FillStartPositions(const HardwareState& hwstate) {
  for (short i = 0; i < hwstate.finger_cnt; i++)
    start_positions_[hwstate.fingers[i].tracking_id] =
        Point(hwstate.fingers[i].position_x, hwstate.fingers[i].position_y);
}

int ImmediateInterpreter::EvaluateButtonType(
    const HardwareState& hwstate) {
  if (hw_props_.supports_t5r2 && hwstate.touch_cnt > 2) {
    if (hwstate.touch_cnt - thumb_.size() == 3 &&
        three_finger_click_enable_.val_ && t5r2_three_finger_click_enable_.val_)
      return GESTURES_BUTTON_MIDDLE;
    return GESTURES_BUTTON_RIGHT;
  }
  int num_pointing = pointing_.size();
  if (num_pointing <= 1)
    return GESTURES_BUTTON_LEFT;
  if (current_gesture_type_ == kGestureTypeScroll)
    return GESTURES_BUTTON_RIGHT;
  if (num_pointing == 3 && three_finger_click_enable_.val_)
    return GESTURES_BUTTON_MIDDLE;
  if (num_pointing > 2) {
    Log("TODO: handle more advanced touchpads.");
    return GESTURES_BUTTON_LEFT;
  }

  // If we get to here, then:
  // pointing_.size() == 2 && current_gesture_type_ != kGestureTypeScroll.
  // Find which two fingers are performing the gesture.
  const FingerState* finger1 = hwstate.GetFingerState(*pointing_.begin());
  const FingerState* finger2 = hwstate.GetFingerState(*(pointing_.begin() + 1));

  return TwoFingersGesturing(*finger1, *finger2) ?
      GESTURES_BUTTON_RIGHT : GESTURES_BUTTON_LEFT;
}

void ImmediateInterpreter::UpdateStartedMovingTime(
    const HardwareState& hwstate,
    const set<short, kMaxGesturingFingers>& gs_fingers) {
  if (started_moving_time_ > changed_time_)
    return;  // Already started moving
  const float kMinDistSq =
      change_move_distance_.val_ * change_move_distance_.val_;
  for (set<short, kMaxGesturingFingers>::const_iterator
           it = gs_fingers.begin(), e = gs_fingers.end(); it != e; ++it) {
    const FingerState* fs = hwstate.GetFingerState(*it);
    if (!fs) {
      Err("Missing hardware state!");
      continue;
    }
    if (!MapContainsKey(start_positions_, *it)) {
      Err("Missing start position!");
      continue;
    }
    const Point& start_position = start_positions_[*it];
    float dist_sq = DistSqXY(*fs, start_position.x_, start_position.y_);
    if (dist_sq > kMinDistSq) {
      started_moving_time_ = hwstate.timestamp;
      return;
    }
  }
}

void ImmediateInterpreter::UpdateButtons(const HardwareState& hwstate) {
  // Current hardware will only ever send a physical left-button down.
  bool prev_button_down = prev_state_.buttons_down;
  bool button_down = hwstate.buttons_down;
  if (!prev_button_down && !button_down)
    return;
  bool phys_down_edge = button_down && !prev_button_down;
  bool phys_up_edge = !button_down && prev_button_down;

  if (phys_down_edge) {
    button_type_ = GESTURES_BUTTON_LEFT;
    sent_button_down_ = false;
    button_down_timeout_ = hwstate.timestamp + button_evaluation_timeout_.val_;
  }
  if (!sent_button_down_) {
    button_type_ = EvaluateButtonType(hwstate);
    // We send non-left buttons immediately, but delay left in case future
    // packets indicate non-left button.
    if (button_type_ != GESTURES_BUTTON_LEFT ||
        button_down_timeout_ <= hwstate.timestamp ||
        phys_up_edge) {
      // Send button down
      if (result_.type == kGestureTypeButtonsChange)
        Err("Gesture type already button?!");
      result_ = Gesture(kGestureButtonsChange,
                        prev_state_.timestamp,
                        hwstate.timestamp,
                        button_type_,
                        0);
      sent_button_down_ = true;
    }
  }
  if (phys_up_edge) {
    // Send button up
    if (result_.type != kGestureTypeButtonsChange)
      result_ = Gesture(kGestureButtonsChange,
                        prev_state_.timestamp,
                        hwstate.timestamp,
                        0,
                        button_type_);
    else
      result_.details.buttons.up = button_type_;
    // Reset button state
    button_type_ = 0;
    button_down_timeout_ = 0;
    sent_button_down_ = false;
  }
}

namespace {
float IncreasingSpeed(float dist, float dt,
                      float prev_dist, float prev_dt) {
  return fabsf(dist) * prev_dt > fabsf(prev_dist) * dt;
}
float DecreasingSpeed(float dist, float dt,
                      float prev_dist, float prev_dt) {
  return fabsf(dist) * prev_dt < fabsf(prev_dist) * dt;
}
}  // namespace {}

size_t ImmediateInterpreter::ScrollEventsForFlingCount() const {
  if (scroll_buffer_.Size() <= 1)
    return scroll_buffer_.Size();
  enum Direction { kNone, kUp, kDown, kLeft, kRight };
  size_t i = 0;
  Direction prev_direction = kNone;
  for (; i < scroll_buffer_.Size(); i++) {
    const ScrollEvent& event = scroll_buffer_.Get(i);
    if (FloatEq(event.dx, 0.0) && FloatEq(event.dy, 0.0))
      break;
    Direction direction;
    if (fabsf(event.dx) > fabsf(event.dy))
      direction = event.dx > 0 ? kRight : kLeft;
    else
      direction = event.dy > 0 ? kDown : kUp;
    if (i > 0 && direction != prev_direction)
      break;
    prev_direction = direction;
  }
  return i;
}

void ImmediateInterpreter::RegressScrollVelocity(int count, ScrollEvent* out)
    const {
  struct RegressionSums {
    float tt_;  // Cumulative sum of t^2.
    float t_;   // Cumulative sum of t.
    float tx_;  // Cumulative sum of t * x.
    float ty_;  // Cumulative sum of t * y.
    float x_;   // Cumulative sum of x.
    float y_;   // Cumulative sum of y.
  };

  out->dt = 1;
  if (count <= 1) {
    out->dx = 0;
    out->dy = 0;
    return;
  }

  RegressionSums sums = {0, 0, 0, 0, 0, 0};

  float time = 0;
  float x_coord = 0;
  float y_coord = 0;

  for (int i = count - 1; i >= 0; --i) {
    const ScrollEvent& event = scroll_buffer_.Get(i);

    time += event.dt;
    x_coord += event.dx;
    y_coord += event.dy;

    sums.tt_ += time * time;
    sums.t_ += time;
    sums.tx_ += time * x_coord;
    sums.ty_ += time * y_coord;
    sums.x_ += x_coord;
    sums.y_ += y_coord;
  }

  // Note the regression determinant only depends on the values of t, and should
  // never be zero so long as (1) count > 1, and (2) dt values are all non-zero.
  float det = count * sums.tt_ - sums.t_ * sums.t_;

  if (det) {
    float det_inv = 1.0 / det;

    out->dx = (count * sums.tx_ - sums.t_ * sums.x_) * det_inv;
    out->dy = (count * sums.ty_ - sums.t_ * sums.y_) * det_inv;
  } else {
    out->dx = 0;
    out->dy = 0;
  }
}

void ImmediateInterpreter::ComputeFling(ScrollEvent* out) const {
  ScrollEvent zero = { 0.0, 0.0, 0.0 };

  const size_t count = ScrollEventsForFlingCount();
  if (count > scroll_buffer_.Size()) {
    Err("Too few events in scroll buffer");
    *out = zero;
    return;
  }

  if (count < 2) {
    if (count == 0)
      *out = zero;
    else if (count == 1)
      *out = scroll_buffer_.Get(0);
    return;
  }

  // If we get here, count == 3 && scroll_buffer_.Size() >= 3
  RegressScrollVelocity(count, out);
}

void ImmediateInterpreter::FillResultGesture(
    const HardwareState& hwstate,
    const set<short, kMaxGesturingFingers>& fingers) {
  if (current_gesture_type_ == kGestureTypeMove ||
      current_gesture_type_ == kGestureTypeScroll)
    last_movement_timestamp_ = hwstate.timestamp;
  switch (current_gesture_type_) {
    case kGestureTypeMove: {
      if (fingers.empty())
        return;
      // Use highest finger (the one closes to the keyboard), excluding
      // palms, to compute motion. First, need to find out which finger that is.
      const FingerState* current = NULL;
      for (set<short, kMaxGesturingFingers>::const_iterator it =
               fingers.begin(), e = fingers.end(); it != e; ++it) {
        const FingerState* fs = hwstate.GetFingerState(*it);
        if (!current || fs->position_y < current->position_y)
          current = fs;
      }
      // Find corresponding finger id in previous state
      const FingerState* prev =
          prev_state_.GetFingerState(current->tracking_id);
      if (!prev)
        return;
      if (fabsf(current->pressure - prev->pressure) >
          max_pressure_change_.val_)
        break;
      float dx = current->position_x - prev->position_x;
      if (current->flags & GESTURES_FINGER_WARP_X)
        dx = 0.0;
      float dy = current->position_y - prev->position_y;
      if (current->flags & GESTURES_FINGER_WARP_Y)
        dy = 0.0;
      result_ = Gesture(kGestureMove,
                        prev_state_.timestamp,
                        hwstate.timestamp,
                        dx,
                        dy);
      break;
    }
    case kGestureTypeScroll: {
      // For now, we take the movement of the biggest moving finger.
      float max_mag_sq = 0.0;  // square of max mag
      float dx = 0.0;
      float dy = 0.0;
      for (set<short, kMaxGesturingFingers>::const_iterator it =
               fingers.begin(), e = fingers.end(); it != e; ++it) {
        const FingerState* fs = hwstate.GetFingerState(*it);
        const FingerState* prev = prev_state_.GetFingerState(*it);
        if (!prev)
          return;
        if (fabsf(fs->pressure - prev->pressure) > max_pressure_change_.val_)
          return;
        float local_dx = fs->position_x - prev->position_x;
        if (fs->flags & GESTURES_FINGER_WARP_X)
          local_dx = 0.0;
        float local_dy = fs->position_y - prev->position_y;
        if (fs->flags & GESTURES_FINGER_WARP_Y)
          local_dy = 0.0;
        float local_max_mag_sq = local_dx * local_dx + local_dy * local_dy;
        if (local_max_mag_sq > max_mag_sq) {
          max_mag_sq = local_max_mag_sq;
          dx = local_dx;
          dy = local_dy;
        }
      }

      // See if we should snap to vertical/horizontal
      if (fabsf(dy) < horizontal_scroll_snap_slope_.val_ * fabsf(dx))
        dy = 0.0;  // snap to horizontal
      else if (fabsf(dy) > vertical_scroll_snap_slope_.val_ * fabsf(dx))
        dx = 0.0;  // snap to vertical

      if (prev_scroll_fingers_ != fingers)
        scroll_buffer_.Clear();
      prev_scroll_fingers_ = fingers;
      scroll_buffer_.Insert(dx, dy, hwstate.timestamp - prev_state_.timestamp);
      if (max_mag_sq > 0) {
        result_ = Gesture(kGestureScroll,
                          prev_state_.timestamp,
                          hwstate.timestamp,
                          dx,
                          dy);
      }

      break;
    }
    case kGestureTypeFling: {
      ScrollEvent out;
      ComputeFling(&out);

      float vx = out.dt ? (out.dx / out.dt) : 0.0;
      float vy = out.dt ? (out.dy / out.dt) : 0.0;

      if (vx || vy) {
        result_ = Gesture(kGestureFling,
                          prev_state_.timestamp,
                          hwstate.timestamp,
                          vx,
                          vy,
                          GESTURES_FLING_START);
      }
      break;
    }
    case kGestureTypeSwipe: {
      float start_sum_x = 0, end_sum_x = 0;
      for (set<short, kMaxGesturingFingers>::const_iterator it =
               fingers.begin(), e = fingers.end(); it != e; ++it) {
        start_sum_x += start_positions_[*it].x_;
        end_sum_x += hwstate.GetFingerState(*it)->position_x;
      }
      double dx = (end_sum_x - start_sum_x) / fingers.size();
      result_ = Gesture(kGestureSwipe, changed_time_, hwstate.timestamp, dx);
      break;
    }

    case kGestureTypeZoom: {
      float current_dist = sqrtf(TwoFingerDistanceSq(hwstate));
      result_ = Gesture(kGestureZoom, changed_time_, hwstate.timestamp,
                        current_dist / two_finger_start_distance_);
      break;
    }
    default:
      result_.type = kGestureTypeNull;
  }
  if (current_gesture_type_ != kGestureTypeScroll) {
    scroll_buffer_.Clear();
    prev_scroll_fingers_.clear();
  }
}

void ImmediateInterpreter::IntWasWritten(IntProperty* prop) {
  if (prop == &keyboard_touched_timeval_low_) {
    struct timeval tv = {
      keyboard_touched_timeval_high_.val_,
      keyboard_touched_timeval_low_.val_
    };
    keyboard_touched_ = StimeFromTimeval(&tv);
  }
}

void ImmediateInterpreter::SetHardwareProperties(
    const HardwareProperties& hw_props) {
  hw_props_ = hw_props;
  if (prev_state_.fingers) {
    free(prev_state_.fingers);
    prev_state_.fingers = NULL;
  }
  prev_state_.fingers =
      reinterpret_cast<FingerState*>(calloc(hw_props_.max_finger_cnt,
                                            sizeof(FingerState)));
}

}  // namespace gestures
