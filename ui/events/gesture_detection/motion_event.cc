// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/motion_event.h"

#include "base/logging.h"

namespace ui {

size_t MotionEvent::GetHistorySize() const {
  return 0;
}

base::TimeTicks MotionEvent::GetHistoricalEventTime(
    size_t historical_index) const {
  NOTIMPLEMENTED();
  return base::TimeTicks();
}

float MotionEvent::GetHistoricalTouchMajor(size_t pointer_index,
                                           size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

float MotionEvent::GetHistoricalX(size_t pointer_index,
                                  size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

float MotionEvent::GetHistoricalY(size_t pointer_index,
                                  size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

bool operator==(const MotionEvent& lhs, const MotionEvent& rhs) {
  if (lhs.GetId() != rhs.GetId() || lhs.GetAction() != rhs.GetAction() ||
      lhs.GetActionIndex() != rhs.GetActionIndex() ||
      lhs.GetPointerCount() != rhs.GetPointerCount() ||
      lhs.GetButtonState() != rhs.GetButtonState() ||
      lhs.GetEventTime() != rhs.GetEventTime())
    return false;

  for (size_t i = 0; i < lhs.GetPointerCount(); ++i) {
    if (lhs.GetX(i) != rhs.GetX(i) || lhs.GetY(i) != rhs.GetY(i) ||
        lhs.GetRawX(i) != rhs.GetRawX(i) || lhs.GetRawY(i) != rhs.GetRawY(i) ||
        lhs.GetTouchMajor(i) != rhs.GetTouchMajor(i) ||
        lhs.GetPressure(i) != rhs.GetPressure(i) ||
        lhs.GetToolType(i) != rhs.GetToolType(i))
      return false;
  }

  if (lhs.GetHistorySize() != rhs.GetHistorySize())
    return false;

  for (size_t h = 0; h < lhs.GetHistorySize(); ++h) {
    if (lhs.GetHistoricalEventTime(h) != rhs.GetHistoricalEventTime(h))
      return false;

    for (size_t i = 0; i < lhs.GetPointerCount(); ++i) {
      if (lhs.GetHistoricalX(i, h) != rhs.GetHistoricalX(i, h) ||
          lhs.GetHistoricalY(i, h) != rhs.GetHistoricalY(i, h) ||
          lhs.GetHistoricalTouchMajor(i, h) !=
              rhs.GetHistoricalTouchMajor(i, h))
        return false;
    }
  }

  return true;
}

bool operator!=(const MotionEvent& lhs, const MotionEvent& rhs) {
  return !(lhs == rhs);
}

}  // namespace ui
