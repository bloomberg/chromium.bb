// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_NULL_EVENT_TARGETER_H_
#define UI_EVENTS_NULL_EVENT_TARGETER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/events/event_targeter.h"
#include "ui/events/events_export.h"

namespace ui {

// NullEventTargeter can be installed on a root window to prevent it from
// dispatching events such as during shutdown.
class EVENTS_EXPORT NullEventTargeter : public EventTargeter {
 public:
  NullEventTargeter();
  ~NullEventTargeter() override;

  // EventTargeter:
  EventTarget* FindTargetForEvent(EventTarget* root, Event* event) override;
  EventTarget* FindNextBestTarget(EventTarget* previous_target,
                                  Event* event) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullEventTargeter);
};

}  // namespace ui

#endif  // UI_EVENTS_NULL_EVENT_TARGETER_H_
