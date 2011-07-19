// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host.h"

#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host_root_view.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/native_menu_host.h"
#include "views/controls/menu/submenu_view.h"
#include "views/widget/native_widget_private.h"
#include "views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHost, public:

MenuHost::MenuHost(SubmenuView* submenu)
    : submenu_(submenu),
      destroying_(false),
      showing_(false) {
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
  showing_ = true;
  Show();
  if (do_capture) {
    native_widget_private()->SetMouseCapture();
    // We do this to effectively disable window manager keyboard accelerators
    // for chromeos. Such accelerators could cause cause another window to
    // become active and confuse things.
    native_widget_private()->SetKeyboardCapture();
  }
  showing_ = false;
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
  if (native_widget_private()->HasMouseCapture())
    native_widget_private()->ReleaseMouseCapture();
  if (native_widget_private()->HasKeyboardCapture())
    native_widget_private()->ReleaseKeyboardCapture();
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
  if (destroying_ || showing_)
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
