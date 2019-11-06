// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_TARGETER_H_
#define UI_EVENTS_EVENT_TARGETER_H_

#include "base/compiler_specific.h"
#include "ui/events/events_export.h"

namespace ui {

class Event;
class EventTarget;

class EVENTS_EXPORT EventTargeter {
 public:
  virtual ~EventTargeter() {}

  // Returns the target |event| should be dispatched to. If there is no such
  // target, return NULL. If |event| is a located event, the location of |event|
  // is in the coordinate space of |root|. Furthermore, the targeter can mutate
  // the event (e.g., by changing the location of the event to be in the
  // returned target's coordinate space) so that it can be dispatched to the
  // target without any further modification.
  virtual EventTarget* FindTargetForEvent(EventTarget* root, Event* event) = 0;

  // Returns the next best target for |event| as compared to |previous_target|.
  // |event| is in the local coordinate space of |previous_target|.
  // Also mutates |event| so that it can be dispatched to the returned target
  // (e.g., by changing |event|'s location to be in the returned target's
  // coordinate space).
  virtual EventTarget* FindNextBestTarget(EventTarget* previous_target,
                                          Event* event) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_TARGETER_H_
