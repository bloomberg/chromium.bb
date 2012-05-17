// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/gestures/gesture_sequence.h"

#include <cmath>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/base/events.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/gfx/rect.h"

namespace ui {

namespace {

// ui::EventType is mapped to TouchState so it can fit into 3 bits of
// Signature.
enum TouchState {
  TS_RELEASED,
  TS_PRESSED,
  TS_MOVED,
  TS_STATIONARY,
  TS_CANCELLED,
  TS_UNKNOWN,
};

// Get equivalent TouchState from EventType |type|.
TouchState TouchEventTypeToTouchState(ui::EventType type) {
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
      return TS_RELEASED;
    case ui::ET_TOUCH_PRESSED:
      return TS_PRESSED;
    case ui::ET_TOUCH_MOVED:
      return TS_MOVED;
    case ui::ET_TOUCH_STATIONARY:
      return TS_STATIONARY;
    case ui::ET_TOUCH_CANCELLED:
      return TS_CANCELLED;
    default:
      DVLOG(1) << "Unknown Touch Event type";
  }
  return TS_UNKNOWN;
}

// Gesture signature types for different values of combination (GestureState,
// touch_id, ui::EventType, touch_handled), see Signature for more info.
//
// Note: New addition of types should be placed as per their Signature value.
#define G(gesture_state, id, touch_state, handled) 1 + ( \
  (((touch_state) & 0x7) << 1) |                         \
  ((handled) ? (1 << 4) : 0) |                           \
  (((id) & 0xfff) << 5) |                                \
  ((gesture_state) << 17))

enum EdgeStateSignatureType {
  GST_NO_GESTURE_FIRST_PRESSED =
      G(GS_NO_GESTURE, 0, TS_PRESSED, false),

  GST_PENDING_SYNTHETIC_CLICK_FIRST_RELEASED =
      G(GS_PENDING_SYNTHETIC_CLICK, 0, TS_RELEASED, false),

  GST_PENDING_SYNTHETIC_CLICK_FIRST_MOVED =
      G(GS_PENDING_SYNTHETIC_CLICK, 0, TS_MOVED, false),

  GST_PENDING_SYNTHETIC_CLICK_FIRST_STATIONARY =
      G(GS_PENDING_SYNTHETIC_CLICK, 0, TS_STATIONARY, false),

  GST_PENDING_SYNTHETIC_CLICK_FIRST_CANCELLED =
      G(GS_PENDING_SYNTHETIC_CLICK, 0, TS_CANCELLED, false),

  GST_PENDING_SYNTHETIC_CLICK_SECOND_PRESSED =
      G(GS_PENDING_SYNTHETIC_CLICK, 1, TS_PRESSED, false),

  GST_SCROLL_FIRST_RELEASED =
      G(GS_SCROLL, 0, TS_RELEASED, false),

  GST_SCROLL_FIRST_MOVED =
      G(GS_SCROLL, 0, TS_MOVED, false),

  GST_SCROLL_FIRST_CANCELLED =
      G(GS_SCROLL, 0, TS_CANCELLED, false),

  GST_SCROLL_SECOND_PRESSED =
      G(GS_SCROLL, 1, TS_PRESSED, false),

  GST_PINCH_FIRST_MOVED =
      G(GS_PINCH, 0, TS_MOVED, false),

  GST_PINCH_SECOND_MOVED =
      G(GS_PINCH, 1, TS_MOVED, false),

  GST_PINCH_FIRST_RELEASED =
      G(GS_PINCH, 0, TS_RELEASED, false),

  GST_PINCH_SECOND_RELEASED =
      G(GS_PINCH, 1, TS_RELEASED, false),

  GST_PINCH_FIRST_CANCELLED =
      G(GS_PINCH, 0, TS_CANCELLED, false),

  GST_PINCH_SECOND_CANCELLED =
      G(GS_PINCH, 1, TS_CANCELLED, false),

  GST_PINCH_THIRD_PRESSED =
      G(GS_PINCH, 2, TS_PRESSED, false),

  GST_THREE_FINGER_SWIPE_FIRST_RELEASED =
      G(GS_THREE_FINGER_SWIPE, 0, TS_RELEASED, false),

  GST_THREE_FINGER_SWIPE_SECOND_RELEASED =
      G(GS_THREE_FINGER_SWIPE, 1, TS_RELEASED, false),

  GST_THREE_FINGER_SWIPE_THIRD_RELEASED =
      G(GS_THREE_FINGER_SWIPE, 2, TS_RELEASED, false),

