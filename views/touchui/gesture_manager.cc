// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/gesture_manager.h"
#ifndef NDEBUG
#include <iostream>
#endif

#include "base/logging.h"
#include "views/events/event.h"
#include "views/view.h"

namespace views {

GestureManager::~GestureManager() {
}

GestureManager* GestureManager::GetInstance() {
  return Singleton<GestureManager>::get();
}

bool GestureManager::ProcessTouchEventForGesture(const TouchEvent& event,
                                                 View* source,
                                                 View::TouchStatus status) {
  if (status != View::TOUCH_STATUS_UNKNOWN)
    return false;  // The event was consumed by a touch sequence.

  // TODO(rjkroege): A realistic version of the GestureManager will
  // appear in a subsequent CL. This interim version permits verifying that the
  // event distribution code works by turning all touch inputs into
  // mouse approximations.
  if (event.GetType() == ui::ET_TOUCH_PRESSED) {
    DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: TouchPressed";
    MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, event.x(), event.y(),
                           event.GetFlags());
    source->OnMousePressed(mouse_event);
    return true;
  }

  if (event.GetType() == ui::ET_TOUCH_RELEASED) {
    DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: TouchReleased";
    MouseEvent mouse_event(ui::ET_MOUSE_RELEASED, event.x(), event.y(),
                           event.GetFlags());
    source->OnMouseReleased(mouse_event, false);
    return true;
  }

  if (event.GetType() == ui::ET_TOUCH_MOVED) {
    DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: TouchMotion";
    MouseEvent mouse_event(ui::ET_MOUSE_DRAGGED, event.x(), event.y(),
                           event.GetFlags());
    source->OnMouseDragged(mouse_event);
    return true;
  }

  DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: unhandled event";
  return false;
}

GestureManager::GestureManager() {
}

}  // namespace views
