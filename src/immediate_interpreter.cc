// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/immediate_interpreter.h"

#include <algorithm>
#include <functional>
#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"

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
  touched_[the_id] = fs;
}

void TapRecord::NoteRelease(short the_id) {
  if (touched_.find(the_id) == touched_.end())
    Err("Release of non-touched finger!");
  else
    released_.insert(the_id);
}

void TapRecord::Remove(short the_id) {
  touched_.erase(the_id);
  released_.erase(the_id);
}

void TapRecord::Update(const HardwareState& hwstate,
                       const set<short, kMaxTapFingers>& added,
                       const set<short, kMaxTapFingers>& removed,
                       const set<short, kMaxFingers>& dead) {
  Log("Updating TapRecord.");
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
  Log("Done Updating TapRecord.");
}

void TapRecord::Clear() {
  touched_.clear();
  released_.clear();
}

bool TapRecord::Moving(const HardwareState& hwstate,
                       const float dist_max) const {
  for (map<short, FingerState, kMaxTapFingers>::const_iterator it =
           touched_.begin(), e = touched_.end(); it != e; ++it) {
    const FingerState* fs = hwstate.GetFingerState((*it).first);
    if (!fs)
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

bool TapRecord::TapComplete() const {
  Log("called TapComplete()");
  for (map<short, FingerState, kMaxTapFingers>::const_iterator
           it = touched_.begin(), e = touched_.end(); it != e; ++it)
    Log("touched_: %d", (*it).first);
  for (set<short, kMaxTapFingers>::const_iterator it = released_.begin(),
           e = released_.end(); it != e; ++it)
    Log("released_: %d", *it);
  bool ret = !touched_.empty() && (touched_.size() == released_.size());
  Log("TapComplete() returning %d", ret);
  return ret;
}

int TapRecord::TapType() const {
  // TODO(adlr): use better logic here
  return touched_.size() > 1 ? GESTURES_BUTTON_RIGHT : GESTURES_BUTTON_LEFT;
}

ImmediateInterpreter::ImmediateInterpreter(PropRegistry* prop_reg)
    : button_type_(0),
      sent_button_down_(false),
      button_down_timeout_(0.0),
      tap_to_click_state_(kTtcIdle),
      tap_enable_(prop_reg, "Tap Enable", true),
      tap_timeout_(prop_reg, "Tap Timeout", 0.2),
      tap_drag_timeout_(prop_reg, "Tap Drag Timeout", 0.7),
      tap_move_dist_(prop_reg, "Tap Move Distance", 2.0),
      palm_pressure_(prop_reg, "Palm Pressure", 200.0),
      palm_edge_min_width_(prop_reg, "Tap Exclusion Border Width", 6.0),
      palm_edge_width_(prop_reg, "Palm Edge Zone Width", 12.5),
      palm_edge_point_speed_(prop_reg, "Palm Edge Zone Min Point Speed", 100.0),
      palm_min_distance_(prop_reg, "Palm Min Distance", 50.0),
      wiggle_max_dist_(prop_reg, "Wiggle Max Distance", 8.0),
      wiggle_suppress_timeout_(prop_reg, "Wiggle Timeout", 0.075),
      wiggle_button_down_timeout_(prop_reg, "Wiggle Button Down Timeout", 0.75),
      change_timeout_(prop_reg, "Change Timeout", 0.04),
      evaluation_timeout_(prop_reg, "Evaluation Timeout", 0.2),
      two_finger_pressure_diff_thresh_(prop_reg,
                                       "Two Finger Pressure Diff Thresh",
                                       32.0),
      two_finger_close_distance_thresh_(prop_reg,
                                        "Two Finger Close Distance Thresh",
                                        40.0),
      two_finger_scroll_distance_thresh_(prop_reg,
                                         "Two Finger Scroll Distance Thresh",
                                         2.0),
      max_pressure_change_(prop_reg, "Max Allowed Pressure Change", 8.0),
      scroll_stationary_finger_max_distance_(
          prop_reg, "Scroll Stationary Finger Max Distance", 1.0),
      bottom_zone_size_(prop_reg, "Bottom Zone Size", 10.0),
      button_evaluation_timeout_(prop_reg, "Button Evaluation Timeout", 0.03) {
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
  UpdatePalmState(*hwstate);
  UpdateClickWiggle(*hwstate);
  UpdateThumbState(*hwstate);
  set<short, kMaxGesturingFingers> gs_fingers = GetGesturingFingers(*hwstate);

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
  start_positions_.clear();
  changed_time_ = now;
}

bool ImmediateInterpreter::FingerNearOtherFinger(const HardwareState& hwstate,
                                                 size_t finger_idx) {
  const FingerState& fs = hwstate.fingers[finger_idx];
  for (int i = 0; i < hwstate.finger_cnt; ++i) {
    const FingerState& other_fs = hwstate.fingers[i];
    if (other_fs.tracking_id == fs.tracking_id)
      continue;
    float dx = fs.position_x - other_fs.position_x;
    float dy = fs.position_y - other_fs.position_y;
    bool too_close_to_other_finger =
        (dx * dx + dy * dy) < (palm_min_distance_.val_ *
                               palm_min_distance_.val_) &&
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
  }
}

void ImmediateInterpreter::UpdateClickWiggle(const HardwareState& hwstate) {
  // Removed outdated fingers from wiggle_recs_
  short dead_ids[wiggle_recs_.size()];
  size_t dead_ids_cnt = 0;
  for (map<short, ClickWiggleRec, kMaxFingers>::iterator it =
           wiggle_recs_.begin(), e = wiggle_recs_.end();
       it != e; ++it) {
    if (hwstate.GetFingerState((*it).first))
      continue;
    dead_ids[dead_ids_cnt++] = (*it).first;
  }
  for (size_t i = 0; i < dead_ids_cnt; i++)
    wiggle_recs_.erase(dead_ids[i]);

  const bool button_down = hwstate.buttons_down & GESTURES_BUTTON_LEFT;
  const bool prev_button_down = prev_state_.buttons_down & GESTURES_BUTTON_LEFT;
  const bool button_down_edge = button_down && !prev_button_down;
  const bool button_up_edge = !button_down && prev_button_down;

  // Update wiggle_recs_ for each current finger
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    map<short, ClickWiggleRec, kMaxFingers>::iterator it =
        wiggle_recs_.find(fs.tracking_id);
    const bool new_finger = it == wiggle_recs_.end();

    if (button_down_edge || button_up_edge || new_finger) {
      stime_t timeout = button_down_edge ?
          wiggle_button_down_timeout_.val_ : wiggle_suppress_timeout_.val_;
      ClickWiggleRec rec = {
        fs.position_x,  // button down x
        fs.position_y,  // button down y
        hwstate.timestamp + timeout,  // unused during click down
        !button_up_edge,  // block inc press
        true  // block dec press
      };
      wiggle_recs_[fs.tracking_id] = rec;
      continue;
    }

    // We have an existing finger
    ClickWiggleRec* rec = &(*it).second;

    if (!rec->suppress_inc_press_ && !rec->suppress_dec_press_)
      continue;  // It's already broken out of wiggle suppression

    float dx = fs.position_x - rec->x_;
    float dy = fs.position_y - rec->y_;
    if (dx * dx + dy * dy > wiggle_max_dist_.val_ * wiggle_max_dist_.val_) {
      // It's moved too much to be considered wiggle
      rec->suppress_inc_press_ = rec->suppress_dec_press_ = false;
      continue;
    }

    if (hwstate.timestamp >= rec->began_press_suppression_) {
      // Too much time has passed to consider this wiggle
      rec->suppress_inc_press_ = rec->suppress_dec_press_ = false;
      continue;
    }

    if (!rec->suppress_inc_press_ && !rec->suppress_dec_press_)
      continue;  // This happens when a finger is around on a down-edge

    FingerState* prev_fs = prev_state_.GetFingerState(fs.tracking_id);
    if (!prev_fs) {
      Err("Missing prev_fs?");
      continue;
    }

    if (fs.pressure >= prev_fs->pressure && rec->suppress_inc_press_)
      continue;
    rec->suppress_inc_press_ = false;
    if (fs.pressure <= prev_fs->pressure && rec->suppress_dec_press_)
      continue;
    rec->suppress_dec_press_ = false;
  }
}
//map<short, ClickWiggleRec, kMaxFingers> wiggle_recs_;

bool ImmediateInterpreter::WiggleSuppressed(short tracking_id) const {
  map<short, ClickWiggleRec, kMaxFingers>::const_iterator it =
      wiggle_recs_.find(tracking_id);
  if (it == wiggle_recs_.end()) {
    Err("Missing wiggle rec for id %d", tracking_id);
    return false;
  }
  return (*it).second.suppress_inc_press_  || (*it).second.suppress_dec_press_;
}

// Updates thumb_ below.
void ImmediateInterpreter::UpdateThumbState(const HardwareState& hwstate) {
  // Remove old ids from thumb_
  RemoveMissingIdsFromSet(&thumb_, hwstate);
  float min_pressure = INFINITY;
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    if (SetContainsValue(palm_, fs.tracking_id))
      continue;
    if (fs.pressure < min_pressure)
      min_pressure = fs.pressure;
  }
  // Make all large-pressure contacts thumbs
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    if (SetContainsValue(palm_, fs.tracking_id))
      continue;
    if (fs.pressure > (min_pressure + two_finger_pressure_diff_thresh_.val_))
      thumb_.insert(fs.tracking_id);
  }
  for (set<short, kMaxFingers>::const_iterator it = thumb_.begin();
       it != thumb_.end(); ++it) {
    pointing_.erase(*it);
  }
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
  const size_t kMaxSize = 2;  // We support up to 2 finger gestures
  if (pointing_.size() <= kMaxSize)
    return pointing_;

  const FingerState* fs[hwstate.finger_cnt];
  for (size_t i = 0; i < hwstate.finger_cnt; ++i)
    fs[i] = &hwstate.fingers[i];

  GetGesturingFingersCompare compare;
  // Pull the kMaxSize FingerStates w/ the lowest position_y to the
  // front of fs[].
  std::partial_sort(fs, fs + kMaxSize, fs + hwstate.finger_cnt, compare);
  set<short, kMaxGesturingFingers> ret;
  ret.insert(fs[0]->tracking_id);
  ret.insert(fs[1]->tracking_id);
  return ret;
}

