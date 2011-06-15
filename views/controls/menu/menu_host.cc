// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host.h"

#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host_root_view.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/native_menu_host.h"
#include "views/controls/menu/submenu_view.h"
#include "views/widget/native_widget.h"
#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHost, public:

MenuHost::MenuHost(SubmenuView* submenu)
    : ALLOW_THIS_IN_INITIALIZER_LIST(native_menu_host_(
          NativeMenuHost::CreateNativeMenuHost(this))),
      submenu_(submenu),
      destroying_(false) {
}

MenuHost::~MenuHost() {
}

void MenuHost::InitMenuHost(gfx::NativeWindow parent,
                            const gfx::Rect& bounds,
                            View* contents_view,
                            bool do_capture) {
  Widget::InitParams params(Widget::InitParams::TYPE_MENU);
  params.has_dropshadow = true;
#if defined(OS_WIN)
  params.parent = parent;
#elif defined(TOOLKIT_USES_GTK)
  params.parent = GTK_WIDGET(parent);
#endif
  params.bounds = bounds;
  params.native_widget = native_menu_host_->AsNativeWidget();
  Init(params);
  SetContentsView(contents_view);
  ShowMenuHost(do_capture);
}

bool MenuHost::IsMenuHostVisible() {
  return IsVisible();
}

void MenuHost::ShowMenuHost(bool do_capture) {
  Show();
  if (do_capture)
    native_menu_host_->StartCapturing();
}

void MenuHost::HideMenuHost() {
  ReleaseMenuHostCapture();
  Hide();
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
  if (native_widget()->HasMouseCapture())
    native_widget()->ReleaseMouseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// MenuHost, Widget overrides:

internal::RootView* MenuHost::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

bool MenuHost::ShouldReleaseCaptureOnMouseReleased() const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// MenuHost, internal::NativeMenuHostDelegate implementation:

void MenuHost::OnNativeMenuHostDestroy() {
  if (!destroying_) {
    // We weren't explicitly told to destroy ourselves, which means the menu was
    // deleted out from under us (the window we're parented to was closed). Tell
    // the SubmenuView to drop references to us.
    submenu_->MenuHostDestroyed();
  }
}

void MenuHost::OnNativeMenuHostCancelCapture() {
  if (destroying_)
    return;
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (menu_controller && !menu_controller->drag_in_progress())
    menu_controller->CancelAll();
}

internal::NativeWidgetDelegate* MenuHost::AsNativeWidgetDelegate() {
  return this;
}

}  // namespace views
