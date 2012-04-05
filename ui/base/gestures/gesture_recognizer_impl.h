// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GESTURES_GESTURE_RECOGNIZER_IMPL_H_
#define UI_BASE_GESTURES_GESTURE_RECOGNIZER_IMPL_H_
#pragma once

#include <map>
#include <queue>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/events.h"
#include "ui/base/gestures/gesture_recognizer.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/point.h"

namespace ui {
class TouchEvent;
class GestureEvent;
class GestureSequence;
class GestureConsumer;
class GestureEventHelper;

class UI_EXPORT GestureRecognizerAura : public GestureRecognizer {
 public:
  explicit GestureRecognizerAura(GestureEventHelper* helper);
  virtual ~GestureRecognizerAura();

  // Checks if this finger is already down, if so, returns the current target.
  // Otherwise, if the finger is within
  // GestureConfiguration::max_separation_for_gesture_touches_in_pixels
  // of another touch, returns the target of the closest touch.
  // If there is no nearby touch, return null.
  virtual GestureConsumer* GetTargetForTouchEvent(TouchEvent* event) OVERRIDE;

  // Returns the target of the touches the gesture is composed of.
  virtual GestureConsumer* GetTargetForGestureEvent(
      GestureEvent* event) OVERRIDE;

  virtual GestureConsumer* GetTargetForLocation(
      const gfx::Point& location) OVERRIDE;

 protected:
  virtual GestureSequence* CreateSequence(GestureEventHelper* helper);
  virtual GestureSequence* GetGestureSequenceForConsumer(GestureConsumer* c);

 private:
  // Overridden from GestureRecognizer
  virtual Gestures* ProcessTouchEventForGesture(
      const TouchEvent& event,
      ui::TouchStatus status,
      GestureConsumer* target) OVERRIDE;
  virtual void QueueTouchEventForGesture(GestureConsumer* consumer,
                                         const TouchEvent& event) OVERRIDE;
  virtual Gestures* AdvanceTouchQueue(GestureConsumer* consumer,
                                      bool processed) OVERRIDE;
  virtual void FlushTouchQueue(GestureConsumer* consumer) OVERRIDE;

  typedef std::queue<TouchEvent*> TouchEventQueue;
  std::map<GestureConsumer*, TouchEventQueue*> event_queue_;
  std::map<GestureConsumer*, GestureSequence*> consumer_sequence_;

  // Both touch_id_target_ and touch_id_target_for_gestures_
  // map a touch-id to its target window.
  // touch_ids are removed from touch_id_target_ on ET_TOUCH_RELEASE
  // and ET_TOUCH_CANCEL. touch_id_target_for_gestures_ never has touch_ids
  // removed.
  std::map<int, GestureConsumer*> touch_id_target_;
  std::map<int, GestureConsumer*> touch_id_target_for_gestures_;
  GestureEventHelper* helper_;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerAura);
};

}  // namespace ui

#endif  // UI_BASE_GESTURES_GESTURE_RECOGNIZER_IMPL_H_
