// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_UI_MOTION_EVENT_H_
#define UI_EVENTS_GESTURE_DETECTION_UI_MOTION_EVENT_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"
#include "ui/events/gesture_detection/motion_event_generic.h"

namespace ui {

// Implementation of MotionEvent which takes a stream of ui::TouchEvents.
class EVENTS_EXPORT MotionEventAura : public MotionEventGeneric {
 public:
  MotionEventAura();
  ~MotionEventAura() override;

  // MotionEventGeneric:
  int GetSourceDeviceId(size_t pointer_index) const override;

  // Returns true iff the touch was valid.
  bool OnTouch(const TouchEvent& touch);

  // We can't cleanup removed touch points immediately upon receipt of a
  // TouchCancel or TouchRelease, as the MotionEvent needs to be able to report
  // information about those touch events. Once the MotionEvent has been
  // processed, we call CleanupRemovedTouchPoints to do the required
  // book-keeping.
  void CleanupRemovedTouchPoints(const TouchEvent& event);

 private:
  bool AddTouch(const TouchEvent& touch);
  void UpdateTouch(const TouchEvent& touch);
  void UpdateCachedAction(const TouchEvent& touch);
  int GetIndexFromId(int id) const;

  DISALLOW_COPY_AND_ASSIGN(MotionEventAura);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_UI_MOTION_EVENT_H_
