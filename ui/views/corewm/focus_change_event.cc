// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/focus_change_event.h"

#include "base/time.h"
#include "ui/base/events/event_utils.h"

namespace views {
namespace corewm {

// static
int FocusChangeEvent::focus_changing_event_type_ = ui::ET_UNKNOWN;
int FocusChangeEvent::focus_changed_event_type_ = ui::ET_UNKNOWN;
int FocusChangeEvent::activation_changing_event_type_ = ui::ET_UNKNOWN;
int FocusChangeEvent::activation_changed_event_type_ = ui::ET_UNKNOWN;

FocusChangeEvent::FocusChangeEvent(int type)
    : Event(static_cast<ui::EventType>(type),
            base::TimeDelta::FromMilliseconds(base::Time::Now().ToDoubleT()),
            0),
      last_focus_(NULL) {
  DCHECK_NE(type, ui::ET_UNKNOWN) <<
      "Call RegisterEventTypes() before instantiating this class";
}

FocusChangeEvent::~FocusChangeEvent() {
}

// static
void FocusChangeEvent::RegisterEventTypes() {
  static bool registered = false;
  if (!registered) {
    registered = true;
    focus_changing_event_type_ = ui::RegisterCustomEventType();
    focus_changed_event_type_ = ui::RegisterCustomEventType();
    activation_changing_event_type_ = ui::RegisterCustomEventType();
    activation_changed_event_type_ = ui::RegisterCustomEventType();
  }
}

}  // namespace corewm
}  // namespace views