void ImmediateInterpreter::UpdateCurrentGestureType(
    const HardwareState& hwstate,
    const set<short, kMaxGesturingFingers>& gs_fingers) {

  if (hwstate.timestamp < changed_time_ + change_timeout_.val_) {
    current_gesture_type_ = kGestureTypeNull;
    return;
  }
  if (sent_button_down_ || tap_to_click_state_ == kTtcDrag) {
    current_gesture_type_ = kGestureTypeMove;
    return;
  }
  if (hw_props_.supports_t5r2 && hwstate.touch_cnt > 2) {
    current_gesture_type_ = kGestureTypeScroll;
    return;
  }
  int num_gesturing = gs_fingers.size();
  if (num_gesturing == 0) {
    current_gesture_type_ = kGestureTypeNull;
  } else if (num_gesturing == 1) {
    current_gesture_type_ = kGestureTypeMove;
  } else if (num_gesturing == 2) {
    if (hwstate.timestamp - changed_time_ < evaluation_timeout_.val_ ||
        current_gesture_type_ == kGestureTypeNull) {
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
          TwoFingersGesturing(*fingers[0],
                              *fingers[1]);
      if (!potential_two_finger_gesture) {
        current_gesture_type_ = kGestureTypeMove;
      } else {
        current_gesture_type_ = GetTwoFingerGestureType(*fingers[0],
                                                        *fingers[1]);
      }
    }
  } else {
    Log("TODO(adlr): support > 2 finger gestures.");
  }
}

