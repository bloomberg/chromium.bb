// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_provider_aura.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/events/event.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

namespace ui {

GestureProviderAura::GestureProviderAura(GestureProviderAuraClient* client)
    : client_(client),
      filtered_gesture_provider_(
          GetGestureProviderConfig(GestureProviderConfigType::CURRENT_PLATFORM),
          this),
      handling_event_(false),
      last_unique_touch_event_id_(
          std::numeric_limits<unsigned long long>::max()) {
  filtered_gesture_provider_.SetDoubleTapSupportForPlatformEnabled(false);
}

GestureProviderAura::~GestureProviderAura() {}

bool GestureProviderAura::OnTouchEvent(TouchEvent* event) {
  if (!pointer_state_.OnTouch(*event))
    return false;

  last_unique_touch_event_id_ = event->unique_event_id();
  last_touch_event_latency_info_ = *event->latency();

  auto result = filtered_gesture_provider_.OnTouchEvent(pointer_state_);
  if (!result.succeeded)
    return false;

  event->set_may_cause_scrolling(result.did_generate_scroll);
  pointer_state_.CleanupRemovedTouchPoints(*event);
  return true;
}

void GestureProviderAura::OnAsyncTouchEventAck(bool event_consumed) {
  DCHECK(pending_gestures_.empty());
  DCHECK(!handling_event_);
  base::AutoReset<bool> handling_event(&handling_event_, true);
  filtered_gesture_provider_.OnAsyncTouchEventAck(event_consumed);
  last_touch_event_latency_info_.Clear();
}

void GestureProviderAura::OnSyncTouchEventAck(const uint64 unique_event_id,
                                              bool event_consumed) {
  DCHECK_EQ(last_unique_touch_event_id_, unique_event_id);
  DCHECK(pending_gestures_.empty());
  DCHECK(!handling_event_);
  base::AutoReset<bool> handling_event(&handling_event_, true);
  filtered_gesture_provider_.OnSyncTouchEventAck(event_consumed);
  last_touch_event_latency_info_.Clear();
}

void GestureProviderAura::OnGestureEvent(
    const GestureEventData& gesture) {
  GestureEventDetails details = gesture.details;
  details.set_oldest_touch_id(gesture.motion_event_id);

  if (gesture.type() == ET_GESTURE_TAP) {
    int tap_count = 1;
    if (previous_tap_ && IsConsideredDoubleTap(*previous_tap_, gesture))
      tap_count = 1 + (previous_tap_->details.tap_count() % 3);
    details.set_tap_count(tap_count);
    if (!previous_tap_)
      previous_tap_.reset(new GestureEventData(gesture));
    else
      *previous_tap_ = gesture;
    previous_tap_->details = details;
  } else if (gesture.type() == ET_GESTURE_TAP_CANCEL) {
    previous_tap_.reset();
  }

  scoped_ptr<ui::GestureEvent> event(
      new ui::GestureEvent(gesture.x,
                           gesture.y,
                           gesture.flags,
                           gesture.time - base::TimeTicks(),
                           details));

  ui::LatencyInfo* gesture_latency = event->latency();

  gesture_latency->CopyLatencyFrom(
      last_touch_event_latency_info_,
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT);
  gesture_latency->CopyLatencyFrom(
      last_touch_event_latency_info_,
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT);
  gesture_latency->CopyLatencyFrom(
      last_touch_event_latency_info_,
      ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT);

  if (!handling_event_) {
    // Dispatching event caused by timer.
    client_->OnGestureEvent(event.get());
  } else {
    // Memory managed by ScopedVector pending_gestures_.
    pending_gestures_.push_back(event.release());
  }
}

ScopedVector<GestureEvent>* GestureProviderAura::GetAndResetPendingGestures() {
  if (pending_gestures_.empty())
    return NULL;
  // Caller is responsible for deleting old_pending_gestures.
  ScopedVector<GestureEvent>* old_pending_gestures =
      new ScopedVector<GestureEvent>();
  old_pending_gestures->swap(pending_gestures_);
  return old_pending_gestures;
}

bool GestureProviderAura::IsConsideredDoubleTap(
    const GestureEventData& previous_tap,
    const GestureEventData& current_tap) const {
  if (current_tap.time - previous_tap.time >
      base::TimeDelta::FromMilliseconds(
          GestureConfiguration::GetInstance()
              ->max_time_between_double_click_in_ms())) {
    return false;
  }

  float double_tap_slop_square =
      GestureConfiguration::GetInstance()
          ->max_distance_between_taps_for_double_tap();
  double_tap_slop_square *= double_tap_slop_square;
  const float delta_x = previous_tap.x - current_tap.x;
  const float delta_y = previous_tap.y - current_tap.y;
  return (delta_x * delta_x + delta_y * delta_y < double_tap_slop_square);
}

}  // namespace content