  GST_THREE_FINGER_SWIPE_FIRST_MOVED =
      G(GS_THREE_FINGER_SWIPE, 0, TS_MOVED, false),

  GST_THREE_FINGER_SWIPE_SECOND_MOVED =
      G(GS_THREE_FINGER_SWIPE, 1, TS_MOVED, false),

  GST_THREE_FINGER_SWIPE_THIRD_MOVED =
      G(GS_THREE_FINGER_SWIPE, 2, TS_MOVED, false),

  GST_THREE_FINGER_SWIPE_FIRST_CANCELLED =
      G(GS_THREE_FINGER_SWIPE, 0, TS_CANCELLED, false),

  GST_THREE_FINGER_SWIPE_SECOND_CANCELLED =
      G(GS_THREE_FINGER_SWIPE, 1, TS_CANCELLED, false),

  GST_THREE_FINGER_SWIPE_THIRD_CANCELLED =
      G(GS_THREE_FINGER_SWIPE, 2, TS_CANCELLED, false),
};

// Builds a signature. Signatures are assembled by joining together
// multiple bits.
// 1 LSB bit so that the computed signature is always greater than 0
// 3 bits for the |type|.
// 1 bit for |touch_handled|
// 12 bits for |touch_id|
// 15 bits for the |gesture_state|.
EdgeStateSignatureType Signature(GestureState gesture_state,
                                 unsigned int touch_id,
                                 ui::EventType type,
                                 bool touch_handled) {
  CHECK((touch_id & 0xfff) == touch_id);
  TouchState touch_state = TouchEventTypeToTouchState(type);
  return static_cast<EdgeStateSignatureType>
      (G(gesture_state, touch_id, touch_state, touch_handled));
}
#undef G

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GestureSequence Public:

GestureSequence::GestureSequence(GestureEventHelper* helper)
    : state_(GS_NO_GESTURE),
      flags_(0),
      pinch_distance_start_(0.f),
      pinch_distance_current_(0.f),
      long_press_timer_(CreateTimer()),
      point_count_(0),
      helper_(helper) {
}

GestureSequence::~GestureSequence() {
}

