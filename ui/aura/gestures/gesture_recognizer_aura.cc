// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/gestures/gesture_recognizer_aura.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "ui/aura/event.h"
#include "ui/base/events.h"

namespace {
// TODO(girard): Make these configurable in sync with this CL
//               http://crbug.com/100773
const double kMaximumTouchDownDurationInSecondsForClick = 0.8;
const double kMinimumTouchDownDurationInSecondsForClick = 0.01;
const double kMaximumSecondsBetweenDoubleClick = 0.7;
const int kMaximumTouchMoveInPixelsForClick = 20;
const float kMinFlickSpeedSquared = 550.f * 550.f;

// This is used to pop a std::queue when returning from a function.
class ScopedPop {
 public:
  ScopedPop(std::queue<aura::TouchEvent*>* queue) : queue_(queue) {
  }

  ~ScopedPop() {
    delete queue_->front();
    queue_->pop();
  }

 private:
  std::queue<aura::TouchEvent*>* queue_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPop);
};

}  // namespace

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
    : first_touch_time_(0.0),
      state_(GestureSequence::GS_NO_GESTURE),
      last_touch_time_(0.0),
      last_click_time_(0.0),
      x_velocity_(0.0),
      y_velocity_(0.0),
      flags_(0) {
}

GestureSequence::~GestureSequence() {
}

GestureSequence::Gestures* GestureSequence::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::TouchStatus status) {
  if (status != ui::TOUCH_STATUS_UNKNOWN)
    return NULL;  // The event was consumed by a touch sequence.

  scoped_ptr<Gestures> gestures(new Gestures());
  UpdateValues(event);
  switch (Signature(state_, event.touch_id(), event.type(), false)) {
    case GST_NO_GESTURE_FIRST_PRESSED:
      TouchDown(event, gestures.get());
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_RELEASED:
      Click(event, gestures.get());
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_MOVED:
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_STATIONARY:
      InClickOrScroll(event, gestures.get());
      break;
    case GST_PENDING_SYNTHETIC_CLICK_FIRST_CANCELLED:
      NoGesture(event, gestures.get());
      break;
    case GST_SCROLL_FIRST_MOVED:
      InScroll(event, gestures.get());
      break;
    case GST_SCROLL_FIRST_RELEASED:
    case GST_SCROLL_FIRST_CANCELLED:
      ScrollEnd(event, gestures.get());
      break;
  }
  return gestures.release();
}

void GestureSequence::Reset() {
  first_touch_time_ = 0.0;
  state_ = GestureSequence::GS_NO_GESTURE;
  last_touch_time_ = 0.0;
  last_touch_position_.SetPoint(0, 0);
  x_velocity_ = 0.0;
  y_velocity_ = 0.0;
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

bool GestureSequence::IsInClickTimeWindow() {
  double duration(last_touch_time_ - first_touch_time_);
  return duration >= kMinimumTouchDownDurationInSecondsForClick &&
         duration < kMaximumTouchDownDurationInSecondsForClick;
}

bool GestureSequence::IsInSecondClickTimeWindow() {
  double duration(last_touch_time_ - last_click_time_);
  return duration < kMaximumSecondsBetweenDoubleClick;
}

bool GestureSequence::IsInsideManhattanSquare(const TouchEvent& event) {
  int manhattanDistance = abs(event.x() - first_touch_position_.x()) +
                          abs(event.y() - first_touch_position_.y());
  return manhattanDistance < kMaximumTouchMoveInPixelsForClick;
}

bool GestureSequence::IsSecondClickInsideManhattanSquare(
    const TouchEvent& event) {
  int manhattanDistance = abs(event.x() - last_click_position_.x()) +
                          abs(event.y() - last_click_position_.y());
  return manhattanDistance < kMaximumTouchMoveInPixelsForClick;
}

bool GestureSequence::IsOverMinFlickSpeed() {
  return (x_velocity_ * x_velocity_ + y_velocity_ * y_velocity_) >
          kMinFlickSpeedSquared;
}

void GestureSequence::AppendTapDownGestureEvent(const TouchEvent& event,
                                                Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_TAP_DOWN,
      first_touch_position_.x(),
      first_touch_position_.y(),
      event.flags(),
      base::Time::FromDoubleT(last_touch_time_),
      0.f, 0.f)));
}

void GestureSequence::AppendClickGestureEvent(const TouchEvent& event,
                                              Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_TAP,
      first_touch_position_.x(),
      first_touch_position_.y(),
      event.flags(),
      base::Time::FromDoubleT(last_touch_time_),
      0.f, 0.f)));
}

void GestureSequence::AppendDoubleClickGestureEvent(const TouchEvent& event,
                                                    Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_DOUBLE_TAP,
      first_touch_position_.x(),
      first_touch_position_.y(),
      event.flags(),
      base::Time::FromDoubleT(last_touch_time_),
      0.f, 0.f)));
}

void GestureSequence::AppendScrollGestureBegin(const TouchEvent& event,
                                               Gestures* gestures) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_SCROLL_BEGIN,
      event.x(),
      event.y(),
      event.flags(),
      base::Time::FromDoubleT(last_touch_time_),
      0.f, 0.f)));
}

void GestureSequence::AppendScrollGestureEnd(const TouchEvent& event,
                                             Gestures* gestures,
                                             float x_velocity,
                                             float y_velocity) {
  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_SCROLL_END,
      event.x(),
      event.y(),
      event.flags(),
      base::Time::FromDoubleT(last_touch_time_),
      x_velocity, y_velocity)));
}

