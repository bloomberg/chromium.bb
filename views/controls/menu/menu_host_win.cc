// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_win.h"

#include "base/win_util.h"
#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host_root_view.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

MenuHost::MenuHost(SubmenuView* submenu)
    : closed_(false),
      submenu_(submenu),
      owns_capture_(false) {
  set_window_style(WS_POPUP);
  set_initial_class_style(
      (win_util::GetWinVersion() < win_util::WINVERSION_XP) ?
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

void MenuHost::Init(HWND parent,
                    const gfx::Rect& bounds,
                    View* contents_view,
                    bool do_capture) {
  WidgetWin::Init(parent, bounds);
  SetContentsView(contents_view);
  Show();
  if (do_capture)
    DoCapture();
}

void MenuHost::Show() {
  // We don't want to take focus away from the hosting window.
  ShowWindow(SW_SHOWNA);
}

void MenuHost::Hide() {
  if (closed_) {
    // We're already closed, nothing to do.
    // This is invoked twice if the first time just hid us, and the second
    // time deleted Closed (deleted) us.
    return;
  }
  // The menus are freed separately, and possibly before the window is closed,
  // remove them so that View doesn't try to access deleted objects.
  static_cast<MenuHostRootView*>(GetRootView())->suspend_events();
  GetRootView()->RemoveAllChildViews(false);
  closed_ = true;
  ReleaseCapture();
  WidgetWin::Hide();
}

void MenuHost::HideWindow() {
  // Make sure we release capture before hiding.
  ReleaseCapture();
  WidgetWin::Hide();
}

void MenuHost::OnCaptureChanged(HWND hwnd) {
  WidgetWin::OnCaptureChanged(hwnd);
  owns_capture_ = false;
#ifdef DEBUG_MENU
  DLOG(INFO) << "Capture changed";
#endif
}

void MenuHost::DoCapture() {
  owns_capture_ = true;
  SetCapture();
  has_capture_ = true;
#ifdef DEBUG_MENU
  DLOG(INFO) << "Doing capture";
#endif
}

void MenuHost::ReleaseCapture() {
  if (owns_capture_) {
#ifdef DEBUG_MENU
    DLOG(INFO) << "released capture";
#endif
    owns_capture_ = false;
    ::ReleaseCapture();
  }
}

RootView* MenuHost::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

void MenuHost::OnCancelMode() {
  if (!closed_) {
#ifdef DEBUG_MENU
    DLOG(INFO) << "OnCanceMode, closing menu";
#endif
    submenu_->GetMenuItem()->GetMenuController()->Cancel(true);
  }
}

// Overriden to return false, we do NOT want to release capture on mouse
// release.
bool MenuHost::ReleaseCaptureOnMouseReleased() {
  return false;
}

}  // namespace views
