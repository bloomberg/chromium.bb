// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_provider_aura.h"

#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/gesture_detection/gesture_config_helper.h"
#include "ui/events/gesture_detection/gesture_event_data.h"

namespace ui {

GestureProviderAura::GestureProviderAura(GestureProviderAuraClient* client)
    : client_(client),
      filtered_gesture_provider_(ui::DefaultGestureProviderConfig(), this) {
  filtered_gesture_provider_.SetDoubleTapSupportForPlatformEnabled(false);
}

GestureProviderAura::~GestureProviderAura() {}

bool GestureProviderAura::OnTouchEvent(const TouchEvent& event) {
  last_touch_event_flags_ = event.flags();
  bool pointer_id_is_active = false;
  for (size_t i = 0; i < pointer_state_.GetPointerCount(); ++i) {
    if (event.touch_id() != pointer_state_.GetPointerId(i))
      continue;
    pointer_id_is_active = true;
    break;
  }

  if (event.type() == ET_TOUCH_PRESSED && pointer_id_is_active) {
    // Ignore touch press events if we already believe the pointer is down.
    return false;
  } else if (event.type() != ET_TOUCH_PRESSED && !pointer_id_is_active) {
    // We could have an active touch stream transfered to us, resulting in touch
    // move or touch up events without associated touch down events. Ignore
    // them.
    return false;
  }

  pointer_state_.OnTouch(event);
  bool result = filtered_gesture_provider_.OnTouchEvent(pointer_state_);
  pointer_state_.CleanupRemovedTouchPoints(event);
  return result;
}

void GestureProviderAura::OnTouchEventAck(bool event_consumed) {
  filtered_gesture_provider_.OnTouchEventAck(event_consumed);
}

void GestureProviderAura::OnGestureEvent(
    const GestureEventData& gesture) {
  ui::GestureEvent event(gesture.type,
                         gesture.x,
                         gesture.y,
                         last_touch_event_flags_,
                         gesture.time - base::TimeTicks(),
                         gesture.details,
                         // ui::GestureEvent stores a bitfield indicating the
                         // ids of active touch points. This is currently only
                         // used when one finger is down, and will eventually
                         // be cleaned up. See crbug.com/366707.
                         1 << gesture.motion_event_id);
  client_->OnGestureEvent(&event);
}

}  // namespace content
