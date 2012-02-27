// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_GESTURES_GESTURE_RECOGNIZER_H_
#define UI_AURA_GESTURES_GESTURE_RECOGNIZER_H_
#pragma once

#include <vector>

#include "base/memory/linked_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/base/events.h"

namespace aura {
class GestureEvent;
class RootWindow;
class TouchEvent;
class Window;

// A GestureRecognizer is an abstract base class for conversion of touch events
// into gestures.
class AURA_EXPORT GestureRecognizer {
 public:
  static GestureRecognizer* Create(RootWindow* root_window);

  // List of GestureEvent*.
  typedef std::vector<linked_ptr<GestureEvent> > Gestures;

  virtual ~GestureRecognizer() {}

  // Invoked for each touch event that could contribute to the current gesture.
  // Returns list of  zero or more GestureEvents identified after processing
  // TouchEvent.
  // Caller would be responsible for freeing up Gestures.
  virtual Gestures* ProcessTouchEventForGesture(const TouchEvent& event,
                                                ui::TouchStatus status) = 0;

  // Touch-events can be queued to be played back at a later time. The queues
  // are identified by the target window.
  virtual void QueueTouchEventForGesture(Window* window,
                                         const TouchEvent& event) = 0;

  // Process the touch-event in the queue for the window. Returns a list of
  // zero or more GestureEvents identified after processing the queueud
  // TouchEvent. Caller is responsible for freeing up Gestures.
  virtual Gestures* AdvanceTouchQueue(Window* window,
                                      bool processed) = 0;

  // Flushes the touch event queue (or removes the queue) for the window.
  virtual void FlushTouchQueue(Window* window) = 0;
};

}  // namespace aura

#endif  // UI_AURA_GESTURES_GESTURE_RECOGNIZER_H_
