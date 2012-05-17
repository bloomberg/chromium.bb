// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GESTURES_GESTURE_TYPES_H_
#define UI_BASE_GESTURES_GESTURE_TYPES_H_
#pragma once

#include "base/time.h"
#include "ui/base/events.h"

namespace ui {

// An abstract type to represent touch-events. The gesture-recognizer uses this
// interface to communicate with the touch-events.
class UI_EXPORT TouchEvent {
 public:
  virtual ~TouchEvent() {}

  virtual EventType GetEventType() const = 0;
  virtual gfx::Point GetLocation() const = 0;
  virtual int GetTouchId() const = 0;
  virtual int GetEventFlags() const = 0;
  virtual base::TimeDelta GetTimestamp() const = 0;
  virtual float RadiusX() const = 0;
  virtual float RadiusY() const = 0;
  virtual float RotationAngle() const = 0;
  virtual float Force() const = 0;

  // Returns a copy of this touch event. Used when queueing events for
  // asynchronous gesture recognition.
  virtual TouchEvent* Copy() const = 0;
};

// An abstract type to represent gesture-events.
class UI_EXPORT GestureEvent {
 public:
  virtual ~GestureEvent() {}

  // A gesture event can have multiple touches. This function should return the
  // lowest ID of the touches in this gesture.
  virtual int GetLowestTouchId() const = 0;
};

// An abstract type for consumers of gesture-events created by the
// gesture-recognizer.
class UI_EXPORT GestureConsumer {
 public:
  GestureConsumer()
      : ignores_events_(false) {
  }

  explicit GestureConsumer(bool ignores_events)
      : ignores_events_(ignores_events) {
  }

  virtual ~GestureConsumer() {}

  // TODO: this is a hack! GestureRecognizer should never expose the internal
  // marker object that implements this.
  bool ignores_events() { return ignores_events_; }

 private:
  const bool ignores_events_;
};

// GestureEventHelper creates implementation-specific gesture events and
// can dispatch them.
class UI_EXPORT GestureEventHelper {
 public:
  virtual ~GestureEventHelper() {
  }

  // |flags| is ui::EventFlags. The meaning of |param_first| and |param_second|
  // depends on the specific gesture type (|type|).
  virtual GestureEvent* CreateGestureEvent(EventType type,
                                           const gfx::Point& location,
                                           int flags,
                                           const base::Time time,
                                           float param_first,
                                           float param_second,
                                           unsigned int touch_id_bitfield) = 0;

  virtual TouchEvent* CreateTouchEvent(EventType type,
                                       const gfx::Point& location,
                                       int touch_id,
                                       base::TimeDelta time_stamp) = 0;

  virtual bool DispatchLongPressGestureEvent(GestureEvent* event) = 0;
  virtual bool DispatchCancelTouchEvent(TouchEvent* event) = 0;
};

}  // namespace ui

#endif  // UI_BASE_GESTURES_GESTURE_TYPES_H_
