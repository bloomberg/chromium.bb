// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GESTURES_GESTURE_RECOGNIZER_H_
#define UI_BASE_GESTURES_GESTURE_RECOGNIZER_H_

#include <vector>

#include "base/memory/scoped_vector.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/base/ui_export.h"

namespace ui {
// A GestureRecognizer is an abstract base class for conversion of touch events
// into gestures.
class UI_EXPORT GestureRecognizer {
 public:
  static GestureRecognizer* Create(GestureEventHelper* helper);

  // List of GestureEvent*.
  typedef ScopedVector<GestureEvent> Gestures;

  virtual ~GestureRecognizer() {}

  // Invoked for each touch event that could contribute to the current gesture.
  // Returns list of  zero or more GestureEvents identified after processing
  // TouchEvent.
  // Caller would be responsible for freeing up Gestures.
  virtual Gestures* ProcessTouchEventForGesture(const TouchEvent& event,
                                                ui::TouchStatus status,
                                                GestureConsumer* consumer) = 0;

  // Touch-events can be queued to be played back at a later time. The queues
  // are identified by the target window.
  virtual void QueueTouchEventForGesture(GestureConsumer* consumer,
                                         const TouchEvent& event) = 0;

  // Process the touch-event in the queue for the window. Returns a list of
  // zero or more GestureEvents identified after processing the queueud
  // TouchEvent. Caller is responsible for freeing up Gestures.
  virtual Gestures* AdvanceTouchQueue(GestureConsumer* consumer,
                                      bool processed) = 0;

  // Flushes the touch event queue (or removes the queue) for the window.
  virtual void FlushTouchQueue(GestureConsumer* consumer) = 0;

  // Return the window which should handle this TouchEvent, in the case where
  // the touch is already associated with a target.
  // Otherwise, returns null.
  virtual GestureConsumer* GetTouchLockedTarget(TouchEvent* event) = 0;

  // Return the window which should handle this GestureEvent.
  virtual GestureConsumer* GetTargetForGestureEvent(GestureEvent* event) = 0;

  // If there is an active touch within
  // GestureConfiguration::max_separation_for_gesture_touches_in_pixels,
  // of |location|, returns the target of the nearest active touch.
  virtual GestureConsumer* GetTargetForLocation(const gfx::Point& location) = 0;

  // Makes |new_consumer| the target for events previously targetting
  // |current_consumer|. All other targets are cancelled. If |new_consumer| is
  // NULL, all targets are cancelled.
  virtual void TransferEventsTo(GestureConsumer* current_consumer,
                                GestureConsumer* new_consumer) = 0;

  // If a gesture is underway for |consumer| |point| is set to the last touch
  // point and true is returned. If no touch events have been processed for
  // |consumer| false is returned and |point| is untouched.
  virtual bool GetLastTouchPointForTarget(GestureConsumer* consumer,
                                          gfx::Point* point) = 0;
};

}  // namespace ui

#endif  // UI_BASE_GESTURES_GESTURE_RECOGNIZER_H_