bool ImmediateInterpreter::TwoFingersGesturing(
    const FingerState& finger1,
    const FingerState& finger2) const {
  // First, make sure the pressure difference isn't too great
  float pdiff = fabsf(finger1.pressure - finger2.pressure);
  if (pdiff > two_finger_pressure_diff_thresh_.val_)
    return false;
  float xdist = fabsf(finger1.position_x - finger2.position_x);
  float ydist = fabsf(finger1.position_y - finger2.position_y);

  // Next, make sure distance between fingers isn't too great
  if ((xdist * xdist + ydist * ydist) >
      (two_finger_close_distance_thresh_.val_ *
       two_finger_close_distance_thresh_.val_))
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
  // Compute distance traveled since fingers changed for each finger
  float dx1 = finger1.position_x -
      start_positions_[finger1.tracking_id].first;
  float dy1 = finger1.position_y -
      start_positions_[finger1.tracking_id].second;
  float dx2 = finger2.position_x -
      start_positions_[finger2.tracking_id].first;
  float dy2 = finger2.position_y -
      start_positions_[finger2.tracking_id].second;

  float large_dx = MaxMag(dx1, dx2);
  float large_dy = MaxMag(dy1, dy2);
  float small_dx = MinMag(dx1, dx2);
  float small_dy = MinMag(dy1, dy2);

  if (fabsf(large_dx) > fabsf(large_dy)) {
    // consider horizontal scroll
    if (fabsf(large_dx) < two_finger_scroll_distance_thresh_.val_)
      return kGestureTypeNull;
    if (fabsf(small_dx) < scroll_stationary_finger_max_distance_.val_)
      small_dx = 0.0;
    return ((large_dx * small_dx) >= 0.0) ?  // same direction
        kGestureTypeScroll : kGestureTypeNull;
  } else {
    // consider vertical scroll
    if (fabsf(large_dy) < two_finger_scroll_distance_thresh_.val_)
      return kGestureTypeNull;
    if (fabsf(small_dy) < scroll_stationary_finger_max_distance_.val_)
      small_dy = 0.0;
    return ((large_dy * small_dy) >= 0.0) ?  // same direction
        kGestureTypeScroll : kGestureTypeNull;
  }
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
    case kTtcTapComplete: return tap_timeout_.val_;
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
  if (hwstate)
    for (int i = 0; i < hwstate->finger_cnt; ++i)
      Log("HWSTATE: %d", hwstate->fingers[i].tracking_id);
  for (set<short, kMaxGesturingFingers>::const_iterator it =
           gs_fingers.begin(), e = gs_fingers.end(); it != e; ++it)
    Log("GS: %d", *it);
  set<short, kMaxTapFingers> added_fingers;

  // Fingers removed from the pad entirely
  set<short, kMaxTapFingers> removed_fingers;

  // Fingers that were gesturing, but now aren't
  set<short, kMaxFingers> dead_fingers;

  const bool phys_button_down = hwstate && hwstate->buttons_down != 0;

  bool is_timeout = (now - tap_to_click_state_entered_ >
                     TimeoutForTtcState(tap_to_click_state_));

  if (hwstate && (!same_fingers || prev_gs_fingers_ != gs_fingers)) {
    // See if fingers were added
    for (set<short, kMaxGesturingFingers>::const_iterator it =
             gs_fingers.begin(), e = gs_fingers.end(); it != e; ++it)
      if (!prev_state_.GetFingerState(*it)) {
        // Gesturing finger wasn't in prev state. It's new.
        added_fingers.insert(*it);
        Log("TTC: Added %d", *it);
      }

    // See if fingers were removed or are now non-gesturing (dead)
    for (set<short, kMaxGesturingFingers>::const_iterator it =
             prev_gs_fingers_.begin(), e = prev_gs_fingers_.end();
         it != e; ++it) {
      if (gs_fingers.find(*it) != gs_fingers.end())
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

  // The state machine:

  // If you are updating the code, keep this diagram correct.
  // We have a TapRecord which stores current tap state.
  // Also, if the physical button is down, we go to (or stay in) Idle state.

  //     Start
  //       ↓
  //    [Idle**] <----------------------------------------------------------,
  //       ↓ added finger(s)                                                |
  //    [FirstTapBegan] -<right click: send right click, timeout/movement>->|
  //       ↓ released all fingers                                           |
  // ,->[TapComplete*] --<timeout: send click>----------------------------->|
  // |     ↓ add finger(s): send button down                                |
  // |  [SubsequentTapBegan] --<timeout/move(non-drag): send btn up>------->|
  // |     | | released all fingers: send button up                         |
  // |<----+-'                                                              |
  // |     ↓ timeout/movement (that looks like drag)                        |
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
  if (phys_button_down) {
    Log("Physical button down. Going to Idle state");
    SetTapToClickState(kTtcIdle, now);
    return;
  }

  switch (tap_to_click_state_) {
    case kTtcIdle:
      if (!added_fingers.empty()) {
        tap_record_.Update(
            *hwstate, added_fingers, removed_fingers, dead_fingers);
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
          *hwstate, added_fingers, removed_fingers, dead_fingers);
      Log("Is tap? %d Is moving? %d",
          tap_record_.TapComplete(),
          tap_record_.Moving(*hwstate, tap_move_dist_.val_));
      if (tap_record_.TapComplete()) {
        if (tap_record_.TapType() == GESTURES_BUTTON_LEFT) {
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
        // Generate a button event for the tap type that got us into
        // kTtcTapComplete state, after which we'll repurpose
        // tap_record_ to record the next tap.
        *buttons_down = tap_record_.TapType();
        tap_record_.Clear();
        tap_record_.Update(
            *hwstate, added_fingers, removed_fingers, dead_fingers);
        SetTapToClickState(kTtcSubsequentTapBegan, now);
      } else if (is_timeout) {
        *buttons_down = *buttons_up = tap_record_.TapType();
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
            *hwstate, added_fingers, removed_fingers, dead_fingers);
      if (is_timeout || tap_record_.Moving(*hwstate, tap_move_dist_.val_)) {
        if (tap_record_.TapType() == GESTURES_BUTTON_LEFT) {
          SetTapToClickState(kTtcDrag, now);
        } else {
          *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(kTtcIdle, now);
        }
        break;
      }
      if (tap_record_.TapComplete()) {
        *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(kTtcTapComplete, now);
        Log("Subsequent left tap complete");
      }
      break;
    case kTtcDrag:
      if (hwstate)
        tap_record_.Update(
            *hwstate, added_fingers, removed_fingers, dead_fingers);
      if (tap_record_.TapComplete()) {
        tap_record_.Clear();
        SetTapToClickState(kTtcDragRelease, now);
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
            *hwstate, added_fingers, removed_fingers, dead_fingers);
        SetTapToClickState(kTtcDragRetouch, now);
      } else if (is_timeout) {
        *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(kTtcIdle, now);
      }
      break;
    case kTtcDragRetouch:
      if (hwstate)
        tap_record_.Update(
            *hwstate, added_fingers, removed_fingers, dead_fingers);
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
    case kTtcIdle:
      tap_record_.Clear();
      break;
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
        make_pair(hwstate.fingers[i].position_x, hwstate.fingers[i].position_y);
}

int ImmediateInterpreter::EvaluateButtonType(
    const HardwareState& hwstate) {
  if (hw_props_.supports_t5r2 && hwstate.finger_cnt > 2)
    return GESTURES_BUTTON_RIGHT;
  int num_pointing = pointing_.size();
  if (num_pointing <= 1)
    return GESTURES_BUTTON_LEFT;
  if (current_gesture_type_ == kGestureTypeScroll)
    return GESTURES_BUTTON_RIGHT;
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
        button_down_timeout_ >= hwstate.timestamp ||
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

void ImmediateInterpreter::FillResultGesture(
    const HardwareState& hwstate,
    const set<short, kMaxGesturingFingers>& fingers) {
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
      if (WiggleSuppressed(current->tracking_id))
        break;
      // Find corresponding finger id in previous state
      const FingerState* prev =
          prev_state_.GetFingerState(current->tracking_id);
      if (!prev) {
        Err("No previous state!");
        return;
      }
      if (fabsf(current->pressure - prev->pressure) >
          max_pressure_change_.val_)
        break;
      result_ = Gesture(kGestureMove,
                        prev_state_.timestamp,
                        hwstate.timestamp,
                        current->position_x -
                        prev->position_x,
                        current->position_y -
                        prev->position_y);
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
        if (fabsf(fs->pressure - prev->pressure) > max_pressure_change_.val_)
          return;
        float local_dx = fs->position_x - prev->position_x;
        float local_dy = fs->position_y - prev->position_y;
        float local_max_mag_sq = local_dx * local_dx + local_dy * local_dy;
        if (local_max_mag_sq > max_mag_sq) {
          max_mag_sq = local_max_mag_sq;
          dx = local_dx;
          dy = local_dy;
        }
      }

      // For now, only do horizontal or vertical scroll
      if (fabsf(dx) > fabsf(dy))
        dy = 0.0;
      else
        dx = 0.0;

      if (max_mag_sq > 0) {
        result_ = Gesture(kGestureScroll,
                          prev_state_.timestamp,
                          hwstate.timestamp,
                          dx,
                          dy);
      }

      break;
    }
    default:
      result_.type = kGestureTypeNull;
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