GestureSequence::Gestures* GestureSequence::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::TouchStatus status) {
  if (status != ui::TOUCH_STATUS_UNKNOWN)
    return NULL;  // The event was consumed by a touch sequence.

  // Set a limit on the number of simultaneous touches in a gesture.
  if (event.GetTouchId() >= kMaxGesturePoints)
    return NULL;

  if (event.GetEventType() == ui::ET_TOUCH_PRESSED) {
    if (point_count_ == kMaxGesturePoints)
      return NULL;
    GesturePoint* new_point = &points_[event.GetTouchId()];
    // We shouldn't be able to get two PRESSED events from the same
    // finger without either a RELEASE or CANCEL in between.
    DCHECK(!points_[event.GetTouchId()].in_use());
    new_point->set_point_id(point_count_++);
    new_point->set_touch_id(event.GetTouchId());
  }

  GestureState last_state = state_;

  // NOTE: when modifying these state transitions, also update gestures.dot
  scoped_ptr<Gestures> gestures(new Gestures());
  GesturePoint& point = GesturePointForEvent(event);
  point.UpdateValues(event);
  flags_ = event.GetEventFlags();
  const int point_id = points_[event.GetTouchId()].point_id();
  if (point_id < 0)
    return NULL;
  switch (Signature(state_, point_id, event.GetEventType(), false)) {
    case GST_NO_GESTURE_FIRST_PRESSED:
      TouchDown(event, point, gestures.get());
      set_state(GS_PENDING_SYNTHETIC_CLICK);
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_RELEASED:
      if (Click(event, point, gestures.get()))
        point.UpdateForTap();
      set_state(GS_NO_GESTURE);
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_MOVED:
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_STATIONARY:
      if (ScrollStart(event, point, gestures.get())) {
        set_state(GS_SCROLL);
        if (ScrollUpdate(event, point, gestures.get()))
          point.UpdateForScroll();
      }
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_CANCELLED:
      NoGesture(event, point, gestures.get());
      break;
    case GST_SCROLL_FIRST_MOVED:
      if (scroll_type_ == ST_VERTICAL ||
          scroll_type_ == ST_HORIZONTAL)
        BreakRailScroll(event, point, gestures.get());
      if (ScrollUpdate(event, point, gestures.get()))
        point.UpdateForScroll();
      break;
    case GST_SCROLL_FIRST_RELEASED:
    case GST_SCROLL_FIRST_CANCELLED:
      ScrollEnd(event, point, gestures.get());
      set_state(GS_NO_GESTURE);
      break;
    case GST_SCROLL_SECOND_PRESSED:
    case GST_PENDING_SYNTHETIC_CLICK_SECOND_PRESSED:
      PinchStart(event, point, gestures.get());
      set_state(GS_PINCH);
      break;
    case GST_PINCH_FIRST_MOVED:
    case GST_PINCH_SECOND_MOVED:
      if (PinchUpdate(event, point, gestures.get())) {
        GetPointByPointId(0)->UpdateForScroll();
        GetPointByPointId(1)->UpdateForScroll();
      }
      break;
    case GST_PINCH_FIRST_RELEASED:
    case GST_PINCH_SECOND_RELEASED:
    case GST_PINCH_FIRST_CANCELLED:
    case GST_PINCH_SECOND_CANCELLED:
      PinchEnd(event, point, gestures.get());

      // Once pinch ends, it should still be possible to scroll with the
      // remaining finger on the screen.
      scroll_type_ = ST_FREE;
      set_state(GS_SCROLL);
      break;
    case GST_PINCH_THIRD_PRESSED:
      three_finger_swipe_has_fired_ = false;
      set_state(GS_THREE_FINGER_SWIPE);
      break;
    case GST_THREE_FINGER_SWIPE_FIRST_RELEASED:
    case GST_THREE_FINGER_SWIPE_SECOND_RELEASED:
    case GST_THREE_FINGER_SWIPE_THIRD_RELEASED:
    case GST_THREE_FINGER_SWIPE_FIRST_CANCELLED:
    case GST_THREE_FINGER_SWIPE_SECOND_CANCELLED:
    case GST_THREE_FINGER_SWIPE_THIRD_CANCELLED:
      GetPointByPointId(0)->ResetVelocity();
      GetPointByPointId(1)->ResetVelocity();
      GetPointByPointId(2)->ResetVelocity();
      set_state(GS_PINCH);
      break;
    case GST_THREE_FINGER_SWIPE_FIRST_MOVED:
    case GST_THREE_FINGER_SWIPE_SECOND_MOVED:
    case GST_THREE_FINGER_SWIPE_THIRD_MOVED:
      if (!three_finger_swipe_has_fired_) {
        ThreeFingerSwipeUpdate(event, point, gestures.get());
        GetPointByPointId(0)->UpdateForScroll();
        GetPointByPointId(1)->UpdateForScroll();
        GetPointByPointId(2)->UpdateForScroll();
      }
      break;
  }

  if (state_ != last_state)
    DVLOG(4) << "Gesture Sequence"
             << " State: " << state_
             << " touch id: " << event.GetTouchId();

  if (last_state == GS_PENDING_SYNTHETIC_CLICK && state_ != last_state)
    long_press_timer_->Stop();

  // The set of point_ids must be contiguous and include 0.
  // When a touch point is released, all points with ids greater than the
  // released point must have their ids decremented, or the set of point_ids
  // could end up with gaps.
  if (event.GetEventType() == ui::ET_TOUCH_RELEASED ||
      event.GetEventType() == ui::ET_TOUCH_CANCELLED) {
    GesturePoint& old_point = points_[event.GetTouchId()];
    for (int i = 0; i < kMaxGesturePoints; ++i) {
      GesturePoint& point = points_[i];
      if (point.point_id() > old_point.point_id())
        point.set_point_id(point.point_id() - 1);
    }
    if (old_point.in_use()) {
      old_point.Reset();
      --point_count_;
      DCHECK_GE(point_count_, 0);
    }
  }

  return gestures.release();
}

void GestureSequence::Reset() {
  set_state(GS_NO_GESTURE);
  for (int i = 0; i < kMaxGesturePoints; ++i)
    points_[i].Reset();
  point_count_ = 0;
}

////////////////////////////////////////////////////////////////////////////////
// GestureSequence Protected:

base::OneShotTimer<GestureSequence>* GestureSequence::CreateTimer() {
  return new base::OneShotTimer<GestureSequence>();
}

////////////////////////////////////////////////////////////////////////////////
// GestureSequence Private:

GesturePoint& GestureSequence::GesturePointForEvent(
    const TouchEvent& event) {
  return points_[event.GetTouchId()];
}

