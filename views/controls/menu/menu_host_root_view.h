// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_MENU_MENU_HOST_ROOT_VIEW_H_
#define VIEWS_CONTROLS_MENU_MENU_HOST_ROOT_VIEW_H_
#pragma once

#include "views/widget/root_view.h"

namespace views {

class MenuController;
class SubmenuView;

// MenuHostRootView is the RootView of the window showing the menu.
// SubmenuView's scroll view is added as a child of MenuHostRootView.
// MenuHostRootView forwards relevant events to the MenuController.
//
// As all the menu items are owned by the root menu item, care must be taken
// such that when MenuHostRootView is deleted it doesn't delete the menu items.
class MenuHostRootView : public internal::RootView {
 public:
  MenuHostRootView(Widget* widget, SubmenuView* submenu);

  void ClearSubmenu() { submenu_ = NULL; }

  // Overridden from View:
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const MouseWheelEvent& event) OVERRIDE;

 private:
  // Returns the MenuController for this MenuHostRootView.
  MenuController* GetMenuController();

  // The SubmenuView we contain.
  SubmenuView* submenu_;

  // Whether mouse dragged/released should be forwarded to the MenuController.
  bool forward_drag_to_menu_controller_;

  DISALLOW_COPY_AND_ASSIGN(MenuHostRootView);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_MENU_MENU_HOST_ROOT_VIEW_H_
