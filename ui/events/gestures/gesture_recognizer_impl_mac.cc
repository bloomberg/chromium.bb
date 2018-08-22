// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_recognizer_impl_mac.h"

namespace ui {

GestureRecognizerImplMac::GestureRecognizerImplMac() = default;
GestureRecognizerImplMac::~GestureRecognizerImplMac() = default;

bool GestureRecognizerImplMac::ProcessTouchEventPreDispatch(
    TouchEvent* event,
    GestureConsumer* consumer) {
  return false;
}

GestureRecognizer::Gestures GestureRecognizerImplMac::AckTouchEvent(
    uint32_t unique_event_id,
    ui::EventResult result,
    bool is_source_touch_event_set_non_blocking,
    GestureConsumer* consumer) {
  return {};
}

bool GestureRecognizerImplMac::CleanupStateForConsumer(
    GestureConsumer* consumer) {
  return false;
}

GestureConsumer* GestureRecognizerImplMac::GetTouchLockedTarget(
    const TouchEvent& event) {
  return NULL;
}

GestureConsumer* GestureRecognizerImplMac::GetTargetForLocation(
    const gfx::PointF& location,
    int source_device_id) {
  return NULL;
}

void GestureRecognizerImplMac::CancelActiveTouchesExcept(
    GestureConsumer* not_cancelled) {}

void GestureRecognizerImplMac::TransferEventsTo(
    GestureConsumer* current_consumer,
    GestureConsumer* new_consumer,
    ShouldCancelTouches should_cancel_touches) {}

bool GestureRecognizerImplMac::GetLastTouchPointForTarget(
    GestureConsumer* consumer,
    gfx::PointF* point) {
  return false;
}

bool GestureRecognizerImplMac::CancelActiveTouches(GestureConsumer* consumer) {
  return false;
}

void GestureRecognizerImplMac::AddGestureEventHelper(
    GestureEventHelper* helper) {}

void GestureRecognizerImplMac::RemoveGestureEventHelper(
    GestureEventHelper* helper) {}

}  // namespace ui
