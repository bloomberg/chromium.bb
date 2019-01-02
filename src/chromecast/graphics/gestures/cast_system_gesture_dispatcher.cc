// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/cast_system_gesture_dispatcher.h"

#include "base/logging.h"

namespace chromecast {

CastSystemGestureDispatcher::CastSystemGestureDispatcher() {}

CastSystemGestureDispatcher::~CastSystemGestureDispatcher() {
  DCHECK(gesture_handlers_.empty());
}

void CastSystemGestureDispatcher::AddGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.insert(handler);
}

void CastSystemGestureDispatcher::RemoveGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.erase(handler);
}

CastGestureHandler::Priority CastSystemGestureDispatcher::GetPriority() {
  return Priority::MAX;
}

bool CastSystemGestureDispatcher::CanHandleSwipe(
    CastSideSwipeOrigin swipe_origin) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      return true;
    }
  }
  return false;
}

void CastSystemGestureDispatcher::HandleSideSwipe(
    CastSideSwipeEvent event,
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  CastGestureHandler* best_handler = nullptr;
  Priority highest_priority = Priority::NONE;
  // Iterate through all handlers. Pick the handler with the highest priority
  // that is capable of handling the swipe event and is not Priority::NONE.
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin) &&
        gesture_handler->GetPriority() > highest_priority) {
      best_handler = gesture_handler;
      highest_priority = gesture_handler->GetPriority();
    }
  }
  if (best_handler)
    best_handler->HandleSideSwipe(event, swipe_origin, touch_location);
}

void CastSystemGestureDispatcher::HandleTapDownGesture(
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    gesture_handler->HandleTapDownGesture(touch_location);
  }
}

void CastSystemGestureDispatcher::HandleTapGesture(
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    gesture_handler->HandleTapGesture(touch_location);
  }
}

}  // namespace chromecast
