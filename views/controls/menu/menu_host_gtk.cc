// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_gtk.h"

#include <gdk/gdk.h>

#if defined(HAVE_XINPUT2) && defined(TOUCH_UI)
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#endif

#include "views/controls/menu/native_menu_host_delegate.h"

#if defined(HAVE_XINPUT2) && defined(TOUCH_UI)
#include "views/touchui/touch_factory.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHostGtk, public:

MenuHostGtk::MenuHostGtk(internal::NativeMenuHostDelegate* delegate)
    : WidgetGtk(delegate->AsNativeWidgetDelegate()),
      did_input_grab_(false),
      delegate_(delegate) {
}

MenuHostGtk::~MenuHostGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// MenuHostGtk, NativeMenuHost implementation:

void MenuHostGtk::StartCapturing() {
  DCHECK(!did_input_grab_);

  // Release the current grab.
  GtkWidget* current_grab_window = gtk_grab_get_current();
  if (current_grab_window)
    gtk_grab_remove(current_grab_window);

  // Make sure all app mouse/keyboard events are targeted at us only.
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

NativeWidget* MenuHostGtk::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// MenuHostGtk, WidgetGtk overrides:

void MenuHostGtk::InitNativeWidget(const Widget::InitParams& params) {
  WidgetGtk::InitNativeWidget(params);
  // Make sure we get destroyed when the parent is destroyed.
  gtk_window_set_destroy_with_parent(GTK_WINDOW(GetNativeView()), TRUE);
  gtk_window_set_type_hint(GTK_WINDOW(GetNativeView()),
                           GDK_WINDOW_TYPE_HINT_MENU);
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
  delegate_->OnNativeMenuHostDestroy();
  WidgetGtk::OnDestroy(object);
}

void MenuHostGtk::HandleXGrabBroke() {
  // Grab may already be release in ReleaseGrab.
  if (did_input_grab_) {
    did_input_grab_ = false;
    delegate_->OnNativeMenuHostCancelCapture();
  }
  WidgetGtk::HandleXGrabBroke();
}

void MenuHostGtk::HandleGtkGrabBroke() {
  // Grab can be broken by drag & drop, other menu or screen locker.
  if (did_input_grab_) {
    ReleaseMouseCapture();
    delegate_->OnNativeMenuHostCancelCapture();
  }
  WidgetGtk::HandleGtkGrabBroke();
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuHost, public:

// static
NativeMenuHost* NativeMenuHost::CreateNativeMenuHost(
    internal::NativeMenuHostDelegate* delegate) {
  return new MenuHostGtk(delegate);
}

}  // namespace views
