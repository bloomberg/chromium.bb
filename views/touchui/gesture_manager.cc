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

  // TODO(sad): Clean this up.
  // This is currently only called where |source| is a RootView. Now, RootView
  // expects the mouse-events in the widget's coordinate system, and not in the
  // RV's coordinate system. But |event| is in the RV's coordinate system. So it
  // is necessary to construct the synthetic event in the widget's coordinate
  // system.
  gfx::Point location = event.location();
  View::ConvertPointToWidget(source, &location);

  if (event.type() == ui::ET_TOUCH_PRESSED) {
    DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: TouchPressed";
    MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, location.x(), location.y(),
                           event.flags());
    source->OnMousePressed(mouse_event);
    return true;
  }

  if (event.type() == ui::ET_TOUCH_RELEASED) {
    DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: TouchReleased";
    MouseEvent mouse_event(ui::ET_MOUSE_RELEASED, location.x(), location.y(),
                           event.flags());
    source->OnMouseReleased(mouse_event, false);
    return true;
  }

  if (event.type() == ui::ET_TOUCH_MOVED) {
    DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: TouchMotion";
    MouseEvent mouse_event(ui::ET_MOUSE_DRAGGED, location.x(), location.y(),
                           event.flags());
    source->OnMouseDragged(mouse_event);
    return true;
  }

  DVLOG(1) << "GestureManager::ProcessTouchEventForGesture: unhandled event";
  return false;
}

GestureManager::GestureManager() {
}

}  // namespace views
