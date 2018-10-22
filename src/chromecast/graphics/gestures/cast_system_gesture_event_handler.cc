// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/cast_system_gesture_event_handler.h"

#include <deque>

#include "base/auto_reset.h"
#include "chromecast/base/chromecast_switches.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromecast {

CastSystemGestureEventHandler::CastSystemGestureEventHandler(
    CastSystemGestureDispatcher* dispatcher,
    aura::Window* root_window)
    : EventHandler(), dispatcher_(dispatcher), root_window_(root_window) {
  DCHECK(dispatcher);
  DCHECK(root_window);
  root_window->AddPreTargetHandler(this);
}

CastSystemGestureEventHandler::~CastSystemGestureEventHandler() {
  root_window_->RemovePreTargetHandler(this);
}

void CastSystemGestureEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    ProcessPressedEvent(event);
  }
}

void CastSystemGestureEventHandler::ProcessPressedEvent(
    ui::GestureEvent* event) {
  gfx::Point touch_location(event->location());
  // Let the subscriber know about the gesture begin.
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      dispatcher_->HandleTapDownGesture(touch_location);
      break;
    case ui::ET_GESTURE_TAP:
      dispatcher_->HandleTapGesture(touch_location);
      break;
    default:
      return;
  }
}

}  // namespace chromecast
