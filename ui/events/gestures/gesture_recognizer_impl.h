// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_H_
#define UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_H_

#include <map>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/gestures/gesture_sequence.h"
#include "ui/gfx/point.h"

namespace ui {
class GestureConsumer;
class GestureEvent;
class GestureEventHelper;
class GestureSequence;
class TouchEvent;

class EVENTS_EXPORT GestureRecognizerImpl : public GestureRecognizer,
                                            public GestureSequenceDelegate {
 public:
  typedef std::map<int, GestureConsumer*> TouchIdToConsumerMap;

  GestureRecognizerImpl();
  virtual ~GestureRecognizerImpl();

  std::vector<GestureEventHelper*>& helpers() { return helpers_; }

  // Overridden from GestureRecognizer
  virtual GestureConsumer* GetTouchLockedTarget(
      const TouchEvent& event) OVERRIDE;
  virtual GestureConsumer* GetTargetForGestureEvent(
      const GestureEvent& event) OVERRIDE;
  virtual GestureConsumer* GetTargetForLocation(
      const gfx::PointF& location, int source_device_id) OVERRIDE;
  virtual void TransferEventsTo(GestureConsumer* current_consumer,
                                GestureConsumer* new_consumer) OVERRIDE;
  virtual bool GetLastTouchPointForTarget(GestureConsumer* consumer,
                                          gfx::PointF* point) OVERRIDE;
  virtual bool CancelActiveTouches(GestureConsumer* consumer) OVERRIDE;

 protected:
  virtual GestureSequence* CreateSequence(GestureSequenceDelegate* delegate);
  virtual GestureSequence* GetGestureSequenceForConsumer(GestureConsumer* c);

 private:
  // Sets up the target consumer for gestures based on the touch-event.
  void SetupTargets(const TouchEvent& event, GestureConsumer* consumer);
  void CancelTouches(std::vector<std::pair<int, GestureConsumer*> >* touches);

  // Overridden from GestureRecognizer
  virtual Gestures* ProcessTouchEventForGesture(
      const TouchEvent& event,
      ui::EventResult result,
      GestureConsumer* target) OVERRIDE;
  virtual bool CleanupStateForConsumer(GestureConsumer* consumer)
      OVERRIDE;
  virtual void AddGestureEventHelper(GestureEventHelper* helper) OVERRIDE;
  virtual void RemoveGestureEventHelper(GestureEventHelper* helper) OVERRIDE;

  // Overridden from ui::GestureSequenceDelegate.
  virtual void DispatchPostponedGestureEvent(GestureEvent* event) OVERRIDE;

  // Convenience method to find the GestureEventHelper that can dispatch events
  // to a specific |consumer|.
  GestureEventHelper* FindDispatchHelperForConsumer(GestureConsumer* consumer);

  std::map<GestureConsumer*, GestureSequence*> consumer_sequence_;

  // Both |touch_id_target_| and |touch_id_target_for_gestures_| map a touch-id
  // to its target window.  touch-ids are removed from |touch_id_target_| on
  // ET_TOUCH_RELEASE and ET_TOUCH_CANCEL. |touch_id_target_for_gestures_| are
  // removed in ConsumerDestroyed().
  TouchIdToConsumerMap touch_id_target_;
  TouchIdToConsumerMap touch_id_target_for_gestures_;

  std::vector<GestureEventHelper*> helpers_;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizerImpl);
};

// Provided only for testing:
EVENTS_EXPORT void SetGestureRecognizerForTesting(
    GestureRecognizer* gesture_recognizer);

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_RECOGNIZER_IMPL_H_
