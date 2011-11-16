// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/gesture_manager.h"

#ifndef NDEBUG
#include <ostream>
#endif

#include "base/logging.h"
#include "views/events/event.h"
#include "views/view.h"
#include "views/views_delegate.h"
#include "views/widget/widget.h"

namespace views {

GestureManager::~GestureManager() {
}

GestureManager* GestureManager::GetInstance() {
  return Singleton<GestureManager>::get();
}

bool GestureManager::ProcessTouchEventForGesture(const TouchEvent& event,
                                                 View* source,
                                                 ui::TouchStatus status) {
  if (status != ui::TOUCH_STATUS_UNKNOWN)
    return false;  // The event was consumed by a touch sequence.

  // TODO(rjkroege): A realistic version of the GestureManager will
  // appear in a subsequent CL. This interim version permits verifying that the
  // event distribution code works by turning all touch inputs into
  // mouse approximations.

  // Conver the touch-event into a mouse-event. This mouse-event gets its
  // location information from the native-event, so it needs to convert the
  // coordinate to the target widget.
  MouseEvent mouseev(event);
  if (ViewsDelegate::views_delegate->GetDefaultParentView()) {
    // TODO(oshima): We may need to send the event back through
    // window manager to handle mouse capture correctly.
    Widget* desktop =
        ViewsDelegate::views_delegate->GetDefaultParentView()->GetWidget();
    Widget* source_widget = source->GetWidget();
    MouseEvent converted(
        mouseev, desktop->GetRootView(), source_widget->GetRootView());
    source_widget->OnMouseEvent(converted);
  } else {
    Widget* source_widget = source->GetWidget();
    Widget* top_widget = source_widget->GetTopLevelWidget();
    if (source_widget != top_widget && top_widget) {
      // This is necessary as TYPE_CHILD widget is still NativeWidgetGtk.
      // Fix this once TYPE_CHILD is switched to NativeWidgetViews.
      MouseEvent converted(mouseev,
                           top_widget->GetRootView(),
                           source_widget->GetRootView());
      source_widget->OnMouseEvent(mouseev);
    } else {
      source_widget->OnMouseEvent(mouseev);
    }
  }
  return true;
}

GestureManager::GestureManager() {
}

}  // namespace views
