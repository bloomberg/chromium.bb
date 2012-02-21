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

 protected:
  virtual GestureSequence* CreateSequence();
  GestureSequence* gesture_sequence() { return default_sequence_.get(); }

 private:
  // Overridden from GestureRecognizer
  virtual Gestures* ProcessTouchEventForGesture(
      const TouchEvent& event,
      ui::TouchStatus status) OVERRIDE;
  virtual void QueueTouchEventForGesture(Window* window,
                                         const TouchEvent& event) OVERRIDE;
  virtual Gestures* AdvanceTouchQueue(Window* window, bool processed) OVERRIDE;
  virtual void FlushTouchQueue(Window* window) OVERRIDE;

  scoped_ptr<GestureSequence> default_sequence_;

  typedef std::queue<TouchEvent*> TouchEventQueue;
  std::map<Window*, TouchEventQueue*> event_queue_;
  std::map<Window*, GestureSequence*> window_sequence_;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerAura);
};

}  // namespace aura

#endif  // UI_AURA_GESTURES_GESTURE_RECOGNIZER_AURA_H_
