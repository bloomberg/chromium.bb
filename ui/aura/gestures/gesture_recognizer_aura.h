// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_GESTURES_GESTURE_RECOGNIZER_AURA_H_
#define UI_AURA_GESTURES_GESTURE_RECOGNIZER_AURA_H_
#pragma once

#include <map>
#include <queue>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/gestures/gesture_recognizer.h"
#include "ui/base/events.h"
#include "ui/gfx/point.h"

namespace aura {
class TouchEvent;
class GestureEvent;
class GestureSequence;

class AURA_EXPORT GestureRecognizerAura : public GestureRecognizer {
 public:
  GestureRecognizerAura();
  virtual ~GestureRecognizerAura();

  // Checks if this finger is already down, if so, returns the current target.
  // Otherwise, if the finger is within
  // GestureConfiguration::max_separation_for_gesture_touches_in_pixels
  // of another touch, returns the target of the closest touch.
  // If there is no nearby touch, return null.
  virtual Window* GetTargetForTouchEvent(TouchEvent* event) OVERRIDE;

  // Returns the target of the touches the gesture is composed of.
  virtual Window* GetTargetForGestureEvent(GestureEvent* event) OVERRIDE;

  virtual Window* GetTargetForLocation(const gfx::Point& location) OVERRIDE;

 protected:
  virtual GestureSequence* CreateSequence(RootWindow* root_window);
  virtual GestureSequence* GetGestureSequenceForWindow(Window* window);

 private:
  // Overridden from GestureRecognizer
  virtual Gestures* ProcessTouchEventForGesture(
      const TouchEvent& event,
      ui::TouchStatus status,
      Window* target) OVERRIDE;
  virtual void QueueTouchEventForGesture(Window* window,
                                         const TouchEvent& event) OVERRIDE;
  virtual Gestures* AdvanceTouchQueue(Window* window, bool processed) OVERRIDE;
  virtual void FlushTouchQueue(Window* window) OVERRIDE;

  typedef std::queue<TouchEvent*> TouchEventQueue;
  std::map<Window*, TouchEventQueue*> event_queue_;
  std::map<Window*, GestureSequence*> window_sequence_;

  // Both touch_id_target_ and touch_id_target_for_gestures_
  // map a touch-id to its target window.
  // touch_ids are removed from touch_id_target_ on ET_TOUCH_RELEASE
  // and ET_TOUCH_CANCEL. touch_id_target_for_gestures_ never has touch_ids
  // removed.
  std::map<int, Window*> touch_id_target_;
  std::map<int, Window*> touch_id_target_for_gestures_;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerAura);
};

}  // namespace aura

#endif  // UI_AURA_GESTURES_GESTURE_RECOGNIZER_AURA_H_
