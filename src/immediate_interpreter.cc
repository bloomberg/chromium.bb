// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/immediate_interpreter.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>

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

// Describes the three classes of fingers we deal with while determining
// the type of physical button clicks.
enum FingerButtonClickStatus {
  // A 'recent' finger has recently touched down on the touchpad.
  GESTURES_FINGER_STATUS_RECENT,
  // A 'cold' finger has already been on the touchpad for a while,
  // but has not been moved.
  GESTURES_FINGER_STATUS_COLD,
  // A 'hot' finger has been moved since it touched down.
  GESTURES_FINGER_STATUS_HOT
};

// A comparator class for use with STL algorithms that sorts FingerStates
// by their origin timestamp.
class FingerOriginCompare {
 public:
  explicit FingerOriginCompare(ImmediateInterpreter* interpreter)
      : interpreter_(interpreter) {
  }
  bool operator()(const FingerState* a, const FingerState* b) const {
    return interpreter_->finger_origin_timestamp(a->tracking_id) <
           interpreter_->finger_origin_timestamp(b->tracking_id);
  }

 private:
  ImmediateInterpreter* interpreter_;
};

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
      if (immediate_interpreter_->finger_metrics_->FingersCloseEnoughToGesture(
              existing_fs,
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
    Log("TapRecord::Update: Added: %d", *it);
  for (set<short, kMaxTapFingers>::const_iterator it = removed.begin(),
           e = removed.end(); it != e; ++it)
    Log("TapRecord::Update: Removed: %d", *it);
  for (set<short, kMaxFingers>::const_iterator it = dead.begin(),
           e = dead.end(); it != e; ++it)
    Log("TapRecord::Update: Dead: %d", *it);
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
      stime_t finger_age = hwstate.timestamp -
          immediate_interpreter_->finger_origin_timestamp(fs->tracking_id);
      if (finger_age > immediate_interpreter_->tap_max_finger_age())
        fingers_below_max_age_ = false;
    }
  }
}

void TapRecord::Clear() {
  min_tap_pressure_met_.clear();
  min_cotap_pressure_met_.clear();
  t5r2_ = false;
  t5r2_touched_size_ = 0;
  t5r2_released_size_ = 0;
  fingers_below_max_age_ = true;
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
    // Respect WARP flags
    if (fs->flags & GESTURES_FINGER_WARP_X_TAP_MOVE)
      dist_x = 0.0;
    if (fs->flags & GESTURES_FINGER_WARP_X_TAP_MOVE)
      dist_y = 0.0;

    bool moving =
        dist_x * dist_x + dist_y * dist_y > dist_max * dist_max;
    if (moving)
      return true;
  }
  return false;
}

bool TapRecord::Motionless(const HardwareState& hwstate, const HardwareState&
                           prev_hwstate, const float max_speed) const {
  const float cotap_min_pressure = CotapMinPressure();
  for (map<short, FingerState, kMaxTapFingers>::const_iterator it =
           touched_.begin(), e = touched_.end(); it != e; ++it) {
    const FingerState* fs = hwstate.GetFingerState((*it).first);
    const FingerState* prev_fs = prev_hwstate.GetFingerState((*it).first);
    if (!fs || !prev_fs)
      continue;
    // Only look for moving when current frame meets cotap pressure and
    // our history contains a contact that's met cotap pressure.
    if (fs->pressure < cotap_min_pressure ||
        prev_fs->pressure < cotap_min_pressure)
      continue;
    // Compute distance moved
    if (DistSq(*fs, *prev_fs) > max_speed * max_speed)
      return false;
  }
  return true;
}

bool TapRecord::TapBegan() const {
  if (t5r2_)
    return t5r2_touched_size_ > 0;
  return !touched_.empty();
}

bool TapRecord::TapComplete() const {
  bool ret = false;
  if (t5r2_)
    ret = t5r2_touched_size_ && t5r2_touched_size_ == t5r2_released_size_;
  else
    ret = !touched_.empty() && (touched_.size() == released_.size());
  for (map<short, FingerState, kMaxTapFingers>::const_iterator
           it = touched_.begin(), e = touched_.end(); it != e; ++it)
    Log("TapRecord::TapComplete: touched_: %d", (*it).first);
  for (set<short, kMaxTapFingers>::const_iterator it = released_.begin(),
           e = released_.end(); it != e; ++it)
    Log("TapRecord::TapComplete: released_: %d", *it);
  return ret;
}

bool TapRecord::MinTapPressureMet() const {
  // True if any touching finger met minimum pressure
  return t5r2_ || !min_tap_pressure_met_.empty();
}

