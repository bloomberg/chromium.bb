// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "views/controls/menu/menu_host_gtk.h"

#include <gdk/gdk.h>

#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host_root_view.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

MenuHost::MenuHost(SubmenuView* submenu)
    : WidgetGtk(WidgetGtk::TYPE_POPUP),
      closed_(false),
      submenu_(submenu) {
  // TODO(sky): make sure this is needed.
  GdkModifierType current_event_mod;
  if (gtk_get_current_event_state(&current_event_mod)) {
    set_mouse_down(
        (current_event_mod & GDK_BUTTON1_MASK) ||
        (current_event_mod & GDK_BUTTON2_MASK) ||
        (current_event_mod & GDK_BUTTON3_MASK) ||
        (current_event_mod & GDK_BUTTON4_MASK) ||
        (current_event_mod & GDK_BUTTON5_MASK));
  }
}

void MenuHost::Init(gfx::NativeView parent,
                    const gfx::Rect& bounds,
                    View* contents_view,
                    bool do_capture) {
  WidgetGtk::Init(parent, bounds);
  SetContentsView(contents_view);
  // TODO(sky): see if there is some way to show without changing focus.
  Show();
  if (do_capture) {
    DoGrab();
#ifdef DEBUG_MENU
    DLOG(INFO) << "Doing capture";
#endif
  }
}

void MenuHost::Show() {
  WidgetGtk::Show();
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
  ReleaseGrab();
  WidgetGtk::Hide();
}

void MenuHost::HideWindow() {
  // Make sure we release capture before hiding.
  ReleaseGrab();
  WidgetGtk::Hide();
}

void MenuHost::ReleaseCapture() {
  ReleaseGrab();
}

RootView* MenuHost::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

void MenuHost::OnCancelMode() {
  // TODO(sky): see if there is an equivalent to this.
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
