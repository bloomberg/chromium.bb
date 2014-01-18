// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_TARGETER_H_
#define UI_EVENTS_EVENT_TARGETER_H_

#include "base/compiler_specific.h"
#include "ui/events/event.h"
#include "ui/events/events_export.h"

namespace ui {

class Event;
class EventTarget;
class LocatedEvent;

class EVENTS_EXPORT EventTargeter {
 public:
  virtual ~EventTargeter();

  // Returns the target |event| should be dispatched to. If there is no such
  // target, this should return NULL.
  virtual EventTarget* FindTargetForEvent(EventTarget* root,
                                          Event* event);

  // Same as FindTargetForEvent(), but used for positional events. The location
  // etc. of |event| are in |root|'s coordinate system. When finding the target
  // for the event, the targeter can mutate the |event| (e.g. chnage the
  // coordinate to be in the returned target's coordinate sustem) so that it can
  // be dispatched to the target without any farther modification.
  virtual EventTarget* FindTargetForLocatedEvent(EventTarget* root,
                                                 LocatedEvent* event);

  // Returns true of |target| or one of its descendants can be a target of
  // |event|. Note that the location etc. of |event| is in |target|'s parent's
  // coordinate system.
  virtual bool SubtreeShouldBeExploredForEvent(EventTarget* target,
                                               const LocatedEvent& event);
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_TARGETER_H_