void GestureSequence:: AppendScrollGestureUpdate(const TouchEvent& event,
                                                 Gestures* gestures) {
  float delta_x(event.x() - first_touch_position_.x());
  float delta_y(event.y() - first_touch_position_.y());

  gestures->push_back(linked_ptr<GestureEvent>(new GestureEvent(
      ui::ET_GESTURE_SCROLL_UPDATE,
      event.x(),
      event.y(),
      event.flags(),
      base::Time::FromDoubleT(last_touch_time_),
      delta_x, delta_y)));

  first_touch_position_ = event.location();
}

void GestureSequence::UpdateValues(const TouchEvent& event) {
  if (state_ != GS_NO_GESTURE && event.type() == ui::ET_TOUCH_MOVED) {
    double interval(event.time_stamp().InSecondsF() - last_touch_time_);
    x_velocity_ = (event.x() - last_touch_position_.x()) / interval;
    y_velocity_ = (event.y() - last_touch_position_.y()) / interval;
  }
  last_touch_time_ = event.time_stamp().InSecondsF();
  last_touch_position_ = event.location();
  if (state_ == GS_NO_GESTURE) {
    first_touch_time_ = last_touch_time_;
    first_touch_position_ = event.location();
    x_velocity_ = 0.0;
    y_velocity_ = 0.0;
  }
}

bool GestureSequence::Click(const TouchEvent& event, Gestures* gestures) {
  bool gesture_added = false;
  if (IsInClickTimeWindow() && IsInsideManhattanSquare(event)) {
    gesture_added = true;
    AppendClickGestureEvent(event, gestures);
    if (IsInSecondClickTimeWindow() &&
        IsSecondClickInsideManhattanSquare(event))
      AppendDoubleClickGestureEvent(event, gestures);
    last_click_time_ = last_touch_time_;
    last_click_position_ = last_touch_position_;
  }
  Reset();
  return gesture_added;
}

bool GestureSequence::InClickOrScroll(const TouchEvent& event,
                                      Gestures* gestures) {
  if (IsInClickTimeWindow() && IsInsideManhattanSquare(event)) {
    SetState(GS_PENDING_SYNTHETIC_CLICK);
    return false;
  }
  if (event.type() == ui::ET_TOUCH_MOVED && !IsInsideManhattanSquare(event)) {
    AppendScrollGestureBegin(event, gestures);
    AppendScrollGestureUpdate(event, gestures);
    SetState(GS_SCROLL);
    return true;
  }
  return false;
}

bool GestureSequence::InScroll(const TouchEvent& event, Gestures* gestures) {
  AppendScrollGestureUpdate(event, gestures);
  return true;
}

bool GestureSequence::NoGesture(const TouchEvent&, Gestures*) {
  Reset();
  return false;
}

bool GestureSequence::TouchDown(const TouchEvent& event, Gestures* gestures) {
  AppendTapDownGestureEvent(event, gestures);
  SetState(GS_PENDING_SYNTHETIC_CLICK);
  return false;
}

bool GestureSequence::ScrollEnd(const TouchEvent& event, Gestures* gestures) {
  if (IsOverMinFlickSpeed() && event.type() != ui::ET_TOUCH_CANCELLED)
    AppendScrollGestureEnd(event, gestures, x_velocity_, y_velocity_);
  else
    AppendScrollGestureEnd(event, gestures, 0.f, 0.f);
  SetState(GS_NO_GESTURE);
  Reset();
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// GestureRecognizerAura, public:

GestureRecognizerAura::GestureRecognizerAura()
    : default_sequence_(new GestureSequence()) {
}

GestureRecognizerAura::~GestureRecognizerAura() {
}

GestureSequence::Gestures* GestureRecognizerAura::ProcessTouchEventForGesture(
    const TouchEvent& event,
    ui::TouchStatus status) {
  return default_sequence_->ProcessTouchEventForGesture(event, status);
}

void GestureRecognizerAura::QueueTouchEventForGesture(Window* window,
                                                      const TouchEvent& event) {
  if (!event_queue_[window])
    event_queue_[window] = new std::queue<TouchEvent*>();
  event_queue_[window]->push(event.Copy());
}

GestureSequence::Gestures* GestureRecognizerAura::AdvanceTouchQueue(
    Window* window,
    bool processed) {
  if (!event_queue_[window]) {
    LOG(ERROR) << "Trying to advance an empty gesture queue for " << window;
    return NULL;
  }

  ScopedPop pop(event_queue_[window]);
  TouchEvent* event = event_queue_[window]->front();

  GestureSequence* sequence = window_sequence_[window];
  if (!sequence) {
    sequence = new GestureSequence();
    window_sequence_[window] = sequence;
  }

  return sequence->ProcessTouchEventForGesture(*event,
      processed ? ui::TOUCH_STATUS_CONTINUE : ui::TOUCH_STATUS_UNKNOWN);
}

void GestureRecognizerAura::FlushTouchQueue(Window* window) {
  if (window_sequence_[window]) {
    delete window_sequence_[window];
    window_sequence_.erase(window);
  }

  if (event_queue_[window]) {
    delete event_queue_[window];
    event_queue_.erase(window);
  }
}

// GestureRecognizer, static
GestureRecognizer* GestureRecognizer::Create() {
  return new GestureRecognizerAura();
}

}  // namespace aura
