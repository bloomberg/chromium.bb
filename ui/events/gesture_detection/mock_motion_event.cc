// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/mock_motion_event.h"

using base::TimeTicks;

namespace ui {
namespace {
const float kTouchMajor = 10.f;
}  // namespace

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x,
                                 float y)
    : action(action), pointer_count(1), time(time) {
  points[0].SetPoint(x, y);
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1)
    : action(action), pointer_count(2), time(time) {
  points[0].SetPoint(x0, y0);
  points[1].SetPoint(x1, y1);
}

MockMotionEvent::MockMotionEvent(const MockMotionEvent& other)
    : action(other.action),
      pointer_count(other.pointer_count),
      time(other.time) {
  points[0] = other.points[0];
  points[1] = other.points[1];
}

MockMotionEvent::~MockMotionEvent() {}

MotionEvent::Action MockMotionEvent::GetAction() const { return action; }

int MockMotionEvent::GetActionIndex() const {
  return static_cast<int>(pointer_count) - 1;
}

size_t MockMotionEvent::GetPointerCount() const { return pointer_count; }

int MockMotionEvent::GetPointerId(size_t pointer_index) const {
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

}  // namespace ui
