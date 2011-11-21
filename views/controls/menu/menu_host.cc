// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host.h"

#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host_root_view.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/native_menu_host.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHost, public:

MenuHost::MenuHost(SubmenuView* submenu)
    : submenu_(submenu),
      destroying_(false),
      ignore_capture_lost_(false) {
}

MenuHost::~MenuHost() {
}

void MenuHost::InitMenuHost(Widget* parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture) {
  Widget::InitParams params(Widget::InitParams::TYPE_MENU);
  params.has_dropshadow = true;
  params.parent_widget = parent;
  params.bounds = bounds;
  Init(params);
  SetContentsView(contents_view);
  ShowMenuHost(do_capture);
}

bool MenuHost::IsMenuHostVisible() {
  return IsVisible();
}

void MenuHost::ShowMenuHost(bool do_capture) {
  // Doing a capture may make us get capture lost. Ignore it while we're in the
  // process of showing.
  ignore_capture_lost_ = true;
  Show();
#if defined(TOUCH_UI)
  Activate();
#endif
  if (do_capture)
    native_widget_private()->SetMouseCapture();
  ignore_capture_lost_ = false;
}

void MenuHost::HideMenuHost() {
  ignore_capture_lost_ = true;
  ReleaseMenuHostCapture();
  Hide();
  ignore_capture_lost_ = false;
}

void MenuHost::DestroyMenuHost() {
  HideMenuHost();
  destroying_ = true;
  static_cast<MenuHostRootView*>(GetRootView())->ClearSubmenu();
  Close();
}

void MenuHost::SetMenuHostBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void MenuHost::ReleaseMenuHostCapture() {
  if (native_widget_private()->HasMouseCapture())
    native_widget_private()->ReleaseMouseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// MenuHost, Widget overrides:

internal::RootView* MenuHost::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

bool MenuHost::ShouldReleaseCaptureOnMouseReleased() const {
  return false;
}

void MenuHost::OnMouseCaptureLost() {
  if (destroying_ || ignore_capture_lost_)
    return;
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (menu_controller && !menu_controller->drag_in_progress())
    menu_controller->CancelAll();
  Widget::OnMouseCaptureLost();
}

void MenuHost::OnNativeWidgetDestroyed() {
  if (!destroying_) {
    // We weren't explicitly told to destroy ourselves, which means the menu was
    // deleted out from under us (the window we're parented to was closed). Tell
    // the SubmenuView to drop references to us.
    submenu_->MenuHostDestroyed();
  }
  Widget::OnNativeWidgetDestroyed();
}

}  // namespace views
