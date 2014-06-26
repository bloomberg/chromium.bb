// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/mock_motion_event.h"

#include "base/logging.h"

using base::TimeTicks;

namespace ui {
namespace test {

MockMotionEvent::MockMotionEvent()
    : action(ACTION_CANCEL), pointer_count(1), touch_major(TOUCH_MAJOR), id(0),
      button_state(0) {
}

MockMotionEvent::MockMotionEvent(Action action)
    : action(action), pointer_count(1), touch_major(TOUCH_MAJOR), id(0),
      button_state(0) {
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x,
                                 float y)
    : action(action),
      pointer_count(1),
      time(time),
      touch_major(TOUCH_MAJOR),
      id(0),
      button_state(0) {
  points[0].SetPoint(x, y);
  tool_types[0] = TOOL_TYPE_UNKNOWN;
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1)
    : action(action),
      pointer_count(2),
      time(time),
      touch_major(TOUCH_MAJOR),
      id(0),
      button_state(0) {
  points[0].SetPoint(x0, y0);
  tool_types[0] = TOOL_TYPE_UNKNOWN;
  points[1].SetPoint(x1, y1);
  tool_types[1] = TOOL_TYPE_UNKNOWN;
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1,
                                 float x2,
                                 float y2)
    : action(action),
      pointer_count(3),
      time(time),
      touch_major(TOUCH_MAJOR),
      id(0),
      button_state(0) {
  points[0].SetPoint(x0, y0);
  tool_types[0] = TOOL_TYPE_UNKNOWN;
  points[1].SetPoint(x1, y1);
  tool_types[1] = TOOL_TYPE_UNKNOWN;
  points[2].SetPoint(x2, y2);
  tool_types[2] = TOOL_TYPE_UNKNOWN;
}

MockMotionEvent::MockMotionEvent(const MockMotionEvent& other)
    : action(other.action),
      pointer_count(other.pointer_count),
      time(other.time),
      touch_major(other.touch_major),
      id(other.GetId()),
      button_state(other.GetButtonState()) {
  for (size_t i = 0; i < pointer_count; ++i) {
    points[i] = other.points[i];
    tool_types[i] = other.tool_types[i];
  }
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

float MockMotionEvent::GetRawX(size_t pointer_index) const {
  return GetX(pointer_index) + raw_offset.x();
}

float MockMotionEvent::GetRawY(size_t pointer_index) const {
  return GetY(pointer_index) + raw_offset.y();
}

float MockMotionEvent::GetTouchMajor(size_t pointer_index) const {
  return touch_major;
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

MotionEvent::ToolType MockMotionEvent::GetToolType(size_t pointer_index) const {
  DCHECK_LT(pointer_index, pointer_count);
  return tool_types[pointer_index];
}

int MockMotionEvent::GetButtonState() const {
  return button_state;
}

scoped_ptr<MotionEvent> MockMotionEvent::Clone() const {
  return scoped_ptr<MotionEvent>(new MockMotionEvent(*this));
}

scoped_ptr<MotionEvent> MockMotionEvent::Cancel() const {
  scoped_ptr<MockMotionEvent> cancel_event(new MockMotionEvent(*this));
  cancel_event->action = MotionEvent::ACTION_CANCEL;
  return cancel_event.PassAs<MotionEvent>();
}

void MockMotionEvent::SetId(int new_id) {
  id = new_id;
}

void MockMotionEvent::SetTime(base::TimeTicks new_time) {
  time = new_time;
}

void MockMotionEvent::PressPoint(float x, float y) {
  // Reset the pointer count if the previously released and/or cancelled pointer
  // was the last pointer in the event.
  if (pointer_count == 1 && (action == ACTION_UP || action == ACTION_CANCEL))
    pointer_count = 0;

  DCHECK_LT(pointer_count, static_cast<size_t>(MAX_POINTERS));
  points[pointer_count++] = gfx::PointF(x, y);
  tool_types[pointer_count] = TOOL_TYPE_UNKNOWN;
  action = pointer_count > 1 ? ACTION_POINTER_DOWN : ACTION_DOWN;
}

void MockMotionEvent::MovePoint(size_t index, float x, float y) {
  DCHECK_LT(index, pointer_count);
  points[index] = gfx::PointF(x, y);
  tool_types[index] = TOOL_TYPE_UNKNOWN;
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

void MockMotionEvent::SetTouchMajor(float new_touch_major) {
  touch_major = new_touch_major;
}

void MockMotionEvent::SetRawOffset(float raw_offset_x, float raw_offset_y) {
  raw_offset.set_x(raw_offset_x);
  raw_offset.set_y(raw_offset_y);
}

void MockMotionEvent::SetToolType(size_t pointer_index, ToolType tool_type) {
  DCHECK_LT(pointer_index, pointer_count);
  tool_types[pointer_index] = tool_type;
}

void MockMotionEvent::SetButtonState(int new_button_state) {
  button_state = new_button_state;
}

}  // namespace test
}  // namespace ui
