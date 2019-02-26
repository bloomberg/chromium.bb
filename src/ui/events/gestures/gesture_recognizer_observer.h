// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_OBSERVER_H_
#define UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_types.h"

namespace ui {

class EVENTS_EXPORT GestureRecognizerObserver : public base::CheckedObserver {
 public:
  // Called when CancelActiveTouchesExcept() is called.
  virtual void OnActiveTouchesCanceledExcept(
      GestureConsumer* not_cancelled) = 0;

  // Called when TransferEventsTo() happened from |current_consumer| to
  // |new_consumer|.
  virtual void OnEventsTransferred(
      GestureConsumer* current_consumer,
      GestureConsumer* new_consumer,
      TransferTouchesBehavior transfer_touches_behavior) = 0;

  // Called when CancelActiveTouches() cancels touches on |consumer|. This is
  // not called from CancelActiveTouchesExcept() causes cancel on |consumer|.
  // Also this is not called when an invocation of CancelActiveTouches() doesn't
  // cancel anything actually.
  virtual void OnActiveTouchesCanceled(GestureConsumer* consumer) = 0;

 protected:
  ~GestureRecognizerObserver() override;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_OBSERVER_H_
