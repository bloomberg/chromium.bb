// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_win.h"

#include "base/win/windows_version.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host_root_view.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

// static
MenuHost* MenuHost::Create(SubmenuView* submenu_view) {
  return new MenuHostWin(submenu_view);
}

MenuHostWin::MenuHostWin(SubmenuView* submenu)
    : destroying_(false),
      submenu_(submenu),
      owns_capture_(false) {
  set_window_style(WS_POPUP);
  set_initial_class_style(
      (base::win::GetVersion() < base::win::VERSION_XP) ?
      0 : CS_DROPSHADOW);
  is_mouse_down_ =
      ((GetKeyState(VK_LBUTTON) & 0x80) ||
       (GetKeyState(VK_RBUTTON) & 0x80) ||
       (GetKeyState(VK_MBUTTON) & 0x80) ||
       (GetKeyState(VK_XBUTTON1) & 0x80) ||
       (GetKeyState(VK_XBUTTON2) & 0x80));
  // Mouse clicks shouldn't give us focus.
  set_window_ex_style(WS_EX_TOPMOST | WS_EX_NOACTIVATE);
}

MenuHostWin::~MenuHostWin() {
}

void MenuHostWin::InitMenuHost(HWND parent,
                               const gfx::Rect& bounds,
                               View* contents_view,
                               bool do_capture) {
  WidgetWin::Init(parent, bounds);
  SetContentsView(contents_view);
  ShowMenuHost(do_capture);
}

bool MenuHostWin::IsMenuHostVisible() {
  return IsVisible();
}

void MenuHostWin::ShowMenuHost(bool do_capture) {
  // We don't want to take focus away from the hosting window.
  ShowWindow(SW_SHOWNA);

  if (do_capture)
    DoCapture();
}

void MenuHostWin::HideMenuHost() {
  // Make sure we release capture before hiding.
  ReleaseMenuHostCapture();

  WidgetWin::Hide();
}

void MenuHostWin::DestroyMenuHost() {
  HideMenuHost();
  destroying_ = true;
  CloseNow();
}

void MenuHostWin::SetMenuHostBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void MenuHostWin::ReleaseMenuHostCapture() {
  if (owns_capture_) {
    owns_capture_ = false;
    ::ReleaseCapture();
  }
}

gfx::NativeWindow MenuHostWin::GetMenuHostWindow() {
  return GetNativeView();
}

void MenuHostWin::OnDestroy() {
  if (!destroying_) {
    // We weren't explicitly told to destroy ourselves, which means the menu was
    // deleted out from under us (the window we're parented to was closed). Tell
    // the SubmenuView to drop references to us.
    submenu_->MenuHostDestroyed();
  }
  WidgetWin::OnDestroy();
}

void MenuHostWin::OnCaptureChanged(HWND hwnd) {
  WidgetWin::OnCaptureChanged(hwnd);
  owns_capture_ = false;
}

void MenuHostWin::OnCancelMode() {
  submenu_->GetMenuItem()->GetMenuController()->Cancel(
      MenuController::EXIT_ALL);
}

RootView* MenuHostWin::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

bool MenuHostWin::ReleaseCaptureOnMouseReleased() {
  return false;
}

void MenuHostWin::DoCapture() {
  owns_capture_ = true;
  SetCapture();
  has_capture_ = true;
}

}  // namespace views
