// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_root_view.h"

#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

MenuHostRootView::MenuHostRootView(Widget* widget,
                                   SubmenuView* submenu)
    : RootView(widget),
      submenu_(submenu),
      forward_drag_to_menu_controller_(true),
      suspend_events_(false) {
}

bool MenuHostRootView::OnMousePressed(const MouseEvent& event) {
  if (suspend_events_)
    return true;

  forward_drag_to_menu_controller_ =
      ((event.x() < 0 || event.y() < 0 || event.x() >= width() ||
        event.y() >= height()) ||
       !RootView::OnMousePressed(event));
  if (forward_drag_to_menu_controller_ && GetMenuController())
    GetMenuController()->OnMousePressed(submenu_, event);
  return true;
}

bool MenuHostRootView::OnMouseDragged(const MouseEvent& event) {
  if (suspend_events_)
    return true;

  if (forward_drag_to_menu_controller_ && GetMenuController()) {
    GetMenuController()->OnMouseDragged(submenu_, event);
    return true;
  }
  return RootView::OnMouseDragged(event);
}

void MenuHostRootView::OnMouseReleased(const MouseEvent& event,
                                       bool canceled) {
  if (suspend_events_)
    return;

  RootView::OnMouseReleased(event, canceled);
  if (forward_drag_to_menu_controller_ && GetMenuController()) {
    forward_drag_to_menu_controller_ = false;
    if (canceled) {
      GetMenuController()->Cancel(MenuController::EXIT_ALL);
    } else {
      GetMenuController()->OnMouseReleased(submenu_, event);
    }
  }
}

void MenuHostRootView::OnMouseMoved(const MouseEvent& event) {
  if (suspend_events_)
    return;

  RootView::OnMouseMoved(event);
  if (GetMenuController())
    GetMenuController()->OnMouseMoved(submenu_, event);
}

bool MenuHostRootView::OnMouseWheel(const MouseWheelEvent& event) {
  // RootView::OnMouseWheel forwards to the focused view. We don't have a
  // focused view, so we need to override this then forward to the menu.
  return submenu_->OnMouseWheel(event);
}

void MenuHostRootView::OnMouseExited(const MouseEvent& event) {
  if (suspend_events_)
    return;

  RootView::OnMouseExited(event);
}

MenuController* MenuHostRootView::GetMenuController() {
  return submenu_->GetMenuItem()->GetMenuController();
}

}  // namespace views