GesturePoint* GestureSequence::GetPointByPointId(int point_id) {
  DCHECK(0 <= point_id && point_id < kMaxGesturePoints);
  for (int i = 0; i < kMaxGesturePoints; ++i) {
    GesturePoint& point = points_[i];
    if (point.in_use() && point.point_id() == point_id)
      return &point;
  }
  NOTREACHED();
  return NULL;
}

void GestureSequence::AppendTapDownGestureEvent(const GesturePoint& point,
                                                Gestures* gestures) {
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_TAP_DOWN,
      point.first_touch_position(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      0.f, 0.f, 1 << point.touch_id()));
}

void GestureSequence::AppendClickGestureEvent(const GesturePoint& point,
                                              Gestures* gestures) {
  gfx::Rect er = point.enclosing_rectangle();
  gfx::Point center = er.CenterPoint();
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_TAP,
      center,
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      er.width() / 2,
      er.height() / 2,
      1 << point.touch_id()));
}

void GestureSequence::AppendDoubleClickGestureEvent(const GesturePoint& point,
                                                    Gestures* gestures) {
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_DOUBLE_TAP,
      point.first_touch_position(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      0.f, 0.f, 1 << point.touch_id()));
}

void GestureSequence::AppendScrollGestureBegin(const GesturePoint& point,
                                               const gfx::Point& location,
                                               Gestures* gestures) {
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_SCROLL_BEGIN,
      location,
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      0.f, 0.f, 1 << point.touch_id()));
}

void GestureSequence::AppendScrollGestureEnd(const GesturePoint& point,
                                             const gfx::Point& location,
                                             Gestures* gestures,
                                             float x_velocity,
                                             float y_velocity) {
  float railed_x_velocity = x_velocity;
  float railed_y_velocity = y_velocity;

  if (scroll_type_ == ST_HORIZONTAL)
    railed_y_velocity = 0;
  else if (scroll_type_ == ST_VERTICAL)
    railed_x_velocity = 0;

  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_SCROLL_END,
      location,
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      railed_x_velocity, railed_y_velocity, 1 << point.touch_id()));

  if (railed_x_velocity != 0 || railed_y_velocity != 0) {
    // TODO(sad|rjkroege): fling-curve is currently configured to work well with
    // touchpad scroll-events. This curve needs to be adjusted to work correctly
    // with both touchpad and touchscreen. Until then, multiply the velocity for
    // touchscreen with a constant.
    // http://crbug.com/120154
    gestures->push_back(helper_->CreateGestureEvent(
        ui::ET_SCROLL_FLING_START,
        location,
        flags_,
        base::Time::FromDoubleT(point.last_touch_time()),
        railed_x_velocity * 2.5, railed_y_velocity * 2.5,
        1 << point.touch_id()));
  }
}

void GestureSequence::AppendScrollGestureUpdate(const GesturePoint& point,
                                                const gfx::Point& location,
                                                Gestures* gestures) {
  int dx = point.x_delta();
  int dy = point.y_delta();

  if (scroll_type_ == ST_HORIZONTAL)
    dy = 0;
  else if (scroll_type_ == ST_VERTICAL)
    dx = 0;

  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_SCROLL_UPDATE,
      location,
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      dx, dy, 1 << point.touch_id()));
}

void GestureSequence::AppendPinchGestureBegin(const GesturePoint& p1,
                                              const GesturePoint& p2,
                                              Gestures* gestures) {
  gfx::Point center = p1.last_touch_position().Middle(p2.last_touch_position());
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_PINCH_BEGIN,
      center,
      flags_,
      base::Time::FromDoubleT(p1.last_touch_time()),
      0.f, 0.f, 1 << p1.touch_id() | 1 << p2.touch_id()));
}

void GestureSequence::AppendPinchGestureEnd(const GesturePoint& p1,
                                            const GesturePoint& p2,
                                            float scale,
                                            Gestures* gestures) {
  gfx::Point center = p1.last_touch_position().Middle(p2.last_touch_position());
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_PINCH_END,
      center,
      flags_,
      base::Time::FromDoubleT(p1.last_touch_time()),
      scale, 0.f, 1 << p1.touch_id() | 1 << p2.touch_id()));
}

