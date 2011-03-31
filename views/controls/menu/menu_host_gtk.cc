// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_gtk.h"

#include <gdk/gdk.h>

#if defined(HAVE_XINPUT2) && defined(TOUCH_UI)
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#endif

#include "views/controls/menu/menu_controller.h"
#include "views/controls/menu/menu_host_root_view.h"
#include "views/controls/menu/menu_item_view.h"
#include "views/controls/menu/submenu_view.h"

#if defined(HAVE_XINPUT2) && defined(TOUCH_UI)
#include "views/touchui/touch_factory.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHostGtk, public:

MenuHostGtk::MenuHostGtk(SubmenuView* submenu)
    : WidgetGtk(WidgetGtk::TYPE_POPUP),
      destroying_(false),
      submenu_(submenu),
      did_input_grab_(false) {
  CreateParams params;
  params.type = CreateParams::TYPE_MENU;
  params.has_dropshadow = true;
  SetCreateParams(params);
}

MenuHostGtk::~MenuHostGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// MenuHostGtk, NativeMenuHost implementation:

void MenuHostGtk::InitMenuHost(gfx::NativeWindow parent,
                               const gfx::Rect& bounds,
                               View* contents_view,
                               bool do_capture) {
  make_transient_to_parent();
  WidgetGtk::Init(GTK_WIDGET(parent), bounds);
  // Make sure we get destroyed when the parent is destroyed.
  gtk_window_set_destroy_with_parent(GTK_WINDOW(GetNativeView()), TRUE);
  gtk_window_set_type_hint(GTK_WINDOW(GetNativeView()),
                           GDK_WINDOW_TYPE_HINT_MENU);
  SetContentsView(contents_view);
  ShowMenuHost(do_capture);
}

bool MenuHostGtk::IsMenuHostVisible() {
  return IsVisible();
}

void MenuHostGtk::ShowMenuHost(bool do_capture) {
  WidgetGtk::Show();
  if (do_capture)
    DoCapture();
}

void MenuHostGtk::HideMenuHost() {
  // Make sure we release capture before hiding.
  ReleaseMenuHostCapture();

  WidgetGtk::Hide();
}

void MenuHostGtk::DestroyMenuHost() {
  HideMenuHost();
  destroying_ = true;
  // We use Close instead of CloseNow to delay the deletion. If this invoked
  // during a key press event, gtk still generates the release event and the
  // AcceleratorHandler will use the window in the event. If we destroy the
  // window now, it means AcceleratorHandler attempts to use a window that has
  // been destroyed.
  Close();
}

void MenuHostGtk::SetMenuHostBounds(const gfx::Rect& bounds) {
  SetBounds(bounds);
}

void MenuHostGtk::ReleaseMenuHostCapture() {
  ReleaseMouseCapture();
}

gfx::NativeWindow MenuHostGtk::GetMenuHostWindow() {
  return GTK_WINDOW(GetNativeView());
}

////////////////////////////////////////////////////////////////////////////////
// MenuHostGtk, WidgetGtk overrides:

RootView* MenuHostGtk::CreateRootView() {
  return new MenuHostRootView(this, submenu_);
}

bool MenuHostGtk::ShouldReleaseCaptureOnMouseReleased() const {
  return false;
}

void MenuHostGtk::ReleaseMouseCapture() {
  WidgetGtk::ReleaseMouseCapture();
  if (did_input_grab_) {
    did_input_grab_ = false;
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
#if defined(HAVE_XINPUT2) && defined(TOUCH_UI)
    TouchFactory::GetInstance()->UngrabTouchDevices(
        GDK_WINDOW_XDISPLAY(window_contents()->window));
#endif
  }
}

void MenuHostGtk::OnDestroy(GtkWidget* object) {
  if (!destroying_) {
    // We weren't explicitly told to destroy ourselves, which means the menu was
    // deleted out from under us (the window we're parented to was closed). Tell
    // the SubmenuView to drop references to us.
    submenu_->MenuHostDestroyed();
  }
  WidgetGtk::OnDestroy(object);
}

void MenuHostGtk::HandleXGrabBroke() {
  // Grab may already be release in ReleaseGrab.
  if (did_input_grab_ && !destroying_) {
    did_input_grab_ = false;
    CancelAllIfNoDrag();
  }
  WidgetGtk::HandleXGrabBroke();
}

void MenuHostGtk::HandleGtkGrabBroke() {
  // Grab can be broken by drag & drop, other menu or screen locker.
  if (did_input_grab_ && !destroying_) {
    ReleaseMouseCapture();
    CancelAllIfNoDrag();
  }
  WidgetGtk::HandleGtkGrabBroke();
}

////////////////////////////////////////////////////////////////////////////////
// MenuHostGtk, private:

void MenuHostGtk::DoCapture() {
  DCHECK(!did_input_grab_);

  // Release the current grab.
  GtkWidget* current_grab_window = gtk_grab_get_current();
  if (current_grab_window)
    gtk_grab_remove(current_grab_window);

  // Make sure all app mouse/keyboard events are targetted at us only.
  SetMouseCapture();

  // And do a grab.  NOTE: we do this to ensure we get mouse/keyboard
  // events from other apps, a grab done with gtk_grab_add doesn't get
  // events from other apps.
  GdkGrabStatus pointer_grab_status =
      gdk_pointer_grab(window_contents()->window, FALSE,
                       static_cast<GdkEventMask>(
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_POINTER_MOTION_MASK),
                       NULL, NULL, GDK_CURRENT_TIME);
  GdkGrabStatus keyboard_grab_status =
      gdk_keyboard_grab(window_contents()->window, FALSE,
                        GDK_CURRENT_TIME);

  did_input_grab_ = pointer_grab_status == GDK_GRAB_SUCCESS &&
      keyboard_grab_status == GDK_GRAB_SUCCESS;

  DCHECK_EQ(GDK_GRAB_SUCCESS, pointer_grab_status);
  DCHECK_EQ(GDK_GRAB_SUCCESS, keyboard_grab_status);

#if defined(HAVE_XINPUT2) && defined(TOUCH_UI)
  ::Window window = GDK_WINDOW_XID(window_contents()->window);
  Display* display = GDK_WINDOW_XDISPLAY(window_contents()->window);
  bool xi2grab = TouchFactory::GetInstance()->GrabTouchDevices(display, window);
  did_input_grab_ = did_input_grab_ && xi2grab;
#endif

  DCHECK(did_input_grab_);
  // need keyboard grab.
}

void MenuHostGtk::CancelAllIfNoDrag() {
  MenuController* menu_controller =
      submenu_->GetMenuItem()->GetMenuController();
  if (menu_controller &&
      !menu_controller->drag_in_progress())
    menu_controller->CancelAll();
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuHost, public:

// static
NativeMenuHost* NativeMenuHost::CreateNativeMenuHost(SubmenuView* submenu) {
  return new MenuHostGtk(submenu);
}

}  // namespace views
