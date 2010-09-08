// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_TOUCHUI_GESTURE_MANAGER_H_
#define VIEWS_TOUCHUI_GESTURE_MANAGER_H_
#pragma once

#include "base/singleton.h"

namespace views {
class View;
class TouchEvent;


// A GestureManager singleton detects gestures occurring in the
// incoming feed of touch events across all of the RootViews in
// the system. In response to a given touch event, the GestureManager
// updates its internal state and optionally dispatches synthetic
// events to the invoking view.
//
class GestureManager {
 public:
  virtual ~GestureManager();

  static GestureManager* Get();

  // Invoked for each touch event that could contribute to the current gesture.
  // Takes the event and the View that originated it and which will also
  // be the target of any generated synthetic event. Finally, handled
  // specifies if the event was actually handled explicitly by a view so that
  // GestureManager state can correctly reflect events that are handled
  // already.
  // Returns true if the event resulted in firing a synthetic event.
  virtual bool ProcessTouchEventForGesture(const TouchEvent& event,
                                           View* source,
                                           bool previouslyHandled);

  // TODO(rjkroege): Write the remainder of this class.
  // It will appear in a subsequent CL.

 protected:
  GestureManager();

 private:
  friend struct DefaultSingletonTraits<GestureManager>;

  DISALLOW_COPY_AND_ASSIGN(GestureManager);
};


}  // namespace views

#endif  // VIEWS_TOUCHUI_GESTURE_MANAGER_H_
