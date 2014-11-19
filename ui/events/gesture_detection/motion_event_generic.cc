// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/motion_event_generic.h"

#include "base/logging.h"

namespace ui {

PointerProperties::PointerProperties()
    : PointerProperties(0, 0, 0) {
}

PointerProperties::PointerProperties(float x, float y, float touch_major)
    : id(0),
      tool_type(MotionEvent::TOOL_TYPE_UNKNOWN),
      x(x),
      y(y),
      raw_x(x),
      raw_y(y),
      pressure(0),
      touch_major(touch_major),
      touch_minor(0),
      orientation(0),
      source_device_id(0) {
}

PointerProperties::PointerProperties(const MotionEvent& event,
                                     size_t pointer_index)
    : id(event.GetPointerId(pointer_index)),
      tool_type(event.GetToolType(pointer_index)),
      x(event.GetX(pointer_index)),
      y(event.GetY(pointer_index)),
      raw_x(event.GetRawX(pointer_index)),
      raw_y(event.GetRawY(pointer_index)),
      pressure(event.GetPressure(pointer_index)),
      touch_major(event.GetTouchMajor(pointer_index)),
      touch_minor(event.GetTouchMinor(pointer_index)),
      orientation(event.GetOrientation(pointer_index)),
      source_device_id(0) {
}

MotionEventGeneric::MotionEventGeneric(Action action,
                                       base::TimeTicks event_time,
                                       const PointerProperties& pointer)
    : action_(action),
      event_time_(event_time),
      id_(0),
      action_index_(0),
      button_state_(0),
      flags_(0) {
  PushPointer(pointer);
}

MotionEventGeneric::MotionEventGeneric(const MotionEventGeneric& other)
    : action_(other.action_),
      event_time_(other.event_time_),
      id_(other.id_),
      action_index_(other.action_index_),
      button_state_(other.button_state_),
      flags_(other.flags_),
      pointers_(other.pointers_) {
  const size_t history_size = other.GetHistorySize();
  for (size_t h = 0; h < history_size; ++h)
    PushHistoricalEvent(other.historical_events_[h]->Clone());
}

MotionEventGeneric::~MotionEventGeneric() {
}

int MotionEventGeneric::GetId() const {
  return id_;
}

MotionEvent::Action MotionEventGeneric::GetAction() const {
  return action_;
}

int MotionEventGeneric::GetActionIndex() const {
  DCHECK(action_ == ACTION_POINTER_DOWN || action_ == ACTION_POINTER_UP);
  DCHECK_GE(action_index_, 0);
  DCHECK_LT(action_index_, static_cast<int>(pointers_->size()));
  return action_index_;
}

size_t MotionEventGeneric::GetPointerCount() const {
  return pointers_->size();
}

int MotionEventGeneric::GetPointerId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].id;
}

float MotionEventGeneric::GetX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].x;
}

float MotionEventGeneric::GetY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].y;
}

float MotionEventGeneric::GetRawX(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].raw_x;
}

float MotionEventGeneric::GetRawY(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].raw_y;
}

float MotionEventGeneric::GetTouchMajor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].touch_major;
}

float MotionEventGeneric::GetTouchMinor(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].touch_minor;
}

float MotionEventGeneric::GetOrientation(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].orientation;
}

float MotionEventGeneric::GetPressure(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].pressure;
}

MotionEvent::ToolType MotionEventGeneric::GetToolType(
    size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointers_->size());
  return pointers_[pointer_index].tool_type;
}

int MotionEventGeneric::GetButtonState() const {
  return button_state_;
}

int MotionEventGeneric::GetFlags() const {
  return flags_;
}

base::TimeTicks MotionEventGeneric::GetEventTime() const {
  return event_time_;
}

size_t MotionEventGeneric::GetHistorySize() const {
  return historical_events_.size();
}

base::TimeTicks MotionEventGeneric::GetHistoricalEventTime(
    size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetEventTime();
}

