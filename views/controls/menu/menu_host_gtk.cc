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
  GdkEvent* event = gtk_get_current_event();
  if (event) {
    if (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS ||
        event->type == GDK_3BUTTON_PRESS) {
      set_mouse_down(true);
    }
    gdk_event_free(event);
  }
}

void MenuHost::Init(gfx::NativeWindow parent,
                    const gfx::Rect& bounds,
                    View* contents_view,
                    bool do_capture) {
  WidgetGtk::Init(GTK_WIDGET(parent), bounds);
  SetContentsView(contents_view);
  Show();
  if (do_capture)
    DoCapture();
}

gfx::NativeWindow MenuHost::GetNativeWindow() {
  return GTK_WINDOW(GetNativeView());
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

void MenuHost::DoCapture() {
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

void MenuHost::ReleaseCapture() {
  ReleaseGrab();
}

RootView* MenuHost::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

gboolean MenuHost::OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event) {
  // Grab breaking only happens when drag and drop starts. So, we don't try
  // and ungrab or cancel the menu.
  did_pointer_grab_ = false;
  return WidgetGtk::OnGrabBrokeEvent(widget, event);
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