void GestureSequence::AppendPinchGestureUpdate(const GesturePoint& p1,
                                               const GesturePoint& p2,
                                               float scale,
                                               Gestures* gestures) {
  // TODO(sad): Compute rotation and include it in delta_y.
  // http://crbug.com/113145
  gfx::Point center = p1.last_touch_position().Middle(p2.last_touch_position());
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_PINCH_UPDATE,
      center,
      flags_,
      base::Time::FromDoubleT(p1.last_touch_time()),
      scale, 0.f, 1 << p1.touch_id() | 1 << p2.touch_id()));
}

void GestureSequence::AppendThreeFingerSwipeGestureEvent(const GesturePoint& p1,
                                                         const GesturePoint& p2,
                                                         const GesturePoint& p3,
                                                         float x_velocity,
                                                         float y_velocity,
                                                         Gestures* gestures) {
  int x = (
      p1.last_touch_position().x() +
      p2.last_touch_position().x() +
      p3.last_touch_position().x()) / 3;
  int y = (
      p1.last_touch_position().y() +
      p2.last_touch_position().y() +
      p3.last_touch_position().y()) / 3;
  gfx::Point center(x, y);
  gestures->push_back(helper_->CreateGestureEvent(
      ui::ET_GESTURE_THREE_FINGER_SWIPE,
      center,
      flags_,
      base::Time::FromDoubleT(p1.last_touch_time()),
      x_velocity, y_velocity,
      1 << p1.touch_id() | 1 << p2.touch_id() | 1 << p3.touch_id()));
}

bool GestureSequence::Click(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_PENDING_SYNTHETIC_CLICK);
  if (point.IsInClickWindow(event)) {
    AppendClickGestureEvent(point, gestures);
    if (point.IsInDoubleClickWindow(event))
      AppendDoubleClickGestureEvent(point, gestures);
    return true;
  }
  return false;
}

bool GestureSequence::ScrollStart(const TouchEvent& event,
    GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_PENDING_SYNTHETIC_CLICK);
  if (point.IsInClickWindow(event) ||
      !point.IsInScrollWindow(event) ||
      !point.HasEnoughDataToEstablishRail())
    return false;
  AppendScrollGestureBegin(point, point.first_touch_position(), gestures);
  if (point.IsInHorizontalRailWindow())
    scroll_type_ = ST_HORIZONTAL;
  else if (point.IsInVerticalRailWindow())
    scroll_type_ = ST_VERTICAL;
  else
    scroll_type_ = ST_FREE;
  return true;
}

void GestureSequence::BreakRailScroll(const TouchEvent& event,
    GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_SCROLL);
  if (scroll_type_ == ST_HORIZONTAL &&
      point.BreaksHorizontalRail())
    scroll_type_ = ST_FREE;
  else if (scroll_type_ == ST_VERTICAL &&
           point.BreaksVerticalRail())
    scroll_type_ = ST_FREE;
}

bool GestureSequence::ScrollUpdate(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_SCROLL);
  if (!point.DidScroll(event, 0))
    return false;
  AppendScrollGestureUpdate(point, point.last_touch_position(), gestures);
  return true;
}

bool GestureSequence::NoGesture(const TouchEvent&,
    const GesturePoint& point, Gestures*) {
  Reset();
  return false;
}

bool GestureSequence::TouchDown(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_NO_GESTURE);
  AppendTapDownGestureEvent(point, gestures);
  long_press_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          GestureConfiguration::long_press_time_in_seconds() * 1000),
      this,
      &GestureSequence::AppendLongPressGestureEvent);
  return true;
}

void GestureSequence::AppendLongPressGestureEvent() {
  const GesturePoint* point = GetPointByPointId(0);
  scoped_ptr<GestureEvent> gesture(helper_->CreateGestureEvent(
      ui::ET_GESTURE_LONG_PRESS,
      point->first_touch_position(),
      flags_,
      base::Time::FromDoubleT(point->last_touch_time()),
      point->point_id(), 0.f, 1 << point->touch_id()));
  helper_->DispatchLongPressGestureEvent(gesture.get());
}

bool GestureSequence::ScrollEnd(const TouchEvent& event,
    GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_SCROLL);
  if (point.IsInFlickWindow(event)) {
    AppendScrollGestureEnd(point, point.last_touch_position(), gestures,
        point.XVelocity(), point.YVelocity());
  } else {
    AppendScrollGestureEnd(point, point.last_touch_position(), gestures,
        0.f, 0.f);
  }
  return true;
}

