// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/touchui/gesture_manager.h"
#ifndef NDEBUG
#include <iostream>
#endif

#include "base/logging.h"
#include "views/events/event.h"
#include "views/view.h"
#include "views/widget/widget.h"

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

  Event::FromNativeEvent2 from_native;
  MouseEvent mouseev(event, from_native);
  source->GetWidget()->OnMouseEvent(mouseev);
  return true;
}

GestureManager::GestureManager() {
}

}  // namespace views