bool TapRecord::FingersBelowMaxAge() const {
  return fingers_below_max_age_;
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

void ScrollEventBuffer::GetSpeedSq(size_t num_events, float* dist_sq,
                                   float* dt) const {
  float dx = 0.0;
  float dy = 0.0;
  *dt = 0.0;
  for (size_t i = 0; i < Size() && i < num_events; i++) {
    const ScrollEvent& evt = Get(i);
    dx += evt.dx;
    dy += evt.dy;
    *dt += evt.dt;
  }
  *dist_sq = dx * dx + dy * dy;
}

HardwareStateBuffer::HardwareStateBuffer(size_t size)
    : states_(new HardwareState[size]),
      newest_index_(0), size_(size), max_finger_cnt_(0) {
  for (size_t i = 0; i < size_; i++) {
    memset(&states_[i], 0, sizeof(HardwareState));
  }
}

HardwareStateBuffer::~HardwareStateBuffer() {
  for (size_t i = 0; i < size_; i++) {
    delete[] states_[i].fingers;
  }
}

void HardwareStateBuffer::Reset(size_t max_finger_cnt) {
  max_finger_cnt_ = max_finger_cnt;
  for (size_t i = 0; i < size_; i++) {
    delete[] states_[i].fingers;
  }
  if (max_finger_cnt_) {
    for (size_t i = 0; i < size_; i++) {
      states_[i].fingers = new FingerState[max_finger_cnt_];
      memset(states_[i].fingers, 0, sizeof(FingerState) * max_finger_cnt_);
    }
  } else {
    for (size_t i = 0; i < size_; i++) {
      states_[i].fingers = NULL;
    }
  }
}

void HardwareStateBuffer::PushState(const HardwareState& state) {
  newest_index_ = (newest_index_ + size_ - 1) % size_;
  Get(0)->DeepCopy(state, max_finger_cnt_);
}

ScrollManager::ScrollManager(PropRegistry* prop_reg)
    : prev_result_high_pressure_change_(false),
      max_pressure_change_(prop_reg, "Max Allowed Pressure Change Per Sec",
                           800.0),
      max_pressure_change_hysteresis_(prop_reg,
                                      "Max Hysteresis Pressure Per Sec",
                                      600.0),
      max_pressure_change_duration_(prop_reg,
                                    "Max Pressure Change Duration",
                                    0.016),
      vertical_scroll_snap_slope_(prop_reg, "Vertical Scroll Snap Slope",
                                  tanf(DegToRad(50.0))),  // 50 deg. from horiz.
      horizontal_scroll_snap_slope_(prop_reg, "Horizontal Scroll Snap Slope",
                                    tanf(DegToRad(30.0))),

      fling_buffer_depth_(prop_reg, "Fling Buffer Depth", 10),
      fling_buffer_suppress_zero_length_scrolls_(
          prop_reg, "Fling Buffer Suppress Zero Length Scrolls", 0),
      fling_buffer_min_avg_speed_(prop_reg,
                                  "Fling Buffer Min Avg Speed",
                                  10.0) {
}

bool ScrollManager::PressureChangingSignificantly(
    const HardwareStateBuffer& state_buffer,
    const FingerState& current) const {
  bool pressure_is_increasing = false;
  bool pressure_direction_established = false;
  const FingerState* prev = &current;
  stime_t now = state_buffer.Get(0)->timestamp;
  stime_t duration = 0.0;

  if (max_pressure_change_duration_.val_ > 0.0) {
    for (size_t i = 1; i < state_buffer.Size(); i++) {
      const HardwareState& state = *state_buffer.Get(i);
      stime_t local_duration = now - state.timestamp;
      if (local_duration > max_pressure_change_duration_.val_)
        break;

      duration = local_duration;
      const FingerState* fs = state.GetFingerState(current.tracking_id);
      // If the finger just appeared, there's no history to look at.
      if (!fs)
        return false;

      float pressure_difference = prev->pressure - fs->pressure;
      if (pressure_difference) {
        bool is_currently_increasing = pressure_difference > 0.0;
        if (!pressure_direction_established) {
          pressure_is_increasing = is_currently_increasing;
          pressure_direction_established = true;
        }

        // If pressure changes are unstable, it's likely just noise.
        if (is_currently_increasing != pressure_is_increasing)
          return false;
      }
      prev = fs;
    }
  } else {
    // To disable this feature, max_pressure_change_duration_ can be set to a
    // negative number.  When this occurs it reverts to just checking the last
    // event, not looking through the backlog as well.
    prev = state_buffer.Get(1)->GetFingerState(current.tracking_id);
    duration = now - state_buffer.Get(1)->timestamp;
  }

  float dp_thresh = duration *
      (prev_result_high_pressure_change_ ?
       max_pressure_change_hysteresis_.val_ :
       max_pressure_change_.val_);
  float dp = fabsf(current.pressure - prev->pressure);
  return dp > dp_thresh;
}

bool ScrollManager::ComputeScroll(
    const HardwareStateBuffer& state_buffer,
    const FingerMap& prev_gs_fingers,
    const FingerMap& gs_fingers,
    GestureType prev_gesture_type,
    const Gesture& prev_result,
    Gesture* result,
    ScrollEventBuffer* scroll_buffer) {
  // For now, we take the movement of the biggest moving finger.
  float max_mag_sq = 0.0;  // square of max mag
  float dx = 0.0;
  float dy = 0.0;
  bool high_pressure_change = false;
  for (FingerMap::const_iterator it =
           gs_fingers.begin(), e = gs_fingers.end(); it != e; ++it) {
    const FingerState* fs = state_buffer.Get(0)->GetFingerState(*it);
    const FingerState* prev = state_buffer.Get(1)->GetFingerState(*it);
    if (!prev)
      return false;
    high_pressure_change = high_pressure_change ||
        PressureChangingSignificantly(state_buffer, *fs);
    float local_dx = fs->position_x - prev->position_x;
    if (fs->flags & GESTURES_FINGER_WARP_X_NON_MOVE)
      local_dx = 0.0;
    float local_dy = fs->position_y - prev->position_y;
    if (fs->flags & GESTURES_FINGER_WARP_Y_NON_MOVE)
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

  prev_result_high_pressure_change_ = high_pressure_change;
  if (high_pressure_change) {
    // If we get here, it means that the pressure of the finger causing
    // the scroll is changing a lot, so we don't trust it. It's likely
    // leaving the touchpad. Normally we might just do nothing, but having
    // a frame or two of 0 length scroll before a fling looks janky. We
    // could also just start the fling now, but we don't want to do that
    // because the fingers may not actually be leaving. What seems to work
    // well is sort of dead-reckoning approach where we just repeat the
    // scroll event from the previous input frame.
    // Since this isn't a "real" scroll event, we don't put it into
    // scroll_buffer_.
    // Also, only use previous gesture if it's in the same direction.
    if (prev_result.type == kGestureTypeScroll &&
        prev_result.details.scroll.dy * dy >= 0 &&
        prev_result.details.scroll.dx * dx >= 0)
      *result = prev_result;
    return false;
  }

  if (max_mag_sq > 0) {
    *result = Gesture(kGestureScroll,
                      state_buffer.Get(1)->timestamp,
                      state_buffer.Get(0)->timestamp,
                      dx, dy);
  }
  if (prev_gesture_type != kGestureTypeScroll || prev_gs_fingers != gs_fingers)
    scroll_buffer->Clear();
  if (!fling_buffer_suppress_zero_length_scrolls_.val_ ||
      !FloatEq(dx, 0.0) || !FloatEq(dy, 0.0))
    scroll_buffer->Insert(
        dx, dy,
        state_buffer.Get(0)->timestamp - state_buffer.Get(1)->timestamp);
  return true;
}

void ScrollManager::UpdateScrollEventBuffer(
    GestureType gesture_type, ScrollEventBuffer* scroll_buffer) const {
  if (gesture_type != kGestureTypeScroll)
    scroll_buffer->Clear();
}

size_t ScrollManager::ScrollEventsForFlingCount(
    const ScrollEventBuffer& scroll_buffer) const {
  if (scroll_buffer.Size() <= 1)
    return scroll_buffer.Size();
  enum Direction { kNone, kUp, kDown, kLeft, kRight };
  size_t i = 0;
  Direction prev_direction = kNone;
  size_t fling_buffer_depth = static_cast<size_t>(fling_buffer_depth_.val_);
  for (; i < scroll_buffer.Size() && i < fling_buffer_depth; i++) {
    const ScrollEvent& event = scroll_buffer.Get(i);
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

void ScrollManager::RegressScrollVelocity(
    const ScrollEventBuffer& scroll_buffer, int count, ScrollEvent* out) const {
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
    const ScrollEvent& event = scroll_buffer.Get(i);

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

void ScrollManager::ComputeFling(const HardwareStateBuffer& state_buffer,
                                 const ScrollEventBuffer& scroll_buffer,
                                 Gesture* result) const {
  ScrollEvent out = { 0.0, 0.0, 0.0 };
  ScrollEvent zero = { 0.0, 0.0, 0.0 };
  size_t count = 0;

  // Make sure fling buffer met the minimum average speed for a fling.
  float buf_dist_sq = 0.0;
  float buf_dt = 0.0;
  scroll_buffer.GetSpeedSq(fling_buffer_depth_.val_, &buf_dist_sq, &buf_dt);
  if (fling_buffer_min_avg_speed_.val_ * fling_buffer_min_avg_speed_.val_ *
      buf_dt * buf_dt > buf_dist_sq) {
    out = zero;
    goto done;
  }

  count = ScrollEventsForFlingCount(scroll_buffer);
  if (count > scroll_buffer.Size()) {
    Err("Too few events in scroll buffer");
    out = zero;
    goto done;
  }

  if (count < 2) {
    if (count == 0)
      out = zero;
    else if (count == 1)
      out = scroll_buffer.Get(0);
    goto done;
  }

  // If we get here, count == 3 && scroll_buffer.Size() >= 3
  RegressScrollVelocity(scroll_buffer, count, &out);

done:
  float vx = out.dt ? (out.dx / out.dt) : 0.0;
  float vy = out.dt ? (out.dy / out.dt) : 0.0;
  *result = Gesture(kGestureFling,
                    state_buffer.Get(1)->timestamp,
                    state_buffer.Get(0)->timestamp,
                    vx,
                    vy,
                    GESTURES_FLING_START);
}

ImmediateInterpreter::ImmediateInterpreter(PropRegistry* prop_reg,
                                           FingerMetrics* finger_metrics,
                                           Tracer* tracer)
    : Interpreter(NULL, tracer, false),
      button_type_(0),
      sent_button_down_(false),
      button_down_timeout_(0.0),
      started_moving_time_(-1.0),
      gs_changed_time_(-1.0),
      finger_leave_time_(0.0),
      tap_to_click_state_(kTtcIdle),
      tap_to_click_state_entered_(0.0),
      tap_record_(this),
      last_movement_timestamp_(0.0),
      last_swipe_timestamp_(0.0),
      swipe_is_vertical_(false),
      current_gesture_type_(kGestureTypeNull),
      state_buffer_(4),
      scroll_buffer_(20),
      finger_metrics_(finger_metrics),
      pinch_guess_start_(-1.0),
      pinch_locked_(false),
      finger_seen_since_button_down_(false),
      scroll_manager_(prop_reg),
      tap_enable_(prop_reg, "Tap Enable", true),
      tap_paused_(prop_reg, "Tap Paused", false),
      tap_timeout_(prop_reg, "Tap Timeout", 0.2),
      inter_tap_timeout_(prop_reg, "Inter-Tap Timeout", 0.15),
      tap_drag_delay_(prop_reg, "Tap Drag Delay", 0.05),
      tap_drag_timeout_(prop_reg, "Tap Drag Timeout", 0.3),
      tap_drag_enable_(prop_reg, "Tap Drag Enable", 0),
      drag_lock_enable_(prop_reg, "Tap Drag Lock Enable", 0),
      tap_drag_stationary_time_(prop_reg, "Tap Drag Stationary Time", 0),
      tap_move_dist_(prop_reg, "Tap Move Distance", 2.0),
      tap_min_pressure_(prop_reg, "Tap Minimum Pressure", 25.0),
      tap_max_movement_(prop_reg, "Tap Maximum Movement", 0.0001),
      tap_max_finger_age_(prop_reg, "Tap Maximum Finger Age", 1.2),
      three_finger_click_enable_(prop_reg, "Three Finger Click Enable", 0),
      zero_finger_click_enable_(prop_reg, "Zero Finger Click Enable", 1),
      t5r2_three_finger_click_enable_(prop_reg,
                                      "T5R2 Three Finger Click Enable",
                                      0),
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
      two_finger_scroll_distance_thresh_(prop_reg,
                                         "Two Finger Scroll Distance Thresh",
                                         2.0),
      three_finger_close_distance_thresh_(prop_reg,
                                          "Three Finger Close Distance Thresh",
                                          50.0),
      three_finger_swipe_distance_thresh_(prop_reg,
                                          "Three Finger Swipe Distance Thresh",
                                          1.0),
      three_finger_swipe_enable_(prop_reg, "Three Finger Swipe EnableX", 1),
      scroll_stationary_finger_max_distance_(
          prop_reg, "Scroll Stationary Finger Max Distance", 1.0),
      bottom_zone_size_(prop_reg, "Bottom Zone Size", 10.0),
      button_evaluation_timeout_(prop_reg, "Button Evaluation Timeout", 0.05),
      button_finger_timeout_(prop_reg, "Button Finger Timeout", 0.03),
      button_move_dist_(prop_reg, "Button Move Distance", 10.0),
      button_max_dist_from_expected_(prop_reg,
                                     "Button Max Distance From Expected", 20.0),
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
      no_pinch_guess_ratio_(prop_reg, "No-Pinch Guess Ratio", 50.0),
      no_pinch_certain_ratio_(prop_reg, "No-Pinch Certain Ratio", 100.0),
      pinch_noise_level_(prop_reg, "Pinch Noise Level", 1.0),
      pinch_guess_min_movement_(prop_reg, "Pinch Guess Minimal Movement", 4.0),
      pinch_certain_min_movement_(prop_reg,
                                  "Pinch Certain Minimal Movement", 8.0),
      pinch_enable_(prop_reg, "Pinch Enable", 1.0),
      right_click_start_time_diff_(prop_reg,
                                   "Right Click Start Time Diff Thresh",
                                   0.5),
      right_click_second_finger_age_(prop_reg,
                                     "Right Click Second Finger Age Thresh",
                                     0.5) {
  InitName();
  if (!finger_metrics_) {
    test_finger_metrics_.reset(new FingerMetrics(prop_reg));
    finger_metrics_ = test_finger_metrics_.get();
  }
}

Gesture* ImmediateInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                                 stime_t* timeout) {
  if (!state_buffer_.Get(0)->fingers) {
    Err("Must call SetHardwareProperties() before Push().");
    return 0;
  }

  state_buffer_.PushState(*hwstate);

  FillOriginInfo(*hwstate);
  result_.type = kGestureTypeNull;
  const bool same_fingers = state_buffer_.Get(1)->SameFingersAs(*hwstate) &&
      (hwstate->buttons_down == state_buffer_.Get(1)->buttons_down);
  if (!same_fingers) {
    // Fingers changed, do nothing this time
    ResetSameFingersState(hwstate->timestamp);
    FillStartPositions(*hwstate);
    UpdatePinchState(*hwstate, true);
  }

  if (hwstate->finger_cnt < state_buffer_.Get(1)->finger_cnt)
    finger_leave_time_ = hwstate->timestamp;

  UpdatePointingFingers(*hwstate);
  UpdateThumbState(*hwstate);
  FingerMap gs_fingers = GetGesturingFingers(*hwstate);
  if (gs_fingers != prev_gs_fingers_)
    gs_changed_time_ = hwstate->timestamp;

  UpdateStartedMovingTime(*hwstate, gs_fingers);
  UpdateButtons(*hwstate, timeout);
  UpdateTapGesture(hwstate,
                   gs_fingers,
                   same_fingers,
                   hwstate->timestamp,
                   timeout);

  UpdateCurrentGestureType(*hwstate, gs_fingers);
  if (result_.type == kGestureTypeNull)
    FillResultGesture(*hwstate, gs_fingers);

  // Prevent moves while in a tap
  if ((tap_to_click_state_ == kTtcFirstTapBegan ||
       tap_to_click_state_ == kTtcSubsequentTapBegan) &&
      result_.type == kGestureTypeMove)
    result_.type = kGestureTypeNull;

  prev_gs_fingers_ = gs_fingers;
  prev_result_ = result_;
  prev_gesture_type_ = current_gesture_type_;
  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

Gesture* ImmediateInterpreter::HandleTimerImpl(stime_t now, stime_t* timeout) {
  result_.type = kGestureTypeNull;
  // Tap-to-click always aborts when real button(s) are being used, so we
  // don't need to worry about conflicts with these two types of callback.
  UpdateButtonsTimeout(now);
  UpdateTapGesture(NULL,
                   FingerMap(),
                   false,
                   now,
                   timeout);
  return result_.type != kGestureTypeNull ? &result_ : NULL;
}

void ImmediateInterpreter::FillOriginInfo(
    const HardwareState& hwstate) {
  RemoveMissingIdsFromMap(&origin_timestamps_, hwstate);
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    if (MapContainsKey(origin_timestamps_, fs.tracking_id))
      continue;
    origin_timestamps_[fs.tracking_id] = hwstate.timestamp;
  }
}

void ImmediateInterpreter::ResetSameFingersState(stime_t now) {
  pointing_.clear();
  fingers_.clear();
  start_positions_.clear();
  moving_.clear();
  changed_time_ = now;
}

void ImmediateInterpreter::UpdatePointingFingers(const HardwareState& hwstate) {
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    if (hwstate.fingers[i].flags & GESTURES_FINGER_PALM)
      pointing_.erase(hwstate.fingers[i].tracking_id);
    else
      pointing_.insert(hwstate.fingers[i].tracking_id);
  }
  fingers_ = pointing_;
}

float ImmediateInterpreter::DistanceTravelledSq(const FingerState& fs,
                                                bool origin) const {
  Point delta = FingerTraveledVector(fs, origin);
  return delta.x_ * delta.x_ + delta.y_ * delta.y_;
}

ImmediateInterpreter::Point ImmediateInterpreter::FingerTraveledVector(
    const FingerState& fs, bool origin) const {
  const map<short, Point, kMaxFingers>* positions;
  if (origin)
    positions = &origin_positions_;
  else
    positions = &start_positions_;

  if (!MapContainsKey(*positions, fs.tracking_id))
    return Point(0.0f, 0.0f);

  const Point& start = (*positions)[fs.tracking_id];
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
    if (fs.flags & GESTURES_FINGER_PALM)
      continue;
    if (fs.pressure < min_pressure) {
      min_pressure = fs.pressure;
      min_fs = &fs;
    }
  }
  if (!min_fs) {
    // Only palms on the touchpad
    return;
  }
  float thumb_dist_sq_thresh = DistanceTravelledSq(*min_fs, false) *
      thumb_movement_factor_.val_ * thumb_movement_factor_.val_;
  // Make all large-pressure contacts located below the min-pressure
  // contact as thumbs.
  for (size_t i = 0; i < hwstate.finger_cnt; i++) {
    const FingerState& fs = hwstate.fingers[i];
    if (fs.flags & GESTURES_FINGER_PALM)
      continue;
    if (fs.pressure > min_pressure + two_finger_pressure_diff_thresh_.val_ &&
        fs.pressure > min_pressure * two_finger_pressure_diff_factor_.val_ &&
        fs.position_y > min_fs->position_y &&
        DistanceTravelledSq(fs, false) <= thumb_dist_sq_thresh) {
      if (!MapContainsKey(thumb_, fs.tracking_id))
        thumb_[fs.tracking_id] = hwstate.timestamp;
    } else if ((MapContainsKey(thumb_, fs.tracking_id) &&
                hwstate.timestamp <
                max(started_moving_time_,
                    thumb_[fs.tracking_id]) + thumb_eval_timeout_.val_) ||
               (DistanceTravelledSq(fs, false) > thumb_dist_sq_thresh &&
                fs.tracking_id != min_fs->tracking_id)) {
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

FingerMap ImmediateInterpreter::GetGesturingFingers(
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
  FingerMap ret;
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
    const FingerMap& gs_fingers) {

  size_t num_gesturing = gs_fingers.size();

  // Physical button or tap overrides current gesture state
  if (sent_button_down_ || tap_to_click_state_ == kTtcDrag) {
    current_gesture_type_ = kGestureTypeMove;
    return;
  }

  // current gesture state machine
  switch (current_gesture_type_) {
    case kGestureTypeContactInitiated:
    case kGestureTypeButtonsChange:
      break;

    case kGestureTypeScroll:
    case kGestureTypeSwipe:
      // If a gesturing finger just left, do fling/lift
      if (AnyGesturingFingerLeft(*state_buffer_.Get(0),
                                 prev_gs_fingers_)) {
        current_gesture_type_ =
            current_gesture_type_ == kGestureTypeScroll ?
            kGestureTypeFling : kGestureTypeSwipeLift;
        return;
      }
      // fallthrough
    case kGestureTypeSwipeLift:
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
            hwstate.timestamp - max(started_moving_time_, gs_changed_time_) <
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
          (pinch_enable_.val_ && !hw_props_.support_semi_mt)) {
        bool do_pinch = UpdatePinchState(hwstate, false);
        if(do_pinch) {
          current_gesture_type_ = kGestureTypePinch;
        }
      }
      break;

    case kGestureTypePinch:
      if (fingers_.size() == 2) {
        return;
      } else {
        current_gesture_type_ = kGestureTypeNull;
      }
      break;
  }
}

bool ImmediateInterpreter::UpdatePinchState(
    const HardwareState& hwstate, bool reset) {

  // perform reset to "don't know" state
  if (reset) {
    pinch_guess_start_ = -1.0f;
    pinch_locked_ = false;
    two_finger_start_distance_ = -1.0f;
    return false;
  }

  // once locked stay locked until reset.
  if (pinch_locked_) {
    return false;
  }

  // check if we have two valid fingers
  if (fingers_.size() != 2) {
    return false;
  }
  const FingerState* finger1 = hwstate.GetFingerState(*fingers_.begin());
  const FingerState* finger2 = hwstate.GetFingerState(*(fingers_.begin()+1));
  if (finger1 == NULL || finger2 == NULL) {
    Err("Finger unexpectedly NULL");
    return false;
  }

  // assign the bottom finger to finger2
  if (finger1->position_y > finger2->position_y) {
    std::swap(finger1, finger2);
  }

  // Calculate start distance between fingers and cache value
  if (two_finger_start_distance_ < 0) {
    two_finger_start_distance_ = sqrtf(TwoFingerDistanceSq(hwstate));
  }

  // Check if the two fingers have start positions
  if (!MapContainsKey(start_positions_, finger1->tracking_id) ||
      !MapContainsKey(start_positions_, finger2->tracking_id)) {
    return false;
  }

  // Pinch gesture detection
  //
  // The pinch gesture detection will try to make a guess about whether a pinch
  // or not-a-pinch is performed. If the guess stays valid for a specific time
  // (slow but consistent movement) or we get a certain decision (fast
  // gesturing) the decision is locked until the state is reset.
  // * A high ratio of the traveled distances between fingers indicates
  //   that a pinch is NOT performed.
  // * Strong movement of both fingers in opposite directions indicates
  //   that a pinch IS performed.

  Point delta1 = FingerTraveledVector(*finger1, false);
  Point delta2 = FingerTraveledVector(*finger2, false);

  // dot product. dot < 0 if fingers move away from each other.
  float dot  = delta1.x_ * delta2.x_ + delta1.y_ * delta2.y_;
  // squared distances both finger have been traveled.
  float d1sq = delta1.x_ * delta1.x_ + delta1.y_ * delta1.y_;
  float d2sq = delta2.x_ * delta2.x_ + delta2.y_ * delta2.y_;
  // ratio between distances. High value when fingers move
  float ratio = d1sq > d2sq ? d2sq / d1sq : d1sq / d2sq;

  // true if movement is not strong enough to be distinguished from noise.
  bool movement_below_noise = (d1sq + d2sq < 2.0*pinch_noise_level_.val_);

  // guesses if a pinch is being performed or not.
  double guess_ratio = no_pinch_guess_ratio_.val_;
  double guess_min_mov = pinch_guess_min_movement_.val_;
  guess_min_mov *= guess_min_mov;
  bool no_pinch_guess = (ratio > guess_ratio);
  bool pinch_guess = d1sq > guess_min_mov && d2sq > guess_min_mov && dot < 0;

  // Thumb is in dampened zone: Only allow inward pinch
  if (FingerInDampenedZone(*finger2)) {
    no_pinch_guess |= (delta2.y_ > 0);
    pinch_guess &= (delta2.y_ < 0);
  }

  // do state transitions and final decision
  if (pinch_guess_start_ < 0) {
    // "Don't Know"-state

    // Determine guess.
    if (!movement_below_noise) {
      if (no_pinch_guess && !pinch_guess) {
        pinch_guess_ = false;
        pinch_guess_start_ = hwstate.timestamp;
      }
      if (pinch_guess && !no_pinch_guess) {
        pinch_guess_ = true;
        pinch_guess_start_ = hwstate.timestamp;
      }
    }
  } else {
    // "Guessed"-state

    // suppress cursor movement when we guess a pinch gesture
    if (pinch_guess_) {
      for (size_t i = 0; i < hwstate.finger_cnt; ++i) {
        FingerState* finger_state = &hwstate.fingers[i];
        finger_state->flags |= GESTURES_FINGER_WARP_X;
        finger_state->flags |= GESTURES_FINGER_WARP_Y;
      }
    }

    // Go back to "Don't Know"-state if guess is no longer valid
    if (pinch_guess_ != pinch_guess ||
        pinch_guess_ == no_pinch_guess ||
        movement_below_noise) {
      pinch_guess_start_ = -1.0f;
      return false;
    }

    // certain decisions if pinch is being performed or not
    double cert_ratio = no_pinch_certain_ratio_.val_;
    double cert_min_mov = pinch_certain_min_movement_.val_;
    cert_min_mov *= cert_min_mov;
    bool no_pinch_certain = d1sq + d2sq > cert_min_mov && (ratio > cert_ratio);
    bool pinch_certain = d1sq > cert_min_mov && d2sq > cert_min_mov && dot < 0;

    // guessed for long enough or certain decision was made: lock
    if (hwstate.timestamp - pinch_guess_start_ > 0.05 ||
        pinch_guess_ == pinch_certain ||
        pinch_guess_ != no_pinch_certain) {
      pinch_locked_ = true;
      return pinch_guess_;
    }
  }

  return false;
}

bool ImmediateInterpreter::TwoFingersGesturing(
    const FingerState& finger1,
    const FingerState& finger2) const {
  // Make sure distance between fingers isn't too great
  if (!finger_metrics_->FingersCloseEnoughToGesture(finger1, finger2))
    return false;

  // Next, if two fingers are moving a lot, they are gesturing together.
  if (started_moving_time_ > changed_time_) {
    // Fingers are moving
    float dist1_sq = DistanceTravelledSq(finger1, false);
    float dist2_sq = DistanceTravelledSq(finger2, false);
    if (thumb_movement_factor_.val_ * thumb_movement_factor_.val_ *
        max(dist1_sq, dist2_sq) < min(dist1_sq, dist2_sq)) {
      return true;
    }
  }

  // Make sure the pressure difference isn't too great for vertically
  // aligned contacts
  float pdiff = fabsf(finger1.pressure - finger2.pressure);
  float xdist = fabsf(finger1.position_x - finger2.position_x);
  float ydist = fabsf(finger1.position_y - finger2.position_y);
  if (pdiff > two_finger_pressure_diff_thresh_.val_ && ydist > xdist)
    return false;

  const float kMin2fDistThreshSq = tapping_finger_min_separation_.val_ *
      tapping_finger_min_separation_.val_;
  float dist_sq = xdist * xdist + ydist * ydist;
  // Make sure distance between fingers isn't too small
  if ((dist_sq < kMin2fDistThreshSq) &&
      !(finger1.flags & GESTURES_FINGER_MERGE))
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
  if (FingerInDampenedZone(finger1) ||
      (finger1.flags & GESTURES_FINGER_POSSIBLE_PALM)) {
    dampened_zone_occupied = true;
    damp_dx = dx1;
    damp_dy = dy1;
    non_damp_dx = dx2;
    non_damp_dy = dy2;
  }
  if (FingerInDampenedZone(finger2) ||
      (finger2.flags & GESTURES_FINGER_POSSIBLE_PALM)) {
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

  float dx[] = {
    min_finger->position_x - start_positions_[min_finger->tracking_id].x_,
    center_finger->position_x - start_positions_[center_finger->tracking_id].x_,
    max_finger->position_x - start_positions_[max_finger->tracking_id].x_
  };
  float dy[] = {
    min_finger->position_y - start_positions_[min_finger->tracking_id].y_,
    center_finger->position_y - start_positions_[center_finger->tracking_id].y_,
    max_finger->position_y - start_positions_[max_finger->tracking_id].y_
  };
  // pick horizontal or vertical
  float *deltas = fabsf(dx[0]) > fabsf(dy[0]) ? dx : dy;
  swipe_is_vertical_ = deltas == dy;

  // All three fingers must move in the same direction.
  if ((deltas[0] > 0 && !(deltas[1] > 0 && deltas[2] > 0)) ||
      (deltas[0] < 0 && !(deltas[1] < 0 && deltas[2] < 0))) {
    return kGestureTypeNull;
  }

  // All three fingers must have traveled far enough.
  if (fabsf(deltas[0]) < three_finger_swipe_distance_thresh_.val_ ||
      fabsf(deltas[1]) < three_finger_swipe_distance_thresh_.val_ ||
      fabsf(deltas[2]) < three_finger_swipe_distance_thresh_.val_) {
    return kGestureTypeNull;
  }

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
    const FingerMap& gs_fingers,
    const bool same_fingers,
    stime_t now,
    stime_t* timeout) {
  unsigned down = 0;
  unsigned up = 0;
  UpdateTapState(hwstate, gs_fingers, same_fingers, now, &down, &up, timeout);
  if (down == 0 && up == 0) {
    return;
  }
  Log("UpdateTapGesture: Tap Generated");
  result_ = Gesture(kGestureButtonsChange,
                    state_buffer_.Get(1)->timestamp,
                    now,
                    down,
                    up);
}

void ImmediateInterpreter::UpdateTapState(
    const HardwareState* hwstate,
    const FingerMap& gs_fingers,
    const bool same_fingers,
    stime_t now,
    unsigned* buttons_down,
    unsigned* buttons_up,
    stime_t* timeout) {
  if (tap_to_click_state_ == kTtcIdle && (!tap_enable_.val_ ||
                                          tap_paused_.val_))
    return;

  FingerMap tap_gs_fingers;

  if (hwstate)
    RemoveMissingIdsFromSet(&tap_dead_fingers_, *hwstate);

  bool cancel_tapping = false;
  if (hwstate) {
    for (int i = 0; i < hwstate->finger_cnt; ++i) {
      if (hwstate->fingers[i].flags &
          (GESTURES_FINGER_NO_TAP | GESTURES_FINGER_MERGE))
        cancel_tapping = true;
    }
    for (FingerMap::const_iterator it =
             gs_fingers.begin(), e = gs_fingers.end(); it != e; ++it) {
      const FingerState* fs = hwstate->GetFingerState(*it);
      if (!fs) {
        Err("Missing finger state?!");
        continue;
      }
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

  if (phys_button_down) {
    // Don't allow any current fingers to tap ever
    for (size_t i = 0; i < hwstate->finger_cnt; i++)
      tap_dead_fingers_.insert(hwstate->fingers[i].tracking_id);
  }

  if (hwstate && (!same_fingers || prev_tap_gs_fingers_ != tap_gs_fingers)) {
    // See if fingers were added
    for (FingerMap::const_iterator it =
             tap_gs_fingers.begin(), e = tap_gs_fingers.end(); it != e; ++it)
      if (!SetContainsValue(prev_tap_gs_fingers_, *it)) {
        // Gesturing finger wasn't in prev state. It's new.
        const FingerState* fs = hwstate->GetFingerState(*it);
        if (FingerTooCloseToTap(*hwstate, *fs) ||
            FingerTooCloseToTap(*state_buffer_.Get(1), *fs) ||
            SetContainsValue(tap_dead_fingers_, fs->tracking_id))
          continue;
        added_fingers.insert(*it);
        Log("TTC: Added %d", *it);
      }

    // See if fingers were removed or are now non-gesturing (dead)
    for (FingerMap::const_iterator it =
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
  // Also, if the physical button is down or previous gesture type is scroll,
  // we go to (or stay in) Idle state.

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
  // |  [SubsequentTapBegan] --<timeout/move w/o delay: send click>-------->|
  // |     | | | release all fingers: send left click                       |
  // |<----+-+-'                                                            |
  // |     | `-> start non-left click: send left click; goto FirstTapBegan  |
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

  if (tap_to_click_state_ != kTtcIdle)
    Log("TTC State: %s", TapToClickStateName(tap_to_click_state_));
  if (!hwstate)
    Log("TTC: This is a timer callback");
  if (phys_button_down || KeyboardRecentlyUsed(now) ||
      prev_result_.type == kGestureTypeScroll ||
      cancel_tapping) {
    Log("TTC: Forced to idle");
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
            *hwstate, *state_buffer_.Get(1), added_fingers, removed_fingers,
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
          *hwstate, *state_buffer_.Get(1), added_fingers,
          removed_fingers, dead_fingers);
      Log("TTC: Is tap? %d Is moving? %d",
          tap_record_.TapComplete(),
          tap_record_.Moving(*hwstate, tap_move_dist_.val_));
      if (tap_record_.TapComplete()) {
        if (!tap_record_.MinTapPressureMet() ||
            !tap_record_.FingersBelowMaxAge()) {
          SetTapToClickState(kTtcIdle, now);
        } else if (tap_record_.TapType() == GESTURES_BUTTON_LEFT &&
                   tap_drag_enable_.val_) {
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
            *hwstate, *state_buffer_.Get(1), added_fingers, removed_fingers,
            dead_fingers);

        // If more than one finger is touching: Send click
        // and return to FirstTapBegan state.
        if (tap_record_.TapType() != GESTURES_BUTTON_LEFT) {
          *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
          SetTapToClickState(kTtcFirstTapBegan, now);
        } else {
          tap_drag_last_motion_time_ = now;
          tap_drag_finger_was_stationary_ = false;
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
        tap_record_.Update(*hwstate, *state_buffer_.Get(1), added_fingers,
                           removed_fingers, dead_fingers);

      if (!tap_record_.Motionless(*hwstate, *state_buffer_.Get(1),
                                  tap_max_movement_.val_)) {
        tap_drag_last_motion_time_ = now;
      }
      if (tap_record_.TapType() == GESTURES_BUTTON_LEFT &&
          now - tap_drag_last_motion_time_ > tap_drag_stationary_time_.val_) {
        tap_drag_finger_was_stationary_ = true;
      }

      if (is_timeout || tap_record_.Moving(*hwstate, tap_move_dist_.val_)) {
        if (tap_record_.TapType() == GESTURES_BUTTON_LEFT) {
          if (is_timeout) {
            // moving with just one finger. Start dragging.
            *buttons_down = GESTURES_BUTTON_LEFT;
            SetTapToClickState(kTtcDrag, now);
          } else {
            bool drag_delay_met = (now - tap_to_click_state_entered_
                                   > tap_drag_delay_.val_);
            if (drag_delay_met && tap_drag_finger_was_stationary_) {
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
      if (tap_record_.TapType() != GESTURES_BUTTON_LEFT) {
        // We aren't going to drag, so send left click now and handle current
        // tap afterwards.
        *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(kTtcFirstTapBegan, now);
      }
      if (tap_record_.TapComplete()) {
        *buttons_down = *buttons_up = GESTURES_BUTTON_LEFT;
        SetTapToClickState(kTtcTapComplete, now);
        Log("TTC: Subsequent left tap complete");
      }
      break;
    case kTtcDrag:
      if (hwstate)
        tap_record_.Update(
            *hwstate, *state_buffer_.Get(1), added_fingers, removed_fingers,
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
            *hwstate, *state_buffer_.Get(1), added_fingers, removed_fingers,
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
            *hwstate, *state_buffer_.Get(1), added_fingers, removed_fingers,
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
  if (tap_to_click_state_ != kTtcIdle)
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

bool ImmediateInterpreter::FingerInDampenedZone(
    const FingerState& finger) const {
  // TODO(adlr): cache thresh
  float thresh = hw_props_.bottom - bottom_zone_size_.val_;
  return finger.position_y > thresh;
}

void ImmediateInterpreter::FillStartPositions(const HardwareState& hwstate) {
  RemoveMissingIdsFromMap(&origin_positions_, hwstate);

  for (short i = 0; i < hwstate.finger_cnt; i++) {
    Point point(hwstate.fingers[i].position_x,
                hwstate.fingers[i].position_y);
    start_positions_[hwstate.fingers[i].tracking_id] = point;
    if (!MapContainsKey(origin_positions_, hwstate.fingers[i].tracking_id))
      origin_positions_[hwstate.fingers[i].tracking_id] = point;
  }
}

int ImmediateInterpreter::GetButtonTypeForTouchCount(int touch_count) const {
  if (touch_count == 2)
    return GESTURES_BUTTON_RIGHT;
  if (touch_count == 3 && three_finger_click_enable_.val_)
    return GESTURES_BUTTON_MIDDLE;
  return GESTURES_BUTTON_LEFT;
}

int ImmediateInterpreter::EvaluateButtonType(
    const HardwareState& hwstate, stime_t button_down_time) {
  const float kMoveDistSq = button_move_dist_.val_ * button_move_dist_.val_;
  const float kMaxDistSq = button_max_dist_from_expected_.val_ *
                           button_max_dist_from_expected_.val_;

  // Handle T5R2 touchpads
  if (hw_props_.supports_t5r2 && hwstate.touch_cnt > 2) {
    if (hwstate.touch_cnt - thumb_.size() == 3 &&
        three_finger_click_enable_.val_ && t5r2_three_finger_click_enable_.val_)
      return GESTURES_BUTTON_MIDDLE;
    return GESTURES_BUTTON_RIGHT;
  }

  // Copy all fingers to an array, but leave out palms
  const FingerState* fingers[4] = {0};
  int num_fingers = 0;
  for (int i = 0; i < hwstate.touch_cnt; ++i) {
    if (!(hwstate.fingers[i].flags & GESTURES_FINGER_PALM)) {
      // we don't support more than 4 fingers
      if (num_fingers >= 4)
        return hwstate.buttons_down;
      fingers[num_fingers] = &hwstate.fingers[i];
      num_fingers++;
    }
  }

  // Single finger is trivial
  if (num_fingers <= 1)
    return hwstate.buttons_down;

  // Sort fingers array by origin timestamp
  FingerOriginCompare comparator(this);
  std::sort(fingers, fingers + num_fingers, comparator);

  // The status describes if a finger is recent (touched down recently),
  // cold (touched down a while ago, but did not move) or hot (has moved).
  // However thumbs are always forced to be "cold".
  FingerButtonClickStatus fingers_status[4];
  for (int i = 0; i < num_fingers; ++i) {
    stime_t finger_age = button_down_time -
                         finger_origin_timestamp(fingers[i]->tracking_id);
    if (!SetContainsValue(pointing_, fingers[i]->tracking_id))
      fingers_status[i] = GESTURES_FINGER_STATUS_COLD;
    else if (DistanceTravelledSq(*fingers[i], true) > kMoveDistSq)
      fingers_status[i] = GESTURES_FINGER_STATUS_HOT;
    else if (finger_age < right_click_second_finger_age_.val_)
      fingers_status[i] = GESTURES_FINGER_STATUS_RECENT;
    else
      fingers_status[i] = GESTURES_FINGER_STATUS_COLD;
  }

  int num_recent = 0;
  for (int i = 0; i < num_fingers; ++i)
    num_recent += (fingers_status[i] == GESTURES_FINGER_STATUS_RECENT);

  int num_cold = 0;
  for (int i = 0; i < num_fingers; ++i)
    num_cold += (fingers_status[i] == GESTURES_FINGER_STATUS_COLD);

  int num_hot = num_fingers-num_recent - num_cold;
  Log("EvaluateButtonType: R/C/H: %d/%d/%d", num_recent, num_cold, num_hot);

  // Handle 2 finger cases:
  if (num_fingers == 2) {
    // Only one finger hot -> moving -> left click
    if (num_hot == 1)
        return GESTURES_BUTTON_LEFT;

    // Close fingers -> same hand -> right click
    // Fingers apart -> second hand finger or thumb -> left click
    if (TwoFingersGesturing(*fingers[0], *fingers[1]))
      return GESTURES_BUTTON_RIGHT;
    else
      return GESTURES_BUTTON_LEFT;
  }

  // Handle cases with 3 or more fingers:

  // A single recent touch, or a single cold touch (with all others being hot)
  // could be a thumb or a second hand finger.
  if (num_recent == 1 || (num_cold == 1 && num_hot == num_fingers - 1)) {
    // The ambiguous finger is either the most recent one, or the only cold one.
    const FingerState* ambiguous_finger = fingers[num_fingers - 1];
    if (num_recent != 1) {
      for (int i = 0; i < num_fingers; ++i) {
        if (fingers_status[i] == GESTURES_FINGER_STATUS_COLD) {
          ambiguous_finger = fingers[i];
          break;
        }
      }
    }
    // If it's in the dampened zone we will expect it to be a thumb.
    // Otherwise it's a second hand finger
    if (FingerInDampenedZone(*ambiguous_finger))
      return GetButtonTypeForTouchCount(num_fingers - 1);
    else
      return GESTURES_BUTTON_LEFT;
  }

  // If all fingers are recent we can be sure they are from the same hand.
  if (num_recent == num_fingers) {
    // Only if all fingers are in the same zone, we can be sure that none
    // of them is a thumb.
    int num_dampened = 0;
    for (int i = 0; i < num_fingers; ++i)
      num_dampened += FingerInDampenedZone(*fingers[i]);
    Log("EvaluateButtonType: Dampened: %d", num_dampened);
    if (num_dampened == 0 || num_dampened == num_recent)
      return GetButtonTypeForTouchCount(num_recent);
  }

  // To make a decision after this point we need to figure out if and how
  // many of the fingers are grouped together. We do so by finding the pair
  // of closest fingers, and then calculate where we expect the remaining
  // fingers to be found.
  // If they are not in the expected place, they will be called separate.
  Log("EvaluateButtonType: Falling back to location based detection");

  // Find pair with the closest distance
  const FingerState* pair_a = NULL;
  const FingerState* pair_b = NULL;
  float pair_dist_sq = std::numeric_limits<float>::infinity();
  for (int i = 0; i < num_fingers; ++i) {
    for (int j = 0; j < i; ++j) {
      float dist_sq = DistSq(*fingers[i], *fingers[j]);
      if (dist_sq < pair_dist_sq) {
        pair_a = fingers[i];
        pair_b = fingers[j];
        pair_dist_sq = dist_sq;
      }
    }
  }

  int num_separate = 0;
  const FingerState* last_separate = NULL;

  if (finger_metrics_->FingersCloseEnoughToGesture(*pair_a, *pair_b)) {
    // Expect the remaining fingers to be next to the pair, all with the same
    // distance from each other.
    float dx = pair_b->position_x - pair_a->position_x;
    float dy = pair_b->position_y - pair_a->position_y;
    float expected1_x = pair_a->position_x + 2 * dx;
    float expected1_y = pair_a->position_y + 2 * dy;
    float expected2_x = pair_b->position_x - 2 * dx;
    float expected2_y = pair_b->position_y - 2 * dy;

    // Check if remaining fingers are close to the expected positions
    for (int i = 0; i < num_fingers; ++i) {
      if (fingers[i] == pair_a || fingers[i] == pair_b)
        continue;
      float dist1_sq = DistSqXY(*fingers[i], expected1_x, expected1_y);
      float dist2_sq = DistSqXY(*fingers[i], expected2_x, expected2_y);
      if (dist1_sq > kMaxDistSq && dist2_sq > kMaxDistSq) {
        num_separate++;
        last_separate = fingers[i];
      }
    }
  } else {
    // In case the pair is not close we have to fall back to using the
    // dampened zone
    Log("EvaluateButtonType: Falling back to dampened zone separation");
    for (int i = 0; i < num_fingers; ++i) {
      if (FingerInDampenedZone(*fingers[i])) {
        num_separate++;
        last_separate = fingers[i];
      }
    }
  }

  // All fingers next to each other
  if (num_separate == 0)
    return GetButtonTypeForTouchCount(num_fingers);

  // The group with the last finger counts!
  // Exception: If the separates have only one finger and it's a thumb
  //            count the other group
  int num_pressing;
  if (fingers[num_fingers - 1] == last_separate &&
      !(num_separate == 1 && FingerInDampenedZone(*last_separate))) {
      num_pressing = num_separate;
  } else {
    num_pressing = num_fingers - num_separate;
  }
  Log("EvaluateButtonType: Pressing: %d", num_pressing);
  return GetButtonTypeForTouchCount(num_pressing);
}

void ImmediateInterpreter::UpdateStartedMovingTime(
    const HardwareState& hwstate,
    const FingerMap& gs_fingers) {
  SetRemoveMissing(&moving_, gs_fingers);
  if (moving_.size() == gs_fingers.size())
    return;  // All fingers already started moving
  const float kMinDistSq =
      change_move_distance_.val_ * change_move_distance_.val_;
  for (FingerMap::const_iterator
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
    if (SetContainsValue(moving_, fs->tracking_id)) {
      // This finger already moving
      continue;
    }
    const Point& start_position = start_positions_[*it];
    float dist_sq = DistSqXY(*fs, start_position.x_, start_position.y_);
    if (dist_sq > kMinDistSq) {
      started_moving_time_ = hwstate.timestamp;
      moving_.insert(fs->tracking_id);
    }
  }
}

void ImmediateInterpreter::UpdateButtons(const HardwareState& hwstate,
                                         stime_t* timeout) {
  // TODO(miletus): To distinguish between left/right buttons down
  bool prev_button_down = state_buffer_.Get(1)->buttons_down;
  bool button_down = hwstate.buttons_down;
  if (!prev_button_down && !button_down)
    return;
  bool phys_down_edge = button_down && !prev_button_down;
  bool phys_up_edge = !button_down && prev_button_down;
  if (phys_down_edge) {
    finger_seen_since_button_down_ = false;
    sent_button_down_ = false;
    button_down_timeout_ = hwstate.timestamp + button_evaluation_timeout_.val_;
  }

  // If we haven't seen a finger on the pad yet we shouldn't do anything
  finger_seen_since_button_down_ =
      finger_seen_since_button_down_ || (hwstate.finger_cnt > 0);
  if (!finger_seen_since_button_down_ && !zero_finger_click_enable_.val_)
    return;

  if (!sent_button_down_) {
    stime_t button_down_time = button_down_timeout_ -
                               button_evaluation_timeout_.val_;
    button_type_ = EvaluateButtonType(hwstate, button_down_time);

    if (!hwstate.SameFingersAs(*state_buffer_.Get(0))) {
      // Fingers have changed since last state, reset timeout
      button_down_timeout_ = hwstate.timestamp + button_finger_timeout_.val_;
    }

    // button_up before button_evaluation_timeout_ expired.
    // Send up & down for button that was previously down, but not yet sent.
    if (button_type_ == GESTURES_BUTTON_NONE)
      button_type_ = prev_button_down;
    // Send button down if timeout has been reached or button up happened
    if (button_down_timeout_ <= hwstate.timestamp ||
        phys_up_edge) {
      // Send button down
      if (result_.type == kGestureTypeButtonsChange)
        Err("Gesture type already button?!");
      result_ = Gesture(kGestureButtonsChange,
                        state_buffer_.Get(1)->timestamp,
                        hwstate.timestamp,
                        button_type_,
                        0);
      sent_button_down_ = true;
    } else if (timeout) {
      *timeout = button_down_timeout_ - hwstate.timestamp;
    }
  }
  if (phys_up_edge) {
    // Send button up
    if (result_.type != kGestureTypeButtonsChange)
      result_ = Gesture(kGestureButtonsChange,
                        state_buffer_.Get(1)->timestamp,
                        hwstate.timestamp,
                        0,
                        button_type_);
    else
      result_.details.buttons.up = button_type_;
    // Reset button state
    button_type_ = GESTURES_BUTTON_NONE;
    button_down_timeout_ = 0;
    sent_button_down_ = false;
  }
}

void ImmediateInterpreter::UpdateButtonsTimeout(stime_t now) {
  if (sent_button_down_) {
    Err("How is sent_button_down_ set?");
    return;
  }
  if (button_type_ != GESTURES_BUTTON_LEFT) {
    Err("How is button_type_ not GESTURES_BUTTON_LEFT?");
    return;
  }
  sent_button_down_ = true;
  result_ = Gesture(kGestureButtonsChange,
                    state_buffer_.Get(1)->timestamp,
                    now,
                    GESTURES_BUTTON_LEFT,
                    0);
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

void ImmediateInterpreter::FillResultGesture(
    const HardwareState& hwstate,
    const FingerMap& fingers) {
  switch (current_gesture_type_) {
    case kGestureTypeMove: {
      if (fingers.empty())
        return;
      // Use highest finger (the one closes to the keyboard), excluding
      // palms, to compute motion. First, need to find out which finger that is.
      const FingerState* current = NULL;
      for (FingerMap::const_iterator it =
               fingers.begin(), e = fingers.end(); it != e; ++it) {
        const FingerState* fs = hwstate.GetFingerState(*it);
        if (!current || fs->position_y < current->position_y ||
            (current->flags & GESTURES_FINGER_POSSIBLE_PALM &&
             !(fs->flags & GESTURES_FINGER_POSSIBLE_PALM)))
          current = fs;
      }
      // Find corresponding finger id in previous state
      const FingerState* prev =
          state_buffer_.Get(1)->GetFingerState(current->tracking_id);
      if (!prev || !current)
        return;
      if (current->flags & GESTURES_FINGER_MERGE)
        return;
      bool high_pressure_change =
        scroll_manager_.PressureChangingSignificantly(state_buffer_, *current);
      if (high_pressure_change) {
        scroll_manager_.prev_result_high_pressure_change_ = true;
        return;
      }
      scroll_manager_.prev_result_high_pressure_change_ = false;
      float dx = current->position_x - prev->position_x;
      if (current->flags & GESTURES_FINGER_WARP_X_MOVE)
        dx = 0.0;
      float dy = current->position_y - prev->position_y;
      if (current->flags & GESTURES_FINGER_WARP_Y_MOVE)
        dy = 0.0;
      if (dx != 0.0 || dy != 0.0) {
        result_ = Gesture(kGestureMove,
                          state_buffer_.Get(1)->timestamp,
                          hwstate.timestamp,
                          dx,
                          dy);
      }
      break;
    }
    case kGestureTypeScroll: {
      if (!scroll_manager_.ComputeScroll(state_buffer_,
                                         prev_gs_fingers_,
                                         fingers,
                                         prev_gesture_type_,
                                         prev_result_,
                                         &result_,
                                         &scroll_buffer_))
        return;
      break;
    }
    case kGestureTypeFling: {
      scroll_manager_.ComputeFling(state_buffer_, scroll_buffer_, &result_);
      break;
    }
    case kGestureTypeSwipe: {
      if (!three_finger_swipe_enable_.val_)
        break;
      float sum_delta[] = { 0.0, 0.0 };
      bool valid[] = { true, true };
      float finger_cnt[] = { 0.0, 0.0 };
      float FingerState::*fields[] = { &FingerState::position_x,
                                       &FingerState::position_y };
      for (FingerMap::const_iterator it =
               fingers.begin(), e = fingers.end(); it != e; ++it) {
        if (!state_buffer_.Get(1)->GetFingerState(*it)) {
          Err("missing prev state?");
          continue;
        }
        // We have this loop in case we want to compute diagonal swipes at
        // some point, even if currently we go with just one axis.
        for (size_t i = 0; i < arraysize(fields); i++) {
          bool correct_axis = (i == 1) == swipe_is_vertical_;
          if (!valid[i] || !correct_axis)
            continue;
          float FingerState::*field = fields[i];
          float delta = hwstate.GetFingerState(*it)->*field -
              state_buffer_.Get(1)->GetFingerState(*it)->*field;
          // The multiply is to see if they have the same sign:
          if (sum_delta[i] == 0.0 || sum_delta[i] * delta > 0) {
            sum_delta[i] += delta;
            finger_cnt[i] += 1.0;
          } else {
            sum_delta[i] = 0.0;
            valid[i] = false;
          }
        }
      }
      result_ = Gesture(
          kGestureSwipe, state_buffer_.Get(1)->timestamp,
          hwstate.timestamp,
          (!swipe_is_vertical_ && finger_cnt[0]) ?
          sum_delta[0] / finger_cnt[0] : 0.0,
          (swipe_is_vertical_ && finger_cnt[1]) ?
          sum_delta[1] / finger_cnt[1] : 0.0);
      break;
    }
    case kGestureTypeSwipeLift: {
      result_ = Gesture(kGestureSwipeLift,
                        state_buffer_.Get(1)->timestamp,
                        hwstate.timestamp);
      break;
    }

    case kGestureTypePinch: {
      float current_dist = sqrtf(TwoFingerDistanceSq(hwstate));
      result_ = Gesture(kGesturePinch, changed_time_, hwstate.timestamp,
                        current_dist / two_finger_start_distance_);
      break;
    }
    default:
      result_.type = kGestureTypeNull;
  }
  scroll_manager_.UpdateScrollEventBuffer(current_gesture_type_,
                                          &scroll_buffer_);
  if (result_.type == kGestureTypeMove ||
      result_.type == kGestureTypeScroll)
    last_movement_timestamp_ = hwstate.timestamp;
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

void ImmediateInterpreter::SetHardwarePropertiesImpl(
    const HardwareProperties& hw_props) {
  hw_props_ = hw_props;
  state_buffer_.Reset(hw_props_.max_finger_cnt);
}

bool AnyGesturingFingerLeft(const HardwareState& state,
                            const FingerMap& prev_gs_fingers) {
  for (FingerMap::const_iterator it = prev_gs_fingers.begin(),
                                 e = prev_gs_fingers.end(); it != e; ++it) {
    if (!state.GetFingerState(*it)) {
      return true;
    }
  }
  return false;
}

}  // namespace gestures