float MotionEventGeneric::GetHistoricalTouchMajor(
    size_t pointer_index,
    size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetTouchMajor(pointer_index);
}

float MotionEventGeneric::GetHistoricalX(size_t pointer_index,
                                         size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetX(pointer_index);
}

float MotionEventGeneric::GetHistoricalY(size_t pointer_index,
                                         size_t historical_index) const {
  DCHECK_LT(historical_index, historical_events_.size());
  return historical_events_[historical_index]->GetY(pointer_index);
}

// static
scoped_ptr<MotionEventGeneric> MotionEventGeneric::CloneEvent(
    const MotionEvent& event) {
  bool with_history = true;
  return make_scoped_ptr(new MotionEventGeneric(event, with_history));
}

// static
scoped_ptr<MotionEventGeneric> MotionEventGeneric::CancelEvent(
    const MotionEvent& event) {
  bool with_history = false;
  scoped_ptr<MotionEventGeneric> cancel_event(
      new MotionEventGeneric(event, with_history));
  cancel_event->set_action(ACTION_CANCEL);
  return cancel_event.Pass();
}

void MotionEventGeneric::PushPointer(const PointerProperties& pointer) {
  DCHECK_EQ(0U, GetHistorySize());
  pointers_->push_back(pointer);
}

void MotionEventGeneric::PushHistoricalEvent(scoped_ptr<MotionEvent> event) {
  DCHECK(event);
  DCHECK_EQ(event->GetAction(), ACTION_MOVE);
  DCHECK_EQ(event->GetPointerCount(), GetPointerCount());
  DCHECK_EQ(event->GetAction(), GetAction());
  DCHECK_LE(event->GetEventTime().ToInternalValue(),
            GetEventTime().ToInternalValue());
  historical_events_.push_back(event.release());
}

MotionEventGeneric::MotionEventGeneric()
    : action_(ACTION_CANCEL), id_(0), action_index_(0), button_state_(0) {
}

MotionEventGeneric::MotionEventGeneric(const MotionEvent& event,
                                       bool with_history)
    : action_(event.GetAction()),
      event_time_(event.GetEventTime()),
      id_(event.GetId()),
      action_index_(
          (action_ == ACTION_POINTER_UP || action_ == ACTION_POINTER_DOWN)
              ? event.GetActionIndex()
              : 0),
      button_state_(event.GetButtonState()),
      flags_(event.GetFlags()) {
  const size_t pointer_count = event.GetPointerCount();
  for (size_t i = 0; i < pointer_count; ++i)
    PushPointer(PointerProperties(event, i));

  if (!with_history)
    return;

  const size_t history_size = event.GetHistorySize();
  for (size_t h = 0; h < history_size; ++h) {
    scoped_ptr<MotionEventGeneric> historical_event(new MotionEventGeneric());
    historical_event->set_action(ACTION_MOVE);
    historical_event->set_event_time(event.GetHistoricalEventTime(h));
    for (size_t i = 0; i < pointer_count; ++i) {
      historical_event->PushPointer(
          PointerProperties(event.GetHistoricalX(i, h),
                            event.GetHistoricalY(i, h),
                            event.GetHistoricalTouchMajor(i, h)));
    }
    PushHistoricalEvent(historical_event.Pass());
  }
}

MotionEventGeneric& MotionEventGeneric::operator=(
    const MotionEventGeneric& other) {
  action_ = other.action_;
  event_time_ = other.event_time_;
  id_ = other.id_;
  action_index_ = other.action_index_;
  button_state_ = other.button_state_;
  flags_ = other.flags_;
  pointers_ = other.pointers_;
  const size_t history_size = other.GetHistorySize();
  for (size_t h = 0; h < history_size; ++h)
    PushHistoricalEvent(other.historical_events_[h]->Clone());
  return *this;
}

void MotionEventGeneric::PopPointer() {
  DCHECK_GT(pointers_->size(), 0U);
  pointers_->pop_back();
}

}  // namespace ui
