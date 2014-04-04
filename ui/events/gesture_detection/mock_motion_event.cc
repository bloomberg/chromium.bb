// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/mock_motion_event.h"

#include "base/logging.h"

using base::TimeTicks;

namespace ui {
namespace {
const float kTouchMajor = 10.f;
}  // namespace

MockMotionEvent::MockMotionEvent()
    : action(ACTION_CANCEL), pointer_count(1), id(0) {}

MockMotionEvent::MockMotionEvent(Action action)
    : action(action), pointer_count(1), id(0) {}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x,
                                 float y)
    : action(action), pointer_count(1), time(time), id(0) {
  points[0].SetPoint(x, y);
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1)
    : action(action), pointer_count(2), time(time), id(0) {
  points[0].SetPoint(x0, y0);
  points[1].SetPoint(x1, y1);
}

MockMotionEvent::MockMotionEvent(const MockMotionEvent& other)
    : action(other.action),
      pointer_count(other.pointer_count),
      time(other.time),
      id(other.GetId()) {
  points[0] = other.points[0];
  points[1] = other.points[1];
}

MockMotionEvent::~MockMotionEvent() {}

MotionEvent::Action MockMotionEvent::GetAction() const { return action; }

int MockMotionEvent::GetActionIndex() const {
  return static_cast<int>(pointer_count) - 1;
}

size_t MockMotionEvent::GetPointerCount() const { return pointer_count; }

int MockMotionEvent::GetId() const {
  return id;
}

void MockMotionEvent::SetId(int new_id) {
  id = new_id;
}

int MockMotionEvent::GetPointerId(size_t pointer_index) const {
  DCHECK(pointer_index < pointer_count);
  return static_cast<int>(pointer_index);
}

float MockMotionEvent::GetX(size_t pointer_index) const {
  return points[pointer_index].x();
}

float MockMotionEvent::GetY(size_t pointer_index) const {
  return points[pointer_index].y();
}

float MockMotionEvent::GetTouchMajor(size_t pointer_index) const {
  return kTouchMajor;
}

float MockMotionEvent::GetPressure(size_t pointer_index) const {
  return 0;
}

TimeTicks MockMotionEvent::GetEventTime() const { return time; }

size_t MockMotionEvent::GetHistorySize() const { return 0; }

TimeTicks MockMotionEvent::GetHistoricalEventTime(
    size_t historical_index) const {
  return TimeTicks();
}

float MockMotionEvent::GetHistoricalTouchMajor(size_t pointer_index,
                                               size_t historical_index) const {
  return 0;
}

float MockMotionEvent::GetHistoricalX(size_t pointer_index,
                                      size_t historical_index) const {
  return 0;
}

float MockMotionEvent::GetHistoricalY(size_t pointer_index,
                                      size_t historical_index) const {
  return 0;
}

scoped_ptr<MotionEvent> MockMotionEvent::Clone() const {
  return scoped_ptr<MotionEvent>(new MockMotionEvent(*this));
}

scoped_ptr<MotionEvent> MockMotionEvent::Cancel() const {
  scoped_ptr<MockMotionEvent> cancel_event(new MockMotionEvent(*this));
  cancel_event->action = MotionEvent::ACTION_CANCEL;
  return cancel_event.PassAs<MotionEvent>();
}

void MockMotionEvent::PressPoint(float x, float y) {
  // Reset the pointer count if the previously released and/or cancelled pointer
  // was the last pointer in the event.
  if (pointer_count == 1 && (action == ACTION_UP || action == ACTION_CANCEL))
    pointer_count = 0;

  DCHECK_LT(pointer_count + 1, static_cast<size_t>(MAX_POINTERS));
  points[pointer_count++] = gfx::PointF(x, y);
  action = pointer_count > 1 ? ACTION_POINTER_DOWN : ACTION_DOWN;
}

void MockMotionEvent::MovePoint(size_t index, float x, float y) {
  DCHECK_LT(index, pointer_count);
  points[index] = gfx::PointF(x, y);
  action = ACTION_MOVE;
}

void MockMotionEvent::ReleasePoint() {
  DCHECK_GT(pointer_count, 0U);
  if (pointer_count > 1) {
    --pointer_count;
    action = ACTION_POINTER_UP;
  } else {
    action = ACTION_UP;
  }
}

void MockMotionEvent::CancelPoint() {
  DCHECK_GT(pointer_count, 0U);
  if (pointer_count > 1)
    --pointer_count;
  action = ACTION_CANCEL;
}

}  // namespace ui
