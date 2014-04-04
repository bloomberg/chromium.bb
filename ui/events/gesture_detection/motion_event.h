// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_MOTION_EVENT_H_
#define UI_EVENTS_GESTURE_DETECTION_MOTION_EVENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"

namespace ui {

// Abstract class for a generic motion-related event, patterned after that
// subset of Android's MotionEvent API used in gesture detection.
class GESTURE_DETECTION_EXPORT MotionEvent {
 public:
  enum Action {
    ACTION_DOWN,
    ACTION_UP,
    ACTION_MOVE,
    ACTION_CANCEL,
    ACTION_POINTER_DOWN,
    ACTION_POINTER_UP,
  };

  // The implementer promises that |GetPointerId()| will never exceed this.
  enum { MAX_POINTER_ID = 31 };

  virtual ~MotionEvent() {}

  virtual int GetId() const = 0;
  virtual Action GetAction() const = 0;
  virtual int GetActionIndex() const = 0;
  virtual size_t GetPointerCount() const = 0;
  virtual int GetPointerId(size_t pointer_index) const = 0;
  virtual float GetX(size_t pointer_index) const = 0;
  virtual float GetY(size_t pointer_index) const = 0;
  virtual float GetTouchMajor(size_t pointer_index) const = 0;
  virtual float GetPressure(size_t pointer_index) const = 0;
  virtual base::TimeTicks GetEventTime() const = 0;

  virtual size_t GetHistorySize() const = 0;
  virtual base::TimeTicks GetHistoricalEventTime(
      size_t historical_index) const = 0;
  virtual float GetHistoricalTouchMajor(size_t pointer_index,
                                        size_t historical_index) const = 0;
  virtual float GetHistoricalX(size_t pointer_index,
                               size_t historical_index) const = 0;
  virtual float GetHistoricalY(size_t pointer_index,
                               size_t historical_index) const = 0;

  virtual scoped_ptr<MotionEvent> Clone() const = 0;
  virtual scoped_ptr<MotionEvent> Cancel() const = 0;

  // Utility accessor methods for convenience.
  float GetX() const { return GetX(0); }
  float GetY() const { return GetY(0); }
  float GetRawX() const { return GetX(); }
  float GetRawY() const { return GetY(); }
  float GetTouchMajor() const { return GetTouchMajor(0); }
  float GetPressure() const { return GetPressure(0); }
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_MOTION_EVENT_H_
