// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/motion_event_test_utils.h"

#include <sstream>

#include "base/logging.h"
#include "ui/events/gesture_detection/bitset_32.h"
#include "ui/events/gesture_detection/motion_event.h"

using base::TimeTicks;

namespace ui {
namespace test {
namespace {

PointerProperties CreatePointer() {
  PointerProperties pointer;
  pointer.touch_major = MockMotionEvent::TOUCH_MAJOR;
  return pointer;
}

PointerProperties CreatePointer(float x, float y, int id) {
  PointerProperties pointer(x, y, MockMotionEvent::TOUCH_MAJOR);
  pointer.id = id;
  return pointer;
}

}  // namespace

MockMotionEvent::MockMotionEvent()
    : MotionEventGeneric(ACTION_CANCEL, base::TimeTicks(), CreatePointer()) {
}

MockMotionEvent::MockMotionEvent(Action action)
    : MotionEventGeneric(action, base::TimeTicks(), CreatePointer()) {
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0)
    : MotionEventGeneric(action, time, CreatePointer(x0, y0, 0)) {
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1)
    : MotionEventGeneric(action, time, CreatePointer(x0, y0, 0)) {
  PushPointer(x1, y1);
  if (action == ACTION_POINTER_UP || action == ACTION_POINTER_DOWN)
    set_action_index(1);
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1,
                                 float x2,
                                 float y2)
    : MotionEventGeneric(action, time, CreatePointer(x0, y0, 0)) {
  PushPointer(x1, y1);
  PushPointer(x2, y2);
  if (action == ACTION_POINTER_UP || action == ACTION_POINTER_DOWN)
    set_action_index(2);
}

MockMotionEvent::MockMotionEvent(Action action,
                                 base::TimeTicks time,
                                 const std::vector<gfx::PointF>& positions) {
  set_action(action);
  set_event_time(time);
  if (action == ACTION_POINTER_UP || action == ACTION_POINTER_DOWN)
    set_action_index(static_cast<int>(positions.size()) - 1);
  for (size_t i = 0; i < positions.size(); ++i)
    PushPointer(positions[i].x(), positions[i].y());
}

MockMotionEvent::MockMotionEvent(const MockMotionEvent& other)
    : MotionEventGeneric(other) {
}

MockMotionEvent::~MockMotionEvent() {
}

void MockMotionEvent::PressPoint(float x, float y) {
  ResolvePointers();
  PushPointer(x, y);
  if (GetPointerCount() > 1) {
    set_action_index(static_cast<int>(GetPointerCount()) - 1);
    set_action(ACTION_POINTER_DOWN);
  } else {
    set_action(ACTION_DOWN);
  }
}

void MockMotionEvent::MovePoint(size_t index, float x, float y) {
  ResolvePointers();
  DCHECK_LT(index, GetPointerCount());
  PointerProperties& p = pointer(index);
  float dx = x - p.x;
  float dy = x - p.y;
  p.x = x;
  p.y = y;
  p.raw_x += dx;
  p.raw_y += dy;
  set_action(ACTION_MOVE);
}

void MockMotionEvent::ReleasePoint() {
  ResolvePointers();
  DCHECK_GT(GetPointerCount(), 0U);
  if (GetPointerCount() > 1) {
    set_action_index(static_cast<int>(GetPointerCount()) - 1);
    set_action(ACTION_POINTER_UP);
  } else {
    set_action(ACTION_UP);
  }
}

void MockMotionEvent::CancelPoint() {
  ResolvePointers();
  DCHECK_GT(GetPointerCount(), 0U);
  set_action(ACTION_CANCEL);
}

void MockMotionEvent::SetTouchMajor(float new_touch_major) {
  for (size_t i = 0; i < GetPointerCount(); ++i)
    pointer(i).touch_major = new_touch_major;
}

void MockMotionEvent::SetRawOffset(float raw_offset_x, float raw_offset_y) {
  for (size_t i = 0; i < GetPointerCount(); ++i) {
    pointer(i).raw_x = pointer(i).x + raw_offset_x;
    pointer(i).raw_y = pointer(i).y + raw_offset_y;
  }
}

void MockMotionEvent::SetToolType(size_t pointer_index, ToolType tool_type) {
  DCHECK_LT(pointer_index, GetPointerCount());
  pointer(pointer_index).tool_type = tool_type;
}

void MockMotionEvent::PushPointer(float x, float y) {
  MotionEventGeneric::PushPointer(
      CreatePointer(x, y, static_cast<int>(GetPointerCount())));
}

void MockMotionEvent::ResolvePointers() {
  set_action_index(-1);
  switch (GetAction()) {
    case ACTION_UP:
    case ACTION_POINTER_UP:
    case ACTION_CANCEL:
      PopPointer();
      return;
    default:
      break;
  }
}

std::string ToString(const MotionEvent& event) {
  std::stringstream ss;
  ss << "MotionEvent {"
     << "\n ID: " << event.GetId() << "\n Action: " << event.GetAction();
  if (event.GetAction() == MotionEvent::ACTION_POINTER_DOWN ||
      event.GetAction() == MotionEvent::ACTION_POINTER_UP)
    ss << "\n ActionIndex: " << event.GetActionIndex();
  ss << "\n Flags: " << event.GetFlags()
     << "\n ButtonState: " << event.GetButtonState() << "\n Pointers: [";
  const size_t pointer_count = event.GetPointerCount();
  const size_t history_size = event.GetHistorySize();

  BitSet32 pointer_ids;
  for (size_t i = 0; i < pointer_count; ++i) {
    pointer_ids.mark_bit(event.GetPointerId(i));

    // Print the pointers sorted by id.
    while (!pointer_ids.is_empty()) {
      int pi = event.FindPointerIndexOfId(pointer_ids.first_marked_bit());
      DCHECK_GE(pi, 0);
      pointer_ids.clear_first_marked_bit();
      ss << "{"
         << "\n  Pos: (" << event.GetX(pi) << ", " << event.GetY(pi) << ")"
         << "\n  RawPos: (" << event.GetX(pi) << ", " << event.GetY(pi) << ")"
         << "\n  Size: (" << event.GetTouchMajor(pi) << ", "
         << event.GetTouchMinor(pi) << ")"
         << "\n  Orientation: " << event.GetOrientation(pi)
         << "\n  Pressure: " << event.GetOrientation(pi)
         << "\n  Tool: " << event.GetToolType(pi);
      if (history_size) {
        ss << "\n  History: [";
        for (size_t h = 0; h < history_size; ++h) {
          ss << "\n   { " << event.GetHistoricalX(pi, h) << ", "
             << event.GetHistoricalY(pi, h) << ", "
             << event.GetHistoricalTouchMajor(pi, h) << ", "
             << event.GetHistoricalEventTime(pi).ToInternalValue() << " }";
          if (h + 1 < history_size)
            ss << ",";
        }
        ss << "\n  ]";
      }
      ss << "\n }";
      if (i + 1 < pointer_count)
        ss << ", ";
    }
    ss << "]\n}";
  }

  return ss.str();
}

}  // namespace test
}  // namespace ui
