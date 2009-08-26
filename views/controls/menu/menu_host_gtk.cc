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
      submenu_(submenu),
      did_pointer_grab_(false) {
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
    // Release the current grab.
    GtkWidget* current_grab_window = gtk_grab_get_current();
    if (current_grab_window)
      gtk_grab_remove(current_grab_window);

    // Make sure all app mouse events are targetted at us only.
    DoGrab();

    // And do a grab.
    // NOTE: we do this to ensure we get mouse events from other apps, a grab
    // done with gtk_grab_add doesn't get events from other apps.
    GdkGrabStatus grab_status =
        gdk_pointer_grab(window_contents()->window, FALSE,
                         static_cast<GdkEventMask>(
                             GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                             GDK_POINTER_MOTION_MASK),
                         NULL, NULL, GDK_CURRENT_TIME);
    did_pointer_grab_ = (grab_status == GDK_GRAB_SUCCESS);
    DCHECK(did_pointer_grab_);
    // need keyboard grab.
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
  ReleaseGrab();
  closed_ = true;
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

gboolean MenuHost::OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event) {
  if (!closed_)
    submenu_->GetMenuItem()->GetMenuController()->Cancel(true);
  return WidgetGtk::OnGrabBrokeEvent(widget, event);
}

void MenuHost::OnGrabNotify(GtkWidget* widget, gboolean was_grabbed) {
  if (!closed_ && (widget != window_contents() || !was_grabbed))
    submenu_->GetMenuItem()->GetMenuController()->Cancel(true);
  WidgetGtk::OnGrabNotify(widget, was_grabbed);
}

// Overriden to return false, we do NOT want to release capture on mouse
// release.
bool MenuHost::ReleaseCaptureOnMouseReleased() {
  return false;
}

void MenuHost::ReleaseGrab() {
  WidgetGtk::ReleaseGrab();
  if (did_pointer_grab_) {
    did_pointer_grab_ = false;
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
  }
}

}  // namespace views