bool GestureSequence::PinchStart(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_SCROLL ||
         state_ == GS_PENDING_SYNTHETIC_CLICK);
  AppendTapDownGestureEvent(point, gestures);

  const GesturePoint* point1 = GetPointByPointId(0);
  const GesturePoint* point2 = GetPointByPointId(1);

  pinch_distance_current_ = point1->Distance(*point2);
  pinch_distance_start_ = pinch_distance_current_;
  AppendPinchGestureBegin(*point1, *point2, gestures);

  if (state_ == GS_PENDING_SYNTHETIC_CLICK) {
    gfx::Point center = point1->last_touch_position().Middle(
        point2->last_touch_position());
    AppendScrollGestureBegin(point, center, gestures);
  }

  return true;
}

bool GestureSequence::PinchUpdate(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_PINCH);

  const GesturePoint* point1 = GetPointByPointId(0);
  const GesturePoint* point2 = GetPointByPointId(1);

  float distance = point1->Distance(*point2);
  if (abs(distance - pinch_distance_current_) <
      GestureConfiguration::min_pinch_update_distance_in_pixels()) {
    // The fingers didn't move towards each other, or away from each other,
    // enough to constitute a pinch. But perhaps they moved enough in the same
    // direction to do a two-finger scroll.
    if (!point1->DidScroll(event,
        GestureConfiguration::min_distance_for_pinch_scroll_in_pixels()) ||
        !point2->DidScroll(event,
        GestureConfiguration::min_distance_for_pinch_scroll_in_pixels()))
      return false;

    gfx::Point center = point1->last_touch_position().Middle(
                        point2->last_touch_position());
    AppendScrollGestureUpdate(point, center, gestures);
  } else {
    AppendPinchGestureUpdate(*point1, *point2,
        distance / pinch_distance_current_, gestures);
    pinch_distance_current_ = distance;
  }
  return true;
}

bool GestureSequence::PinchEnd(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_PINCH);

  GesturePoint* point1 = GetPointByPointId(0);
  GesturePoint* point2 = GetPointByPointId(1);

  float distance = point1->Distance(*point2);
  AppendPinchGestureEnd(*point1, *point2,
      distance / pinch_distance_start_, gestures);

  point1->ResetVelocity();
  point2->ResetVelocity();

  pinch_distance_start_ = 0;
  pinch_distance_current_ = 0;
  return true;
}

bool GestureSequence::ThreeFingerSwipeUpdate(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  DCHECK(state_ == GS_THREE_FINGER_SWIPE);

  GesturePoint* point1 = GetPointByPointId(0);
  GesturePoint* point2 = GetPointByPointId(1);
  GesturePoint* point3 = GetPointByPointId(2);

  int min_velocity = GestureConfiguration::min_swipe_speed();

  float vx1 = point1->XVelocity();
  float vx2 = point2->XVelocity();
  float vx3 = point3->XVelocity();
  float vy1 = point1->YVelocity();
  float vy2 = point2->YVelocity();
  float vy3 = point3->YVelocity();

  float vx = (vx1 + vx2 + vx3) / 3;
  float vy = (vy1 + vy2 + vy3) / 3;

  // Not moving fast enough
  if (fabs(vx) < min_velocity && fabs(vy) < min_velocity)
    return false;

  // Diagonal swipe, no event fired
  // TODO(tdresser|sadrul): consider adding diagonal swipes
  float ratio = fabs(vx) > fabs(vy) ? fabs(vx / vy) : fabs(vy / vx);
  if (ratio < GestureConfiguration::max_swipe_deviation_ratio())
    return false;

  if (fabs(vx) > fabs(vy)) {
    int sign = vx > 0 ? 1 : -1;
    // ensure all touches are moving in the same direction, and are
    // all moving faster than min_velocity.
    if (vx1 * sign < min_velocity ||
        vx2 * sign < min_velocity ||
        vx3 * sign < min_velocity)
        return false;
    AppendThreeFingerSwipeGestureEvent(*point1, *point2, *point3,
                                       sign, 0, gestures);
  } else {
    int sign = vy > 0 ? 1 : -1;
    // ensure all touches are moving in the same direction, and are
    // all moving faster than min_velocity.
    if (vy1 * sign < min_velocity ||
        vy2 * sign < min_velocity ||
        vy3 * sign < min_velocity)
      return false;
    AppendThreeFingerSwipeGestureEvent(*point1, *point2, *point3,
                                       0, sign, gestures);
  }
  three_finger_swipe_has_fired_ = true;
  return true;
}

}  // namespace ui
