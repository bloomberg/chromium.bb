// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/gestures/gesture_sequence.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/event.h"
#include "ui/base/events.h"

namespace aura {

namespace {

// Get equivalent TouchState from EventType |type|.
GestureSequence::TouchState TouchEventTypeToTouchState(ui::EventType type) {
  switch (type) {
    case ui::ET_TOUCH_RELEASED:
      return GestureSequence::TS_RELEASED;
    case ui::ET_TOUCH_PRESSED:
      return GestureSequence::TS_PRESSED;
    case ui::ET_TOUCH_MOVED:
      return GestureSequence::TS_MOVED;
    case ui::ET_TOUCH_STATIONARY:
      return GestureSequence::TS_STATIONARY;
    case ui::ET_TOUCH_CANCELLED:
      return GestureSequence::TS_CANCELLED;
    default:
      VLOG(1) << "Unknown Touch Event type";
  }
  return GestureSequence::TS_UNKNOWN;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// GestureSequence Public:

GestureSequence::GestureSequence()
    : state_(GS_NO_GESTURE),
      flags_(0),
      point_count_(0) {
}

GestureSequence::~GestureSequence() {
}

GestureSequence::Gestures* GestureSequence::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::TouchStatus status) {
  if (status != ui::TOUCH_STATUS_UNKNOWN)
    return NULL;  // The event was consumed by a touch sequence.

  // Set a limit on the number of simultaneous touches in a gesture.
  if (event.touch_id() >= kMaxGesturePoints)
    return NULL;

  if (event.type() == ui::ET_TOUCH_PRESSED) {
    if (point_count_ == kMaxGesturePoints)
      return NULL;
    ++point_count_;
  }

  scoped_ptr<Gestures> gestures(new Gestures());
  GesturePoint& point = GesturePointForEvent(event);
  point.UpdateValues(event, state_);
  flags_ = event.flags();
  switch (Signature(state_, event.touch_id(), event.type(), false)) {
    case GST_NO_GESTURE_FIRST_PRESSED:
      TouchDown(event, point, gestures.get());
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_RELEASED:
      if (Click(event, point, gestures.get()))
        point.UpdateForTap();
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_MOVED:
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_STATIONARY:
      if (InClickOrScroll(event, point, gestures.get()))
        point.UpdateForScroll();
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_CANCELLED:
      NoGesture(event, point, gestures.get());
      break;
    case GST_SCROLL_FIRST_MOVED:
      if (InScroll(event, point, gestures.get()))
        point.UpdateForScroll();
      break;
    case GST_SCROLL_FIRST_RELEASED:
    case GST_SCROLL_FIRST_CANCELLED:
      ScrollEnd(event, point, gestures.get());
      break;
  }

  if (event.type() == ui::ET_TOUCH_RELEASED)
    --point_count_;

  return gestures.release();
}

void GestureSequence::Reset() {
  state_ = GS_NO_GESTURE;
  for (int i = 0; i < point_count_; ++i)
    points_[i].Reset();
}

////////////////////////////////////////////////////////////////////////////////
// GestureSequence Private:

unsigned int GestureSequence::Signature(GestureState gesture_state,
    unsigned int touch_id,
    ui::EventType type,
    bool touch_handled) {
  CHECK((touch_id & 0xfff) == touch_id);
  TouchState touch_state = TouchEventTypeToTouchState(type);
  return 1 + ((touch_state & 0x7) << 1 | (touch_handled ? 1 << 4 : 0) |
             ((touch_id & 0xfff) << 5) | (gesture_state << 17));
}

GesturePoint& GestureSequence::GesturePointForEvent(
    const TouchEvent& event) {
  return points_[event.touch_id()];
}

void GestureSequence::AppendTapDownGestureEvent(const GesturePoint& point,
                                                Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_TAP_DOWN,
      point.first_touch_position().x(),
      point.first_touch_position().y(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      0.f, 0.f)));
}

void GestureSequence::AppendClickGestureEvent(const GesturePoint& point,
                                              Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_TAP,
      point.first_touch_position().x(),
      point.first_touch_position().y(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      0.f, 0.f)));
}

void GestureSequence::AppendDoubleClickGestureEvent(const GesturePoint& point,
                                                    Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_DOUBLE_TAP,
      point.first_touch_position().x(),
      point.first_touch_position().y(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      0.f, 0.f)));
}

void GestureSequence::AppendScrollGestureBegin(const GesturePoint& point,
                                               Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_SCROLL_BEGIN,
      point.last_touch_position().x(),
      point.last_touch_position().y(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      0.f, 0.f)));
}

void GestureSequence::AppendScrollGestureEnd(const GesturePoint& point,
                                             Gestures* gestures,
                                             float x_velocity,
                                             float y_velocity) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_SCROLL_END,
      point.last_touch_position().x(),
      point.last_touch_position().y(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      x_velocity, y_velocity)));
}

void GestureSequence:: AppendScrollGestureUpdate(const GesturePoint& point,
                                                 Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_SCROLL_UPDATE,
      point.last_touch_position().x(),
      point.last_touch_position().y(),
      flags_,
      base::Time::FromDoubleT(point.last_touch_time()),
      point.x_delta(), point.y_delta())));
}

bool GestureSequence::Click(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  if (point.IsInClickWindow(event)) {
    AppendClickGestureEvent(point, gestures);
    if (point.IsInDoubleClickWindow(event))
      AppendDoubleClickGestureEvent(point, gestures);
    return true;
  }
  return false;
}

bool GestureSequence::InClickOrScroll(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  if (point.IsInClickWindow(event)) {
    set_state(GS_PENDING_SYNTHETIC_CLICK);
    return false;
  }
  if (point.IsInScrollWindow(event)) {
    AppendScrollGestureBegin(point, gestures);
    AppendScrollGestureUpdate(point, gestures);
    set_state(GS_SCROLL);
    return true;
  }
  return false;
}

bool GestureSequence::InScroll(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  AppendScrollGestureUpdate(point, gestures);
  return true;
}

bool GestureSequence::NoGesture(const TouchEvent&,
    const GesturePoint& point, Gestures*) {
  Reset();
  return false;
}

bool GestureSequence::TouchDown(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  AppendTapDownGestureEvent(point, gestures);
  set_state(GS_PENDING_SYNTHETIC_CLICK);
  return false;
}

bool GestureSequence::ScrollEnd(const TouchEvent& event,
    const GesturePoint& point, Gestures* gestures) {
  if (point.IsInFlickWindow(event))
    AppendScrollGestureEnd(point, gestures, point.x_velocity(),
        point.y_velocity());
  else
    AppendScrollGestureEnd(point, gestures, 0.f, 0.f);
  set_state(GS_NO_GESTURE);
  Reset();
  return false;
}

}  // namespace aura
