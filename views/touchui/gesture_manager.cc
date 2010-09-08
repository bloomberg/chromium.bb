// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/gesture_manager.h"
#ifndef NDEBUG
#include <iostream>
#endif

#include "base/logging.h"
#include "views/event.h"
#include "views/view.h"

namespace views {

GestureManager::~GestureManager() {
}

GestureManager* GestureManager::Get() {
  return Singleton<GestureManager>::get();
}

bool GestureManager::ProcessTouchEventForGesture(const TouchEvent& event,
                                                 View* source,
                                                 bool previouslyHandled) {
  // TODO(rjkroege): A realistic version of the GestureManager will
  // appear in a subsequent CL. This interim version permits verifying that the
  // event distirbution code works by turning all touch inputs into
  // mouse approximations.
  bool handled = false;
  if (event.GetType() == Event::ET_TOUCH_PRESSED) {
    DLOG(INFO) << "GestureManager::ProcessTouchEventForGesture: " <<
        "TouchPressed\n";
    MouseEvent mouse_event(Event::ET_MOUSE_PRESSED, event.x(), event.y(),
                           event.GetFlags());
    source->OnMousePressed(mouse_event);
    handled = true;
  } else if (event.GetType() == Event::ET_TOUCH_RELEASED) {
    DLOG(INFO) << "GestureManager::ProcessTouchEventForGesture: " <<
        "TouchReleased\n";
    MouseEvent mouse_event(Event::ET_MOUSE_RELEASED, event.x(), event.y(),
                           event.GetFlags());
    source->OnMouseReleased(mouse_event, false);
    handled = true;
  } else if (event.GetType() == Event::ET_TOUCH_MOVED) {
    DLOG(INFO) << "GestureManager::ProcessTouchEventForGesture: " <<
        "TouchMotion\n";
    MouseEvent mouse_event(Event::ET_MOUSE_DRAGGED, event.x(), event.y(),
                           event.GetFlags());
    source->OnMouseDragged(mouse_event);
    handled = true;
  } else {
    DLOG(INFO) << "GestureManager::ProcessTouchEventForGesture: " <<
        "unhandled event\n";
  }
  return handled;
}

GestureManager::GestureManager() {
}

}  // namespace views
