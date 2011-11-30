// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TOUCHUI_GESTURE_MANAGER_H_
#define UI_VIEWS_TOUCHUI_GESTURE_MANAGER_H_
#pragma once

#include "base/memory/singleton.h"
#include "ui/views/view.h"

namespace ui {
enum TouchStatus;
}

namespace views {
class TouchEvent;

// A GestureManager singleton detects gestures occurring in the
// incoming feed of touch events across all of the RootViews in
// the system. In response to a given touch event, the GestureManager
// updates its internal state and optionally dispatches synthetic
// events to the invoking view.
//
class VIEWS_EXPORT GestureManager {
 public:
  virtual ~GestureManager();

  static GestureManager* GetInstance();

  // Invoked for each touch event that could contribute to the current gesture.
  // Takes the event and the View that originated it and which will also
  // be the target of any generated synthetic event. Finally, status
  // specifies if a touch sequence is in progress or not, so that the
  // GestureManager state can correctly reflect events that are handled
  // already.
  // Returns true if the event resulted in firing a synthetic event.
  virtual bool ProcessTouchEventForGesture(const TouchEvent& event,
                                           View* source,
                                           ui::TouchStatus status);

  // TODO(rjkroege): Write the remainder of this class.
  // It will appear in a subsequent CL.

 protected:
  GestureManager();

 private:
  friend struct DefaultSingletonTraits<GestureManager>;

  DISALLOW_COPY_AND_ASSIGN(GestureManager);
};

}  // namespace views

#endif  // UI_VIEWS_TOUCHUI_GESTURE_MANAGER_H_
