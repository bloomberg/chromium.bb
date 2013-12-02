// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_targeter.h"

#include "ui/events/event.h"
#include "ui/events/event_target.h"

namespace ui {

EventTargeter::~EventTargeter() {
}

EventTarget* EventTargeter::FindTargetForEvent(EventTarget* root,
                                               Event* event) {
  return NULL;
}

EventTarget* EventTargeter::FindTargetForLocatedEvent(EventTarget* root,
                                                      LocatedEvent* event) {
  return NULL;
}

bool EventTargeter::SubtreeShouldBeExploredForEvent(EventTarget* target,
                                                    const LocatedEvent& event) {
  return true;
}

}  // namespace ui
